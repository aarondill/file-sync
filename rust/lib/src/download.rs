use std::path::Path;

use tokio::fs;
use tokio::io::{self, AsyncRead, AsyncReadExt};
use tokio::net::TcpStream;

use crate::download_file::DownloadFile;
use crate::download_message::DownloadMessage;
use crate::download_response::{self, DownloadResponse};
use crate::file_hash::FileHash;
use crate::file_info::FileInfo;
use crate::protocol::{read_message, write_message};
use crate::serial::{Deserialize, Serialize};

// if destdir is non-null, the file contents will be read and written to
// disk
async fn read_file_list(
    socket: &mut (dyn AsyncRead + Unpin),
    file_count: u8,
    destdir: Option<&Path>,
) -> Result<Vec<FileInfo>, Box<dyn std::error::Error>> {
    // recv the file info
    let mut list = Vec::<FileInfo>::with_capacity(file_count as usize);
    for _ in 0..file_count {
        let msg = read_message(socket).await?;
        let file = DownloadFile::deserialize(&mut &msg[..])?;
        list.push(file.into());
    }
    let list = list;

    // recv/write the file contents
    if let Some(destdir) = destdir {
        for f in &list {
            let path = destdir.join(f.path());
            fs::create_dir_all(path.parent().unwrap()).await?;
            let mut file = fs::File::create(&path).await?;
            io::copy(&mut socket.take(f.size()), &mut file).await?;
            // TODO: verify hash
        }
    }

    Ok(list)
}

// if destdir is NULL, the files will not be read from the message (ie. the
// uploader must not send the file contents)
async fn read_download_message(
    socket: &mut (dyn AsyncRead + Unpin),
    destdir: Option<&Path>,
) -> Result<Vec<FileInfo>, Box<dyn std::error::Error>> {
    let msg = read_message(socket).await?;
    let msg = DownloadMessage::deserialize(&mut &msg[..])?;
    return read_file_list(socket, msg.file_count(), destdir).await;
}

pub async fn download(
    socket: &mut TcpStream,
    files: &mut Vec<FileInfo>,
    srcdir: &Path,
) -> Result<(), Box<dyn std::error::Error>> {
    //  read the download message
    let mut recvlist = read_download_message(socket, None).await?;
    let to_delete = files
        .iter()
        .filter(|f| recvlist.iter().find(|o| o.path() == f.path()).is_none())
        .collect::<Vec<_>>();
    // filter the recv list to exclude anything that we already have; keep only the files that we need
    recvlist
        .retain(|f| files.iter().find(|o| o.path() == f.path() && o.hash() == f.hash()).is_none());
    {
        // send download response
        assert!(recvlist.len() <= 255); // this is a protocol limit
        let hashes: Vec<FileHash> = recvlist.iter().map(|f| f.hash()).cloned().collect();
        let resp = DownloadResponse::new(download_response::Flags::empty(), hashes);
        let mut buf = Vec::with_capacity(4096);
        resp.serialize(&mut buf).expect("error serializing download response");
        write_message(socket, &buf).await?;
    }
    // read download message 2
    read_download_message(socket, Some(srcdir)).await?;
    // delete files that we don't need anymore
    for node in to_delete {
        let mut path = srcdir.join(node.path());
        println!("deleting {}", path.display());
        // remove the file and parent directories
        fs::remove_file(&path).await?;
        while path.pop() && path != Path::new("") {
            match fs::remove_dir(&path).await {
                Ok(_) => break,
                Err(e) if e.kind() == io::ErrorKind::NotFound => break,
                Err(e) if e.kind() == io::ErrorKind::DirectoryNotEmpty => break,
                Err(e) => return Err(e.into()),
            }
        }
    }
    Ok(())
}
