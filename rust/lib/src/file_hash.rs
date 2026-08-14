use std::borrow::{Borrow, BorrowMut};

use crate::serial::{Deserialize, Serialize};
use digest_io::IoWrapper;
use md5::{Digest, Md5};

#[derive(PartialEq, Eq, Clone)]
pub struct FileHash([u8; 16]);
impl FileHash {
    pub fn new_from_bytes(hash: [u8; 16]) -> Self {
        Self(hash)
    }
    pub fn new(f: &mut dyn std::io::Read) -> Result<Self, Box<dyn std::error::Error>> {
        let mut hasher = IoWrapper(Md5::new());
        std::io::copy(f, &mut hasher)?;
        Ok(Self(hasher.0.finalize().into()))
    }
    pub fn hex(&self) -> String {
        let mut s = String::new();
        for b in &self.0 {
            s.push_str(&format!("{:02x}", b));
        }
        s
    }
}
impl std::fmt::Display for FileHash {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.hex())
    }
}
impl std::fmt::Debug for FileHash {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "FileHash({})", self.hex())
    }
}

impl Serialize for FileHash {
    fn serialize(&self, writer: &mut dyn std::io::Write) -> Result<(), Box<dyn std::error::Error>> {
        writer.write_all(&self.0)?;
        Ok(())
    }
}
impl Deserialize for FileHash {
    fn deserialize(reader: &mut dyn std::io::Read) -> Result<Self, Box<dyn std::error::Error>>
    where
        Self: Sized,
    {
        let mut hash = [0; 16];
        reader.read_exact(&mut hash)?;
        Ok(Self(hash))
    }
}

#[cfg(test)]
mod tests {

    use std::io::Cursor;

    use super::*;
    #[test]
    fn test_file_hash() {
        let data = "hello world".as_bytes().to_vec();
        let fh = FileHash::new(&mut Cursor::new(data)).unwrap();
        assert_eq!(fh.hex(), "5eb63bbbe01eeed093cb22bb8f5acdc3")
    }
    #[test]
    fn test_serialize() {
        let bytes = [
            0x5e, 0xb6, 0x3b, 0xbb, 0xe0, 0x1e, 0xee, 0xd0, 0x93, 0xcb, 0x22, 0xbb, 0x8f, 0x5a,
            0xcd, 0xc3,
        ];
        let fh = FileHash::new_from_bytes(bytes);
        let mut buf = Vec::new();
        fh.serialize(&mut buf).unwrap();
        assert_eq!(buf, bytes);
    }
    #[test]
    fn test_deserialize() {
        let bytes = [
            0x5e, 0xb6, 0x3b, 0xbb, 0xe0, 0x1e, 0xee, 0xd0, 0x93, 0xcb, 0x22, 0xbb, 0x8f, 0x5a,
            0xcd, 0xc3,
        ];
        let fh = FileHash::deserialize(&mut &bytes[..]).unwrap();
        assert_eq!(fh.0, bytes);
    }
}
