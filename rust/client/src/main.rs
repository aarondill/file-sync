// gethostname is not stable yet, i could use a crate, but i'd rather use the nightly feature
#![feature(gethostname)]

use std::collections::HashMap;
use std::io::Read;
use std::path::Path;
use std::process::ExitCode;
use std::sync::atomic::AtomicBool;

use futures::StreamExt;
use lib::client_connect::{self, ClientConnect};
use lib::download::download;
use lib::file_info::FileInfo;
use lib::protocol::write_message;
use lib::serial::Serialize;
use lib::upload::upload;
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
    let mut global_list = Vec::<FileInfo>::new();
    // TODO: ctrl-c
    let stop = AtomicBool::new(false);

    let (args, opts) = parse_args(std::env::args());
    let should_upload = opts.contains(&"-u".to_string());
    if args.len() != 2 {
        eprintln!("usage: {} <server ip> <directory> [-u]", args[0]);
        return ExitCode::from(2);
    }
    let server = &args[1];
    let directory = Path::new(&args[2]);
    if !directory.is_dir() || directory.metadata().map_or(true, |m| m.permissions().readonly()) {
        eprintln!("directory is not readable or writable");
        return ExitCode::from(2);
    }

    // The server starts by sending an upload to the client unless the client
    // explicitly requests otherwise
    let upload_pending = AtomicBool::new(should_upload);

    update_list(directory, &mut global_list).await; // update the list before starting the server

    let addr = server
        .rsplit_once(":")
        .map(|(host, port)| (host, port.parse().unwrap()))
        .unwrap_or((server, 8080));
    let mut connection = TcpStream::connect(addr).await.expect("error connecting to server");

    // send connect message
    {
        let msg = init_connect_msg(should_upload);
        let mut buf = Vec::with_capacity(4096);
        msg.serialize(&mut buf).expect("error serializing client connect message");
        write_message(&mut connection, &buf).await.expect("error writing connect message");
    }

    let mut commands: HashMap<char, Box<dyn FnMut() + Send>> = HashMap::new();
    commands.insert(
        'q',
        Box::new(|| {
            stop.store(true, std::sync::atomic::Ordering::SeqCst);
        }),
    );
    commands
        .insert('u', Box::new(|| upload_pending.store(true, std::sync::atomic::Ordering::SeqCst)));
    commands.insert(
        'h',
        Box::new(|| {
            println!("commands: ");
            println!("  q: quit");
            println!("  u: upload");
            println!("  h: help");
            println!();
        }),
    );

    let (send, mut recv) = mpsc::channel(32);
    std::thread::spawn(move || {
        // Handle stdin input
        let handle = std::io::stdin().lock();
        for c in handle.bytes() {
            let Ok(c) = c else {
                break;
            };
            send.blocking_send(c).unwrap();
        }
    });

    while !stop.load(std::sync::atomic::Ordering::SeqCst) {
        if !upload_pending.load(std::sync::atomic::Ordering::SeqCst) {
            tokio::select! {
                recv = recv.recv() => { // user input
                    let Some(c) = recv else {
                        eprintln!("connection closed");
                        return ExitCode::FAILURE;
                    };
                    let c = c as char;
                    if c.is_ascii_whitespace() {
                        continue;
                    }
                    match commands.get_mut(&c) {
                        Some(f) => f(),
                        None => eprintln!("unknown command: {}", c),
                    }
                }
                r = connection.readable() => {
                    if let Err(e) = r {
                        eprintln!("error reading from connection: {}", e);
                        return ExitCode::FAILURE;
                    }
                    download(&mut connection, &global_list, directory).await.expect("download failed");
                    update_list(directory, &mut global_list).await;
                }
            }
        }

        if upload_pending.load(std::sync::atomic::Ordering::SeqCst) {
            let mut buf = [0];
            match connection.try_read(&mut buf) {
                Err(e) if e.kind() == io::ErrorKind::WouldBlock => {} // ok
                Ok(0) => {
                    eprintln!("connection closed");
                    return ExitCode::FAILURE;
                }
                Ok(_) => {
                    eprintln!("upload pending while connection has data!");
                    return ExitCode::FAILURE;
                }
                Err(e) => {
                    eprintln!("error reading from connection: {}", e);
                    return ExitCode::FAILURE;
                }
            };
            update_list(directory, &mut global_list).await; // files may change between downloads
            upload(&mut connection, &global_list, directory).await.expect("upload failed");
            upload_pending.store(false, std::sync::atomic::Ordering::SeqCst);
        }
    }
    return ExitCode::SUCCESS;
}
