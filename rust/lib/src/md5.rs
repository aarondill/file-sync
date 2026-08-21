use std::convert::Infallible;

use md5::{Digest, Md5};
use tokio::io::{AsyncRead, AsyncReadExt};

use crate::serial::{self, Deserialize, Serialize};
#[derive(Debug, thiserror::Error)]
pub enum HashParseError {
    #[error("invalid hex length")]
    InvalidHexLength,
    #[error("invalid hex character")]
    InvalidHexCharacter,
}

#[derive(PartialEq, Eq, Clone)]
pub struct Hash([u8; 16]);
impl Hash {
    pub fn from_bytes(hash: [u8; 16]) -> Self {
        Self(hash)
    }

    pub fn from_hex(hex: &str) -> Result<Self, HashParseError> {
        let mut hash = [0; 16];
        if hex.len() != 32 {
            return Err(HashParseError::InvalidHexLength);
        }
        for (i, c) in hex.as_bytes().chunks(2).enumerate() {
            let s = std::str::from_utf8(c).map_err(|_| HashParseError::InvalidHexCharacter)?;
            let c = u8::from_str_radix(s, 16).map_err(|_| HashParseError::InvalidHexCharacter)?;
            hash[i] = c;
        }
        Ok(Self(hash))
    }

    pub async fn new(f: &mut (dyn AsyncRead + Unpin + Send)) -> Result<Self, std::io::Error> {
        let mut hasher = Md5::new();
        let mut buf = [0; 4096];
        loop {
            let size = f.read(&mut buf).await?;
            if size == 0 {
                break; // EOF
            }
            hasher.update(&buf[..size]);
        }
        Ok(Self(hasher.finalize().into()))
    }

    pub fn hex(&self) -> String {
        let mut s = String::new();
        for b in &self.0 {
            s.push_str(&format!("{:02x}", b));
        }
        s
    }
}
impl std::str::FromStr for Hash {
    type Err = HashParseError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::from_hex(s)
    }
}
impl std::fmt::Display for Hash {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.hex())
    }
}
impl std::fmt::Debug for Hash {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "FileHash({})", self.hex())
    }
}

impl Serialize for Hash {
    fn serialize(&self, writer: &mut dyn std::io::Write) -> std::io::Result<()> {
        writer.write_all(&self.0)?;
        Ok(())
    }
}
impl Deserialize for Hash {
    type Error = Infallible;

    fn deserialize(reader: &mut dyn std::io::Read) -> serial::Result<Self, Self::Error>
    where
        Self: Sized,
    {
        let mut hash = [0; 16];
        reader.read_exact(&mut hash)?;
        Ok(Ok(Self(hash)))
    }
}

#[cfg(test)]
mod tests {
    use std::io::Cursor;

    use super::*;
    #[tokio::test]
    async fn test_file_hash() {
        let data = "hello world".as_bytes().to_vec();
        let fh = Hash::new(&mut Cursor::new(data)).await.unwrap();
        assert_eq!(fh.hex(), "5eb63bbbe01eeed093cb22bb8f5acdc3")
    }
    #[test]
    fn test_serialize() {
        let bytes = [
            0x5e, 0xb6, 0x3b, 0xbb, 0xe0, 0x1e, 0xee, 0xd0, 0x93, 0xcb, 0x22, 0xbb, 0x8f, 0x5a,
            0xcd, 0xc3,
        ];
        let fh = Hash::from_bytes(bytes);
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
        let fh = Hash::deserialize(&mut &bytes[..]).unwrap().unwrap();
        assert_eq!(fh.0, bytes);
    }
    #[test]
    fn test_from_hex() {
        let fh = Hash::from_hex("5eb63bbbe01eeed093cb22bb8f5acdc3").unwrap();
        assert_eq!(fh.hex(), "5eb63bbbe01eeed093cb22bb8f5acdc3");
    }
}
