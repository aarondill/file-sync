use std::error::Error;
use std::path::Path;

use tokio::fs;
use tokio::io::{self, AsyncRead, AsyncReadExt, AsyncWrite};

use crate::file_info::FileInfo;
use crate::io::{ProtocolError, read_message, write_message};
use crate::md5::Hash;
use crate::protocol::{
    DownloadFile, DownloadMessage, DownloadResponse, download_response,
};
use crate::serial::{Deserialize, Serialize, from_infallible};

#[derive(Debug, thiserror::Error)]
pub enum DownloadError {
    #[error(transparent)]
    Io(#[from] std::io::Error),
    #[error("protocol error: {0}")]
    Protocol(ProtocolError),
    #[error("deserialization error: {0}")]
    Deserialization(#[source] Box<dyn Error + Send + Sync + 'static>),
}
impl From<ProtocolError> for DownloadError {
    fn from(e: ProtocolError) -> Self {
        match e {
            ProtocolError::Io(e) => Self::Io(e),
            _ => Self::Protocol(e),
        }
    }
}
from_infallible!(DownloadError);

// if destdir is non-null, the file contents will be read and written to
// disk
async fn read_file_list(
    socket: &mut (dyn AsyncRead + Unpin + Send),
    file_count: u8,
    destdir: Option<&Path>,
) -> Result<Vec<FileInfo>, DownloadError> {
    // recv the file info
    let mut list = Vec::<FileInfo>::with_capacity(file_count as usize);
    for _ in 0..file_count {
        let msg = read_message(socket).await?;
        let file = DownloadFile::deserialize(&mut &msg[..])?
            .map_err(Into::into)
            .map_err(DownloadError::Deserialization)?;
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
    socket: &mut (dyn AsyncRead + Unpin + Send),
    destdir: Option<&Path>,
) -> Result<Vec<FileInfo>, DownloadError> {
    let msg = read_message(socket).await?;
    let msg = DownloadMessage::deserialize(&mut &msg[..])?
        .map_err(Into::into)
        .map_err(DownloadError::Deserialization)?;
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
        let hashes: Vec<Hash> = recvlist.iter().map(|f| f.hash()).cloned().collect();
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
