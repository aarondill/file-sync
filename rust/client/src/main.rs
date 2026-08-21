// gethostname is not stable yet, i could use a crate, but i'd rather use the nightly feature
#![feature(gethostname)]

use std::io::Read;
use std::path::Path;
use std::process::ExitCode;

use anyhow::{Context, Result, bail};
use futures::StreamExt;
use lib::common::{download, upload};
use lib::file_info::FileInfo;
use lib::io::{check_readable, write_message};
use lib::protocol::{ClientConnect, ProtocolString, client_connect};
use lib::serial::Serialize;
use tokio::net::TcpStream;
use tokio::pin;
use tokio::sync::mpsc;

// Initialize a client connect message
fn init_connect_msg(upload: bool) -> ClientConnect {
    let name = std::net::hostname().unwrap_or_else(|_| "unknown".into());
    let flags =
        if upload { client_connect::Flags::IntentToUpload } else { client_connect::Flags::empty() };
    let name =
        ProtocolString::new_truncate(&name.to_string_lossy()).expect("hostname is valid ascii");
    ClientConnect::new(flags, name)
}

async fn update_list(directory: &Path, list: &mut Vec<FileInfo>) {
    list.clear();
    let stream = FileInfo::read_list(directory);
    pin!(stream);
    while let Some(info) = stream.next().await {
        list.push(info);
    }
}

fn parse_args(iter: impl Iterator<Item = String>) -> (Vec<String>, Vec<String>) {
    let mut positional = Vec::new();
    let mut opts = Vec::new();

    let mut iter = iter.skip(1);
    for arg in iter.by_ref().take_while(|arg| arg != "--") {
        // parse until "--" is reached
        if arg.starts_with("-") {
            opts.push(arg);
        } else {
            positional.push(arg);
        }
    }
    positional.extend(iter);
    (positional, opts)
}

#[tokio::main]
async fn main() -> ExitCode {
    let (args, opts) = parse_args(std::env::args());
    let should_upload = opts.contains(&"-u".to_string());
    if args.len() != 2 {
        eprintln!(
            "usage: {} <server ip> <directory> [-u]",
            std::env::args().next().expect("argv[0] is not set")
        );
        return ExitCode::from(2);
    }
    let server = &args[0];
    let directory = Path::new(&args[1]);
    if !directory.is_dir() || directory.metadata().map_or(true, |m| m.permissions().readonly()) {
        eprintln!("directory is not readable or writable");
        return ExitCode::from(3);
    }

    match process(server, directory, should_upload).await {
        Ok(_) => ExitCode::SUCCESS,
        Err(e) => {
            eprintln!("error: {}", e);
            ExitCode::FAILURE
        }
    }
}
async fn process(server: &str, directory: &Path, should_upload: bool) -> Result<()> {
    let mut global_list = Vec::<FileInfo>::new();
    let (tx_stop, mut rx_stop) = tokio::sync::watch::channel(false);

    // The server starts by sending an upload to the client unless the client
    // explicitly requests otherwise
    let mut upload_pending = should_upload;

    update_list(directory, &mut global_list).await; // update the list before starting the server

    let addr = server
        .rsplit_once(":")
        .map(|(host, port)| port.parse().context("invalid port").map(|p| (host, p)))
        .transpose()?
        .unwrap_or((server, 8080));
    let mut connection = TcpStream::connect(addr).await.context("error connecting to server")?;

    // send connect message
    {
        let msg = init_connect_msg(should_upload);
        let mut buf = Vec::with_capacity(4096);
        msg.serialize(&mut buf).context("error serializing client connect message")?;
        write_message(&mut connection, &buf).await.context("error writing connect message")?;
    }

    let (send, mut recv) = mpsc::channel(32);
    std::thread::spawn(move || {
        // Handle stdin input
        let handle = std::io::stdin().lock();
        for c in handle.bytes() {
            match c {
                Err(_) => break,
                Ok(c) if c.is_ascii_whitespace() => continue,
                Ok(c) => send.blocking_send(c as char).unwrap(),
            }
        }
    });

    {
        let tx_stop = tx_stop.clone();
        tokio::spawn(async move {
            // handle ctrl-c
            tokio::signal::ctrl_c().await.expect("failed to listen for ctrl-c");
            tx_stop.send(true).unwrap();
        });
    }

    loop {
        enum SelectState {
            Command(char),
            Downloading,
            Uploading,
        }
        let state = if upload_pending {
            upload_pending = false;
            SelectState::Uploading
        } else {
            tokio::select! {
                _ = rx_stop.changed() => break,
                recv = recv.recv() => { // user input
                    let Some(c) = recv else { bail!("connection closed") };
                    SelectState::Command(c)
                },
                r = connection.readable() => {
                    if let Err(e) = r {
                        bail!( "error reading from connection: {}", e);
                    }
                    SelectState::Downloading
                }
            }
        };

        match state {
            SelectState::Downloading => {
                let (read, mut write) = connection.split();
                let mut read = match check_readable(read)? {
                    None => continue, // false positive
                    Some(r) => r,
                };

                println!("downloading");
                download(&mut read, &mut write, &global_list, directory)
                    .await
                    .context("download failed")?;
                update_list(directory, &mut global_list).await;
            }
            SelectState::Uploading => {
                match check_readable(connection.split().0)? {
                    Some(_) => bail!("upload pending while connection has data!"),
                    None => {}
                }
                println!("uploading");
                update_list(directory, &mut global_list).await; // files may change between downloads
                let (mut read, mut write) = connection.split();
                upload(&mut read, &mut write, &global_list, directory)
                    .await
                    .context("upload failed")?;
            }
            SelectState::Command(c) => match c {
                'q' => {
                    tx_stop.send(true).unwrap();
                }
                'u' => upload_pending = true, // the upload will happen on the next loop
                'h' => {
                    eprintln!("commands: ");
                    eprintln!("  q: quit");
                    eprintln!("  u: upload");
                    eprintln!("  h: help");
                    eprintln!();
                }
                _ => {
                    eprintln!("unknown command");
                }
            },
        };
    }
    return Ok(());
}
