use std::borrow::Borrow;
use std::path::Path;

use tokio::fs::File;
use tokio::io::{AsyncRead, AsyncWrite, AsyncWriteExt};

use crate::file_info::FileInfo;
use crate::io::{ProtocolError, read_message, write_message};
use crate::protocol::download_file::FileInfoError;
use crate::protocol::{DownloadFile, DownloadMessage, DownloadResponse, download_message};
use crate::serial::{Deserialize, Serialize};

#[derive(Debug, thiserror::Error)]
pub enum UploadError {
    #[error(transparent)]
    Io(#[from] std::io::Error),
    #[error("deserialization error: {0}")]
    Deserialize(Box<dyn std::error::Error + Send + Sync>),
    #[error(transparent)]
    FileInfo(#[from] FileInfoError),
    #[error(transparent)]
    Protocol(ProtocolError),
    #[error("requested file not in local list")]
    FileNotFound,
}
impl From<ProtocolError> for UploadError {
    fn from(e: ProtocolError) -> Self {
        match e {
            ProtocolError::Io(e) => Self::Io(e),
            e => Self::Protocol(e),
        }
    }
}

async fn write_file_list<T: Borrow<FileInfo>>(
    socket: &mut (dyn AsyncWrite + Unpin + Send),
    list: &[T],
    srcdir: Option<&Path>,
) -> Result<(), UploadError> {
    // Send file metadata
    let mut buf = Vec::with_capacity(4096);
    for node in list {
        let f: DownloadFile = node.borrow().try_into()?;
        f.serialize(&mut buf)?;
        write_message(socket, &buf).await?;
        buf.clear();
    }
    if let Some(srcdir) = srcdir {
        // Send file contents
        for path in list.iter().map(Borrow::borrow).map(|p| srcdir.join(p.path())) {
            let mut file = File::open(path).await?;
            tokio::io::copy(&mut file, socket).await?;
            file.flush().await?;
        }
    }
    Ok(())
}

async fn write_download_message<T: Borrow<FileInfo>>(
    socket: &mut (dyn AsyncWrite + Unpin + Send),
    list: &[T],
    srcdir: Option<&Path>,
) -> Result<(), UploadError> {
    let msg = DownloadMessage::new(
        download_message::Flags::empty(),
        list.len().try_into().expect("too many files"),
    );
    let mut buf = Vec::with_capacity(4096);
    msg.serialize(&mut buf).expect("error serializing download message");
    write_message(socket, &buf).await?;
    write_file_list(socket, list, srcdir).await
}

pub async fn upload(
    read: &mut (dyn AsyncRead + Unpin + Send),
    write: &mut (dyn AsyncWrite + Unpin + Send),
    files: &[FileInfo],
    srcdir: &Path,
) -> Result<(), UploadError> {
    // Send download message 1
    write_download_message(write, files, None).await?;

    // Receive download response
    let msg = read_message(read).await?;
    let mut cursor = std::io::Cursor::new(msg);
    let resp = DownloadResponse::deserialize(&mut cursor)?
        .map_err(Into::into)
        .map_err(UploadError::Deserialize)?;
    assert_eq!(cursor.position(), cursor.into_inner().len() as u64); // EOF

    let filtered_list = resp
        .into_files()
        .into_iter()
        .map(|f| files.iter().find(|&node| node.hash() == &f).ok_or(UploadError::FileNotFound))
        .collect::<Result<Vec<_>, _>>()?;

    write_download_message(write, &filtered_list, Some(srcdir)).await?;
    Ok(())
}
