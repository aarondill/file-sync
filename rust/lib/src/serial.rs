// TODO: use serde eventually?

use std::io::{Read, Write};

pub trait Deserialize {
    fn deserialize(reader: &mut dyn Read) -> Result<Self, Box<dyn std::error::Error>>
    where
        Self: Sized;
}

pub trait Serialize {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Box<dyn std::error::Error>>;
}

impl Deserialize for u8 {
    fn deserialize(reader: &mut dyn Read) -> Result<Self, Box<dyn std::error::Error>> {
        let mut buf = [0u8; (Self::BITS / 8) as usize];
        reader.read_exact(&mut buf)?;
        Ok(Self::from_be_bytes(buf))
    }
}

impl Serialize for u8 {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Box<dyn std::error::Error>> {
        writer.write_all(&self.to_be_bytes())?;
        Ok(())
    }
}
impl Deserialize for u16 {
    fn deserialize(reader: &mut dyn Read) -> Result<Self, Box<dyn std::error::Error>> {
        let mut buf = [0u8; (Self::BITS / 8) as usize];
        reader.read_exact(&mut buf)?;
        Ok(Self::from_be_bytes(buf))
    }
}

impl Serialize for u16 {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Box<dyn std::error::Error>> {
        writer.write_all(&self.to_be_bytes())?;
        Ok(())
    }
}
impl Deserialize for u32 {
    fn deserialize(reader: &mut dyn Read) -> Result<Self, Box<dyn std::error::Error>> {
        let mut buf = [0u8; (Self::BITS / 8) as usize];
        reader.read_exact(&mut buf)?;
        Ok(Self::from_be_bytes(buf))
    }
}

impl Serialize for u32 {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Box<dyn std::error::Error>> {
        writer.write_all(&self.to_be_bytes())?;
        Ok(())
    }
}
impl Deserialize for u64 {
    fn deserialize(reader: &mut dyn Read) -> Result<Self, Box<dyn std::error::Error>> {
        let mut buf = [0u8; (Self::BITS / 8) as usize];
        reader.read_exact(&mut buf)?;
        Ok(Self::from_be_bytes(buf))
    }
}

impl Serialize for u64 {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Box<dyn std::error::Error>> {
        writer.write_all(&self.to_be_bytes())?;
        Ok(())
    }
}
impl Deserialize for u128 {
    fn deserialize(reader: &mut dyn Read) -> Result<Self, Box<dyn std::error::Error>> {
        let mut buf = [0u8; (Self::BITS / 8) as usize];
        reader.read_exact(&mut buf)?;
        Ok(Self::from_be_bytes(buf))
    }
}

impl Serialize for u128 {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Box<dyn std::error::Error>> {
        writer.write_all(&self.to_be_bytes())?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn test_u128() {
        let data = 128;
        let mut buf = Vec::new();
        data.serialize(&mut buf).unwrap();
        assert_eq!(buf, vec![0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80]);
        let mut cursor = std::io::Cursor::new(buf);
        let data2 = u128::deserialize(&mut cursor).unwrap();
        assert_eq!(data, data2);
    }
    #[test]
    fn test_u64() {
        let data = 64;
        let mut buf = Vec::new();
        data.serialize(&mut buf).unwrap();
        assert_eq!(buf, vec![0x00, 0x00, 0x00, 0x40]);
        let mut cursor = std::io::Cursor::new(buf);
        let data2 = u64::deserialize(&mut cursor).unwrap();
        assert_eq!(data, data2);
    }
    #[test]
    fn test_u32() {
        let data = 32;
        let mut buf = Vec::new();
        data.serialize(&mut buf).unwrap();
        assert_eq!(buf, vec![0x00, 0x20]);
        let mut cursor = std::io::Cursor::new(buf);
        let data2 = u32::deserialize(&mut cursor).unwrap();
        assert_eq!(data, data2);
    }
    #[test]
    fn test_u16() {
        let data = 16;
        let mut buf = Vec::new();
        data.serialize(&mut buf).unwrap();
        assert_eq!(buf, vec![0x10]);
        let mut cursor = std::io::Cursor::new(buf);
        let data2 = u16::deserialize(&mut cursor).unwrap();
        assert_eq!(data, data2);
    }
    #[test]
    fn test_u8() {
        let data = 8;
        let mut buf = Vec::new();
        data.serialize(&mut buf).unwrap();
        assert_eq!(buf, vec![0x08]);
        let mut cursor = std::io::Cursor::new(buf);
        let data2 = u8::deserialize(&mut cursor).unwrap();
        assert_eq!(data, data2);
    }
    #[test]
    fn test_u16_fail() {
        let buf = vec![0x10];
        let mut cursor = std::io::Cursor::new(buf);
        let data2 = u8::deserialize(&mut cursor);
        assert!(data2.is_err());
    }
}
