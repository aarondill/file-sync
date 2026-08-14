use bitflags::bitflags;
use std::{io::Read, io::Write};

use crate::{
    file_hash::FileHash,
    protocol::{Deserialize, Serialize},
};

bitflags! {
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub struct Flags: u8 {
        const isError = 0b00000001; /// must not be set!
        const _ = !0; // any other bits are reserved for future use
    }
}
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct DownloadResponse {
    flags: Flags,
    files: Vec<FileHash>,
}

impl DownloadResponse {
    pub fn new(flags: Flags, files: Vec<FileHash>) -> Self {
        assert!(!flags.contains(Flags::isError));
        u8::try_from(files.len()).expect("too many files");
        Self { flags, files }
    }
}

impl Serialize for DownloadResponse {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Box<dyn std::error::Error>> {
        self.flags.bits().serialize(writer)?;
        let len: u8 = self.files.len() as u8;
        len.serialize(writer)?;
        for file in &self.files {
            file.serialize(writer)?;
        }
        Ok(())
    }
}
impl Deserialize for DownloadResponse {
    fn deserialize(reader: &mut dyn Read) -> Result<Self, Box<dyn std::error::Error>> {
        let flags = Flags::from_bits(u8::deserialize(reader)?).unwrap();
        if flags.contains(Flags::isError) {
            return Err("is an error".into());
        }
        let file_count = u8::deserialize(reader)?;
        let mut files = Vec::with_capacity(file_count as usize);
        for _ in 0..file_count {
            files.push(FileHash::deserialize(reader)?);
        }
        Ok(Self { flags, files })
    }
}

#[cfg(test)]
mod tests {
    use crate::file_hash::FileHash;

    fn test_file() -> DownloadResponse {
        DownloadResponse::new(Flags::empty(), vec![FileHash::new_from_bytes([1; 16])])
    }
    const TEST_BYTES: [u8; 18] = [
        0, // flags
        1, // file count
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // hash
    ];

    use super::*;
    #[test]
    fn serialize_works() {
        let mut buffer = Vec::new();
        test_file().serialize(&mut buffer).unwrap();
        assert_eq!(buffer, TEST_BYTES);
    }
    #[test]
    fn deserialize_works() {
        let bytes = TEST_BYTES.clone();
        let message = DownloadResponse::deserialize(&mut bytes.as_slice()).unwrap();
        assert_eq!(message, test_file());
    }
    #[test]
    fn it_works() {
        let message = test_file();
        let mut buffer = Vec::new();
        message.serialize(&mut buffer).unwrap();

        let deserialized = DownloadResponse::deserialize(&mut buffer.as_slice()).unwrap();
        assert_eq!(message, deserialized);
    }
}
