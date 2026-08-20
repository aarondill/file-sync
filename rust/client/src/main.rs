// gethostname is not stable yet, i could use a crate, but i'd rather use the nightly feature
#![feature(gethostname)]

use std::error::Error;
use std::io::Read;
use std::path::Path;
use std::process::ExitCode;

use anyhow::{Context, Result};
use futures::StreamExt;
use lib::client_connect::{self, ClientConnect};
use lib::download::download;
use lib::file_info::FileInfo;
use lib::protocol::write_message;
use lib::serial::Serialize;
use lib::upload::upload;
use lib::util::check_readable;
use lib::variable_length_string::VariableLengthString;
use tokio::net::TcpStream;
use tokio::sync::mpsc;
use tokio::{io, pin};

// Initialize a client connect message
fn init_connect_msg(upload: bool) -> ClientConnect {
    let name = std::net::hostname().unwrap_or_else(|_| "unknown".into());
    let flags =
        if upload { client_connect::Flags::IntentToUpload } else { client_connect::Flags::empty() };
    let name = VariableLengthString::new_truncate(name.to_string_lossy().as_bytes());
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
        eprintln!("usage: {} <server ip> <directory> [-u]", std::env::args().next().unwrap());
        return ExitCode::from(2);
    }
    let server = &args[0];
    let directory = Path::new(&args[1]);
    if !directory.is_dir() || directory.metadata().map_or(true, |m| m.permissions().readonly()) {
        eprintln!("directory is not readable or writable");
        return ExitCode::from(2);
    }

    process(server, directory, should_upload).await.unwrap();
    ExitCode::SUCCESS
}
async fn process(
    server: &str,
    directory: &Path,
    should_upload: bool,
) -> Result<(), Box<dyn Error>> {
    let mut global_list = Vec::<FileInfo>::new();
    let (tx_stop, rx_stop) = tokio::sync::watch::channel(false);

    // The server starts by sending an upload to the client unless the client
    // explicitly requests otherwise
    let mut upload_pending = should_upload;

    update_list(directory, &mut global_list).await; // update the list before starting the server

    let addr = server
        .rsplit_once(":")
        .map(|(host, port)| (host, port.parse().unwrap()))
        .unwrap_or((server, 8080));
    let mut connection = TcpStream::connect(addr).await.context("error connecting to server")?;

    // send connect message
    {
        let msg = init_connect_msg(should_upload);
        let mut buf = Vec::with_capacity(4096);
        msg.serialize(&mut buf)
            .map_err(anyhow::Error::msg)
            .context("error serializing client connect message")?;
        write_message(&mut connection, &buf).await.context("error writing connect message")?;
    }

    let (send, mut recv) = mpsc::channel(32);
    std::thread::spawn(move || {
        // Handle stdin input
        let handle = std::io::stdin().lock();
        for c in handle.bytes() {
            let Ok(c) = c else {
                break;
            };
            if c.is_ascii_whitespace() {
                continue;
            }
            send.blocking_send(c as char).unwrap();
        }
    });

    {
        let tx_stop = tx_stop.clone();
        tokio::spawn(async move {
            // handle ctrl-c
            tokio::signal::ctrl_c().await.expect("error handling ctrl-c");
            tx_stop.send(true).unwrap();
        });
    }

    while !rx_stop.has_changed().unwrap() {
        enum SelectState {
            Command(char),
            Downloading,
            Uploading,
        }
        let state = if upload_pending {
            SelectState::Uploading
        } else {
            tokio::select! {
                recv = recv.recv() => { // user input
                    let Some(c) = recv else {
                        return Err("connection closed".into());
                    };
                    SelectState::Command(c)
                },
                r = connection.readable() => {
                    if let Err(e) = r {
                        return Err(format!("error reading from connection: {}", e).into());
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
                download(&mut read, &mut write, &global_list, directory)
                    .await
                    .expect("download failed");
                update_list(directory, &mut global_list).await;
            }
            SelectState::Uploading => {
                let mut buf = [0];
                match connection.try_read(&mut buf) {
                    Err(e) if e.kind() == io::ErrorKind::WouldBlock => {} // ok
                    Ok(0) => return Err("connection closed".into()),
                    Ok(_) => return Err("upload pending while connection has data!".into()),
                    Err(e) => return Err(format!("error reading from connection: {}", e).into()),
                };
                update_list(directory, &mut global_list).await; // files may change between downloads
                let (mut read, mut write) = connection.split();
                upload(&mut read, &mut write, &global_list, directory)
                    .await
                    .expect("upload failed");
            }
            SelectState::Command(c) => match c {
                'q' => {
                    tx_stop.send(true).unwrap();
                    continue;
                }
                'u' => upload_pending = true, // the upload will happen on the next loop
                'h' => {
                    println!("commands: ");
                    println!("  q: quit");
                    println!("  u: upload");
                    println!("  h: help");
                    println!();
                    continue;
                }
                _ => return Err("unknown command".into()),
            },
        };
    }
    return Ok(());
}
