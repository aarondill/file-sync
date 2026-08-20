use std::path::Path;

use tokio::fs;
use tokio::io::{self, AsyncRead, AsyncReadExt, AsyncWrite};

use crate::download_file::DownloadFile;
use crate::download_message::{DownloadMessage, MessageDeserializeError};
use crate::download_response::{self, DownloadResponse};
use crate::file_hash::FileHash;
use crate::file_info::FileInfo;
use crate::protocol::{ProtocolError, read_message, write_message};
use crate::serial::{Deserialize, Serialize};

// if destdir is non-null, the file contents will be read and written to
// disk
async fn read_file_list(
    socket: &mut (dyn AsyncRead + Unpin + Send),
    file_count: u8,
    destdir: Option<&Path>,
) -> Result<Vec<FileInfo>, std::io::Error> {
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

#[derive(Debug, thiserror::Error)]
pub enum DownloadError {
    #[error(transparent)]
    Io(#[from] std::io::Error),
    #[error("deserialization error: {0}")]
    Deserialization(MessageDeserializeError),
    #[error("protocol error: {0}")]
    Protocol(ProtocolError),
}
impl From<MessageDeserializeError> for DownloadError {
    fn from(e: MessageDeserializeError) -> Self {
        match e {
            MessageDeserializeError::Io(e) => Self::Io(e),
            _ => Self::Deserialization(e),
        }
    }
}
impl From<ProtocolError> for DownloadError {
    fn from(e: ProtocolError) -> Self {
        match e {
            ProtocolError::Io(e) => Self::Io(e),
            _ => Self::Protocol(e),
        }
    }
}

// if destdir is NULL, the files will not be read from the message (ie. the
// uploader must not send the file contents)
async fn read_download_message(
    socket: &mut (dyn AsyncRead + Unpin + Send),
    destdir: Option<&Path>,
) -> Result<Vec<FileInfo>, DownloadError> {
    let msg = read_message(socket).await?;
    let msg = DownloadMessage::deserialize(&mut &msg[..])?;
    return Ok(read_file_list(socket, msg.file_count(), destdir).await?);
}

pub async fn download(
    read: &mut (dyn AsyncRead + Unpin + Send),
    write: &mut (dyn AsyncWrite + Unpin + Send),
    files: &Vec<FileInfo>,
    srcdir: &Path,
) -> Result<(), DownloadError> {
    //  read the download message
    let mut recvlist = read_download_message(read, None).await?;
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
        resp.serialize(&mut buf)?;
        write_message(write, &buf).await?;
    }
    // read download message 2
    read_download_message(read, Some(srcdir)).await?;
    // delete files that we don't need anymore
    for node in to_delete {
        let mut path = srcdir.join(node.path());
        println!("deleting {}", path.display());
        // remove the file and parent directories
        fs::remove_file(&path).await?;
        while path.pop() && path != srcdir && !path.is_empty() {
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
