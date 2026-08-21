use std::io::{Read, Write};

use crate::file_info::FileInfo;
use crate::md5::Hash;
use crate::protocol::{self, ProtocolString};
use crate::serial::{self, Deserialize, Serialize, deserialize, from_infallible};

#[derive(Debug, PartialEq, Eq, Clone)]
pub struct DownloadFile {
    hash: Hash,
    size: u32,
    name: ProtocolString,
}

// - for each file:
// - 128 bits for file hash (MD5)
// - 32 bits for file size in bytes
//   - Note: supports up to 4 GB
// - 8 bits for file name length in bytes
//   - Note: supports up to 255 bytes
//   - Files with longer names should be rejected (don't truncate because it can cause security issues, and split characters in the middle)
// - File name (max of 255 bytes, variable length)

impl DownloadFile {
    pub fn new(hash: Hash, size: u32, name: ProtocolString) -> Self {
        Self { hash, size, name }
    }
}

impl std::convert::From<DownloadFile> for FileInfo {
    fn from(f: DownloadFile) -> Self {
        Self::new(f.name.into_inner().into(), f.hash, f.size.into())
    }
}

#[derive(Debug, thiserror::Error)]
pub enum FileInfoError {
    #[error("size too large")]
    SizeTooLarge,
    #[error("invalid name")]
    InvalidName(#[from] protocol::string::Error),
}
impl std::convert::TryFrom<&FileInfo> for DownloadFile {
    type Error = FileInfoError;

    fn try_from(info: &FileInfo) -> Result<Self, Self::Error> {
        let size = info.size().try_into().map_err(|_| FileInfoError::SizeTooLarge)?;
        let name = ProtocolString::new(&info.path().to_string_lossy())?;
        Ok(Self::new(info.hash().clone(), size, name))
    }
}

impl Serialize for DownloadFile {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), std::io::Error> {
        self.hash.serialize(writer)?;
        self.size.serialize(writer)?;
        self.name.serialize(writer)?;
        Ok(())
    }
}
#[derive(Debug, thiserror::Error)]
pub enum SerialError {
    #[error("invalid name: {0}")]
    String(#[from] protocol::string::SerialError),
}
from_infallible!(SerialError);

impl Deserialize for DownloadFile {
    type Error = SerialError;

    fn deserialize(reader: &mut dyn Read) -> serial::Result<Self, Self::Error> {
        let hash = deserialize!(reader, Hash);
        let size = deserialize!(reader, u32);
        let name = deserialize!(reader, ProtocolString);
        Ok(Ok(Self { hash, size, name }))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn serialize_works() {
        let file = DownloadFile::new(Hash::from_bytes([0; 16]), 64, "test".try_into().unwrap());
        let mut buffer = Vec::new();
        file.serialize(&mut buffer).unwrap();
        let expected = [
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // hash
            0b00000000, 0b00000000, 0b00000000, 0b1000000, // size
            4, b't', b'e', b's', b't',
        ];
        assert_eq!(buffer, expected);
    }
    #[test]
    fn deserialize_works() {
        let bytes = [
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // hash
            0b00000000, 0b00000000, 0b00000000, 0b1000000, // size
            4, b't', b'e', b's', b't',
        ];
        let file = DownloadFile::deserialize(&mut bytes.as_slice()).unwrap().unwrap();
        let expected = DownloadFile::new(Hash::from_bytes([0; 16]), 64, "test".try_into().unwrap());
        assert_eq!(file, expected);
    }
    #[test]
    fn it_works() {
        let file =
            DownloadFile::new(Hash::from_bytes([1; 16]), 64, "test world".try_into().unwrap());
        let mut buffer = Vec::new();
        file.serialize(&mut buffer).unwrap();

        let mut cursor = std::io::Cursor::new(buffer);
        let file2 = DownloadFile::deserialize(&mut cursor).unwrap().unwrap();
        assert_eq!(file, file2);
    }
}
