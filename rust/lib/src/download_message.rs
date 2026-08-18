use crate::serial::{Deserialize, Serialize};
use bitflags::bitflags;
use std::io::{Read, Write};

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
    file_count: u8,
}

impl DownloadMessage {
    pub fn flags(&self) -> &Flags {
        &self.flags
    }
    pub fn file_count(&self) -> u8 {
        self.file_count
    }
    pub fn new(flags: Flags, file_count: u8) -> Self {
        assert!(!flags.contains(Flags::isError));
        Self { flags, file_count }
    }
}

impl Serialize for DownloadMessage {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Box<dyn std::error::Error>> {
        self.flags.bits().serialize(writer)?;
        self.file_count.serialize(writer)?;
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
        Ok(Self { flags, file_count })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn serialize_works() {
        let mut buffer = Vec::new();
        DownloadMessage::new(Flags::empty(), 1)
            .serialize(&mut buffer)
            .unwrap();
        assert_eq!(buffer, [0, 1]);
    }
    #[test]
    fn deserialize_works() {
        let bytes = [0, 1];
        let message = DownloadMessage::deserialize(&mut bytes.as_slice()).unwrap();
        assert_eq!(message, DownloadMessage::new(Flags::empty(), 1));
    }
    #[test]
    fn it_works() {
        let message = DownloadMessage::new(Flags::empty(), 1);
        let mut buffer = Vec::new();
        message.serialize(&mut buffer).unwrap();

        let deserialized = DownloadMessage::deserialize(&mut buffer.as_slice()).unwrap();
        assert_eq!(message, deserialized);
    }
}
