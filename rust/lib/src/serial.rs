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
