use std::borrow::Borrow;
use std::path::Path;

use tokio::fs::File;
use tokio::io::{AsyncRead, AsyncWrite, AsyncWriteExt};

use crate::download_file::DownloadFile;
use crate::download_message::{self, DownloadMessage};
use crate::download_response::DownloadResponse;
use crate::file_info::FileInfo;
use crate::protocol::{read_message, write_message};
use crate::serial::{Deserialize, Serialize};

async fn write_file_list<T: Borrow<FileInfo>>(
    socket: &mut (dyn AsyncWrite + Unpin + Send),
    list: &[T],
    srcdir: Option<&Path>,
) -> Result<(), Box<dyn std::error::Error>> {
    // Send file metadata
    let mut buf = Vec::with_capacity(4096);
    for node in list {
        let f: DownloadFile = node.borrow().into();
        f.serialize(&mut buf).expect("error serializing download file");
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
) -> Result<(), Box<dyn std::error::Error>> {
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
) -> Result<(), Box<dyn std::error::Error>> {
    // Send download message 1
    write_download_message(write, files, None).await?;

    // Receive download response
    let msg = read_message(read).await?;
    let mut cursor = std::io::Cursor::new(msg);
    let resp = DownloadResponse::deserialize(&mut cursor)?;
    assert_eq!(cursor.position(), cursor.into_inner().len() as u64); // EOF

    let filtered_list = resp
        .into_files()
        .into_iter()
        .map(|f| {
            files.iter().find(|&node| node.hash() == &f).ok_or("requested file not in local list")
        })
        .collect::<Result<Vec<_>, _>>()?;

    write_download_message(write, &filtered_list, Some(srcdir)).await?;
    Ok(())
}
