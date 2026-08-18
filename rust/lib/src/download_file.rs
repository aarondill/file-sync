use std::io::{Read, Write};

use crate::file_hash::FileHash;
use crate::file_info::FileInfo;
use crate::serial::{Deserialize, Serialize};
use crate::variable_length_string::VariableLengthString;

#[derive(Debug, PartialEq, Eq, Clone)]
pub struct DownloadFile {
    hash: FileHash,
    size: u32,
    name: VariableLengthString,
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
    pub fn new(hash: FileHash, size: u32, name: VariableLengthString) -> Self {
        Self { hash, size, name }
    }
}

impl std::convert::Into<FileInfo> for DownloadFile {
    fn into(self) -> FileInfo {
        FileInfo::new(
            self.name.to_string().into(),
            self.hash,
            self.size.try_into().expect("size too large"),
        )
    }
}

impl std::convert::From<&FileInfo> for DownloadFile {
    fn from(info: &FileInfo) -> Self {
        Self::new(
            info.hash().clone(),
            info.size().try_into().expect("size too large"),
            info.path().to_string_lossy().as_bytes().try_into().expect("name too long"),
        )
    }
}

impl Serialize for DownloadFile {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Box<dyn std::error::Error>> {
        self.hash.serialize(writer)?;
        self.size.serialize(writer)?;
        self.name.serialize(writer)?;
        Ok(())
    }
}
impl Deserialize for DownloadFile {
    fn deserialize(reader: &mut dyn Read) -> Result<Self, Box<dyn std::error::Error>> {
        let hash = FileHash::deserialize(reader)?;
        let size = u32::deserialize(reader)?;
        let name = VariableLengthString::deserialize(reader)?;
        Ok(Self { hash, size, name })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn serialize_works() {
        let file =
            DownloadFile::new(FileHash::new_from_bytes([0; 16]), 64, "test".try_into().unwrap());
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
        let file = DownloadFile::deserialize(&mut bytes.as_slice()).unwrap();
        let expected =
            DownloadFile::new(FileHash::new_from_bytes([0; 16]), 64, "test".try_into().unwrap());
        assert_eq!(file, expected);
    }
    #[test]
    fn it_works() {
        let file = DownloadFile::new(
            FileHash::new_from_bytes([1; 16]),
            64,
            "test world".try_into().unwrap(),
        );
        let mut buffer = Vec::new();
        file.serialize(&mut buffer).unwrap();

        let mut cursor = std::io::Cursor::new(buffer);
        let file2 = DownloadFile::deserialize(&mut cursor).unwrap();
        assert_eq!(file, file2);
    }
}
