use core::panic;
use std::path::{Path, PathBuf};
use std::process::ExitCode;
use std::sync::Arc;

use anyhow::{Context, Result, bail};
use futures::StreamExt;
use lib::client_connect::{self, ClientConnect};
use lib::download::download;
use lib::file_info::FileInfo;
use lib::protocol::read_message;
use lib::serial::Deserialize;
use lib::upload::upload;
use lib::util::check_readable;
use tokio::net::{TcpListener, TcpStream};
use tokio::sync::{RwLock, broadcast};
use tokio::{io, pin};
use tokio_util::sync::CancellationToken;
use tokio_util::task::TaskTracker;

async fn update_list(directory: &Path, list: &mut Vec<FileInfo>) {
    list.clear();
    let stream = FileInfo::read_list(directory);
    pin!(stream);
    while let Some(info) = stream.next().await {
        list.push(info);
    }
}

async fn handle_client(
    mut connection: TcpStream,
    directory: &Path,
    global_list_lock: Arc<RwLock<Vec<FileInfo>>>,
    token: CancellationToken,
    send_upload: broadcast::Sender<()>,
) -> Result<()> {
    // recv connect message
    let msg = read_message(&mut connection).await.context("error reading connect message")?;
    let msg = ClientConnect::deserialize(&mut &msg[..])?;
    // we upload unless the client wants to upload
    let mut initial_upload = !msg.flags().contains(client_connect::Flags::IntentToUpload);

    let mut upload_pending = send_upload.subscribe();
    loop {
        // Use enum because autocomplete doesn't work with tokio::select!
        enum SelectState {
            Upload,
            Download,
        }
        let state = if initial_upload {
            initial_upload = false;
            SelectState::Upload
        } else {
            tokio::select! {
                biased; // check top-to-bottom
                // TODO: better handle cancelation token
                _ = token.cancelled() => break,
                r = upload_pending.recv() => {
                    match r {
                        Ok(_) => SelectState::Upload,
                        Err(broadcast::error::RecvError::Lagged(n)) => { // lag is fine, the messages are indistinguishable
                            eprintln!("upload pending lagged {} times", n);
                            SelectState::Upload
                        }
                        Err(broadcast::error::RecvError::Closed) => panic!("somehow send closed (should be impossible)"),
                    }
                }
                r = connection.readable() => {
                     r.context("error reading from connection")?;
                    SelectState::Download
                }
            }
        };

        match state {
            SelectState::Download => {
                let mut global_list = global_list_lock.write().await;

                let (read, mut write) = connection.split();
                let mut read = match check_readable(read)? {
                    None => continue, // false positive
                    Some(r) => r,
                };

                download(&mut read, &mut write, &global_list, directory)
                    .await
                    .context("error downloading")?;
                update_list(directory, &mut global_list).await;
                send_upload.send(()).unwrap(); // tell other tasks to upload
                upload_pending.recv().await.unwrap(); // ignore our own upload
            }
            SelectState::Upload => {
                // grab read lock before emptying the upload pending channel
                // this makes it impossible for the upload to be triggered while we are reading
                // (since it needs to grab the write lock)
                let global_list = global_list_lock.read().await;
                loop {
                    match upload_pending.try_recv() {
                        Ok(_) => {} // empty the upload pending channel
                        Err(broadcast::error::TryRecvError::Lagged(_)) => {}
                        Err(broadcast::error::TryRecvError::Empty) => break,
                        Err(broadcast::error::TryRecvError::Closed) => {
                            panic!("somehow send closed (should be impossible)")
                        }
                    }
                }
                let mut buf = [0];
                match connection.try_read(&mut buf) {
                    Err(e) if e.kind() == io::ErrorKind::WouldBlock => {} // ok
                    Ok(0) => bail!("connection closed"),
                    Ok(_) => bail!("upload pending while connection has data!"), // client is sending us data
                    Err(e) => Err(e).context("error reading from connection")?,
                };
                let (mut read, mut write) = connection.split();
                upload(&mut read, &mut write, &global_list, directory)
                    .await
                    .context("upload failed")?;
            }
        }
    }

    Ok(())
}

#[tokio::main]
async fn main() -> ExitCode {
    let global_list_lock = Arc::new(RwLock::new(Vec::<FileInfo>::new()));

    let args: Vec<_> = std::env::args().skip(1).collect();
    if args.len() > 2 || args.is_empty() {
        eprintln!(
            "usage: {} <directory> [port]",
            std::env::args().next().expect("argv[0] not set")
        );
        return ExitCode::from(2);
    }
    let directory = PathBuf::from(&args[0]);
    let port = args.get(1).map_or(8080, |s| s.parse().unwrap());

    if !directory.is_dir() || directory.metadata().map_or(true, |m| m.permissions().readonly()) {
        eprintln!("directory is not readable or writable");
        return ExitCode::from(3);
    }

    {
        // update the list before starting the server
        let mut global_list = global_list_lock.write().await;
        update_list(&directory, &mut global_list).await;
    }

    let listener = TcpListener::bind(("127.0.0.1", port)).await.expect("error binding to server");

    let tasks = TaskTracker::new();
    let token = CancellationToken::new();

    {
        let token = token.clone();
        tokio::spawn(async move {
            // handle ctrl-c
            tokio::signal::ctrl_c().await.expect("error handling ctrl-c");
            token.cancel();
        });
    }

    // A channel to coordinate uploads
    let (tx, _) = broadcast::channel::<()>(10);

    loop {
        let info = tokio::select! { biased;
            _ = token.cancelled() => break,
            info = listener.accept() => info,
        };
        let socket = match info {
            Ok((stream, _)) => stream,
            Err(e) => {
                eprintln!("error accepting connection: {}", e);
                continue;
            }
        };
        let global_list_lock = global_list_lock.clone();
        let token = token.clone();
        let tx = tx.clone();
        let directory = directory.clone();
        let fut = async move {
            let _ = handle_client(socket, &directory, global_list_lock, token, tx)
                .await
                .map_err(|e| eprintln!("error handling client: {}", e));
        };
        tasks.spawn(Box::pin(fut));
    }
    tasks.close();
    tasks.wait().await; // wait for all tasks to finish

    return ExitCode::SUCCESS;
}
