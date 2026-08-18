use crate::{
    download_message::DownloadMessage,
    download_response::{self, DownloadResponse},
    file_hash::FileHash,
    protocol::write_message,
    serial::{Deserialize, Serialize},
};
use digest_io::HashWriter;
use md5::Md5;
use std::{
    fs::{self, File},
    io::{self, Read},
    net::TcpStream,
    path::Path,
};

use crate::{download_file::DownloadFile, file_info::FileInfo, protocol::read_message};

// if destdir is non-null, the file contents will be read and written to
// disk
fn read_file_list(
    socket: &mut dyn Read,
    file_count: u8,
    destdir: Option<&Path>,
) -> Result<Vec<FileInfo>, Box<dyn std::error::Error>> {
    // recv the file info
    let list = (0..file_count)
        .map(|_| read_message(socket))
        .take_while(Result::is_ok) // fail at first error
        .map(Result::unwrap)
        .map(|msg| {
            let mut cursor = std::io::Cursor::new(msg);
            DownloadFile::deserialize(&mut cursor).map(|f| f.into())
        })
        .collect::<Result<Vec<FileInfo>, _>>()?;
    if let Some(destdir) = destdir {
        // recv/write the file contents
        for f in &list {
            let path = destdir.join(f.path());
            fs::create_dir_all(path.parent().unwrap())?;
            let mut file = File::create(&path)?;
            let mut hasher = HashWriter::<Md5, _>::new(&mut file);
            io::copy(&mut socket.take(f.size()), &mut hasher)?;
            let hash = FileHash::new_from_bytes(hasher.finalize().into());
            // verify hash
            if f.hash() != &hash {
                return Err(format!(
                    "hash mismatch for file {}: expected {}, got {}",
                    path.display(),
                    f.hash(),
                    hash
                )
                .into());
            }
        }
    }
    Ok(list)
}

// if destdir is NULL, the files will not be read from the message (ie. the
// uploader must not send the file contents)
fn read_download_message(
    socket: &mut dyn Read,
    destdir: Option<&Path>,
) -> Result<Vec<FileInfo>, Box<dyn std::error::Error>> {
    let msg = read_message(socket)?;
    let msg = DownloadMessage::deserialize(&mut &msg[..])?;
    return read_file_list(socket, msg.file_count(), destdir);
}

pub fn download(
    socket: &mut TcpStream,
    files: &mut Vec<FileInfo>,
    srcdir: &Path,
) -> Result<(), Box<dyn std::error::Error>> {
    //  read the download message
    let mut recvlist = read_download_message(socket, None)?;
    let to_delete = files
        .iter()
        .filter(|f| recvlist.iter().find(|o| o.path() == f.path()).is_none())
        .collect::<Vec<_>>();
    // filter the recv list to exclude anything that we already have; keep only the files that we need
    recvlist.retain(|f| {
        files
            .iter()
            .find(|o| o.path() == f.path() && o.hash() == f.hash())
            .is_none()
    });
    {
        // send download response
        assert!(recvlist.len() <= 255); // this is a protocol limit
        let hashes: Vec<FileHash> = recvlist.iter().map(|f| f.hash()).cloned().collect();
        let resp = DownloadResponse::new(download_response::Flags::empty(), hashes);
        let mut buf = Vec::with_capacity(4096);
        resp.serialize(&mut buf)
            .expect("error serializing download response");
        write_message(socket, &buf)?;
    }
    // read download message 2
    read_download_message(socket, Some(srcdir))?;
    // delete files that we don't need anymore
    for node in to_delete {
        let mut path = srcdir.join(node.path());
        println!("deleting {}", path.display());
        // remove the file and parent directories
        std::fs::remove_file(&path)?;
        while path.pop() && path != Path::new("") {
            match std::fs::remove_dir(&path) {
                Ok(_) => break,
                Err(e) if e.kind() == std::io::ErrorKind::NotFound => break,
                Err(e) if e.kind() == std::io::ErrorKind::DirectoryNotEmpty => break,
                Err(e) => return Err(e.into()),
            }
        }
    }
    Ok(())
}
