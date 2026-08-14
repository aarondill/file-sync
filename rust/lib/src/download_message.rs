use bitflags::bitflags;
use std::{io::Read, io::Write};

use crate::{
    download_file::DownloadFile,
    serial::{Deserialize, Serialize},
};

bitflags! {
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub struct Flags: u8 {
        const isError = 0b00000001; /// must not be set!
        const _ = !0; // any other bits are reserved for future use
    }
}

// 1. download message 1 - no file contents
// 2. download response
// 3. download message 2 - file contents
//
// - 8 bits for flags
//   - bit 0 - error
//     - Must be 0
//   - rest of bits are reserved for future use
// - 8 bits for file count
//   - Note: supports up to 255 files
// - for each file:
//   - 128 bits for file hash (MD5)
//   - 32 bits for file size in bytes
//     - Note: supports up to 4 GB
//   - 8 bits for file name length in bytes
//     - Note: supports up to 255 bytes
//     - Files with longer names should be rejected (don't truncate because it can cause security issues, and split characters in the middle)
//   - File name (max of 255 bytes, variable length)
// - All files data (variable length)
//   - This data is only present on the second message. The first message says the file sizes and count, but does not include the data.
//   - Each file is sent in order, and the client must verify the hash
//   - total size is the sum of all file sizes

#[derive(Debug, PartialEq, Eq, Clone)]
/// NOTE: followed by file data on second message
pub struct DownloadMessage {
    flags: Flags,
    files: Vec<DownloadFile>,
}

impl DownloadMessage {
    pub fn new(flags: Flags, files: Vec<DownloadFile>) -> Self {
        assert!(!flags.contains(Flags::isError));
        u8::try_from(files.len()).expect("too many files");
        Self { flags, files }
    }
}

impl Serialize for DownloadMessage {
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
impl Deserialize for DownloadMessage {
    fn deserialize(reader: &mut dyn Read) -> Result<Self, Box<dyn std::error::Error>> {
        let flags = Flags::from_bits(u8::deserialize(reader)?).unwrap();
        if flags.contains(Flags::isError) {
            return Err("is an error".into());
        }
        let file_count = u8::deserialize(reader)?;
        let mut files = Vec::with_capacity(file_count as usize);
        for _ in 0..file_count {
            files.push(DownloadFile::deserialize(reader)?);
        }
        Ok(Self { flags, files })
    }
}

#[cfg(test)]
mod tests {
    use crate::file_hash::FileHash;

    fn test_file() -> DownloadMessage {
        DownloadMessage::new(
            Flags::empty(),
            vec![DownloadFile::new(
                FileHash::new_from_bytes([1; 16]),
                100029,
                "test.txt".try_into().unwrap(),
            )],
        )
    }
    const TEST_BYTES: [u8; 31] = [
        0, // flags
        1, // file count
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // hash
        0b00000000, 0b00000001, 0b10000110, 0b10111101, // file size
        8, b't', b'e', b's', b't', b'.', b't', b'x', b't', // file name
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
        let message = DownloadMessage::deserialize(&mut bytes.as_slice()).unwrap();
        assert_eq!(message, test_file());
    }
    #[test]
    fn it_works() {
        let message = test_file();
        let mut buffer = Vec::new();
        message.serialize(&mut buffer).unwrap();

        let deserialized = DownloadMessage::deserialize(&mut buffer.as_slice()).unwrap();
        assert_eq!(message, deserialized);
    }
}
