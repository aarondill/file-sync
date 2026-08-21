use std::convert::Infallible;
use std::io;

pub type Result<T, E> = io::Result<std::result::Result<T, E>>;

pub trait Deserialize {
    type Error: std::error::Error + 'static;
    fn deserialize(reader: &mut dyn io::Read) -> Result<Self, Self::Error>
    where
        Self: Sized;
}

pub trait Serialize {
    fn serialize(&self, writer: &mut dyn io::Write) -> io::Result<()>;
}

/// A simple macro to implement `From<!>` for a type
macro_rules! from_infallible {
    ($ty:ty) => {
        impl From<std::convert::Infallible> for $ty {
            fn from(_: std::convert::Infallible) -> Self {
                unreachable!("infallible")
            }
        }
    };
}
pub(crate) use from_infallible;

/// A simple macro that calls deserialize on a type and handles the errors
macro_rules! deserialize {
    ($reader:expr, $ty:ty) => {
        match <$ty as Deserialize>::deserialize($reader)? {
            Ok(v) => v,
            #[allow(unreachable_code)] // in case of infallible
            Err(e) => return Ok(Err(e.into())),
        }
    };
}
pub(crate) use deserialize;

impl Deserialize for u8 {
    type Error = Infallible;

    fn deserialize(reader: &mut dyn io::Read) -> Result<Self, Self::Error> {
        let mut buf = [0u8; (Self::BITS / 8) as usize];
        reader.read_exact(&mut buf)?;
        Ok(Ok(Self::from_be_bytes(buf)))
    }
}

impl Serialize for u8 {
    fn serialize(&self, writer: &mut dyn io::Write) -> io::Result<()> {
        writer.write_all(&self.to_be_bytes())?;
        Ok(())
    }
}
impl Deserialize for u16 {
    type Error = Infallible;

    fn deserialize(reader: &mut dyn io::Read) -> Result<Self, Self::Error> {
        let mut buf = [0u8; (Self::BITS / 8) as usize];
        reader.read_exact(&mut buf)?;
        Ok(Ok(Self::from_be_bytes(buf)))
    }
}

impl Serialize for u16 {
    fn serialize(&self, writer: &mut dyn io::Write) -> io::Result<()> {
        writer.write_all(&self.to_be_bytes())?;
        Ok(())
    }
}
impl Deserialize for u32 {
    type Error = Infallible;

    fn deserialize(reader: &mut dyn io::Read) -> Result<Self, Self::Error> {
        let mut buf = [0u8; (Self::BITS / 8) as usize];
        reader.read_exact(&mut buf)?;
        Ok(Ok(Self::from_be_bytes(buf)))
    }
}

impl Serialize for u32 {
    fn serialize(&self, writer: &mut dyn io::Write) -> io::Result<()> {
        writer.write_all(&self.to_be_bytes())?;
        Ok(())
    }
}
impl Deserialize for u64 {
    type Error = Infallible;

    fn deserialize(reader: &mut dyn io::Read) -> Result<Self, Self::Error> {
        let mut buf = [0u8; (Self::BITS / 8) as usize];
        reader.read_exact(&mut buf)?;
        Ok(Ok(Self::from_be_bytes(buf)))
    }
}

impl Serialize for u64 {
    fn serialize(&self, writer: &mut dyn io::Write) -> io::Result<()> {
        writer.write_all(&self.to_be_bytes())?;
        Ok(())
    }
}
impl Deserialize for u128 {
    type Error = Infallible;

    fn deserialize(reader: &mut dyn io::Read) -> Result<Self, Self::Error> {
        let mut buf = [0u8; (Self::BITS / 8) as usize];
        reader.read_exact(&mut buf)?;
        Ok(Ok(Self::from_be_bytes(buf)))
    }
}

impl Serialize for u128 {
    fn serialize(&self, writer: &mut dyn io::Write) -> io::Result<()> {
        writer.write_all(&self.to_be_bytes())?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn test_u128() {
        let data: u128 = 128;
        let mut buf = Vec::new();
        data.serialize(&mut buf).unwrap();
        assert_eq!(
            buf,
            vec![
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x80
            ]
        );
        let data2 = u128::deserialize(&mut &buf[..]).unwrap().unwrap();
        assert_eq!(data, data2);
    }
    #[test]
    fn test_u64() {
        let data: u64 = 64;
        let mut buf = Vec::new();
        data.serialize(&mut buf).unwrap();
        assert_eq!(buf, vec![0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40]);
        let data2 = u64::deserialize(&mut &buf[..]).unwrap().unwrap();
        assert_eq!(data, data2);
    }
    #[test]
    fn test_u32() {
        let data: u32 = 32;
        let mut buf = Vec::new();
        data.serialize(&mut buf).unwrap();
        assert_eq!(buf, vec![0x00, 0x00, 0x00, 0x20]);
        let data2 = u32::deserialize(&mut &buf[..]).unwrap().unwrap();
        assert_eq!(data, data2);
    }
    #[test]
    fn test_u16() {
        let data: u16 = 16;
        let mut buf = Vec::new();
        data.serialize(&mut buf).unwrap();
        assert_eq!(buf, vec![0x00, 0x10]);
        let data2 = u16::deserialize(&mut &buf[..]).unwrap().unwrap();
        assert_eq!(data, data2);
    }
    #[test]
    fn test_u8() {
        let data: u8 = 8;
        let mut buf = Vec::new();
        data.serialize(&mut buf).unwrap();
        assert_eq!(buf, vec![0x08]);
        let data2 = u8::deserialize(&mut &buf[..]).unwrap().unwrap();
        assert_eq!(data, data2);
    }
    #[test]
    fn test_u16_fail() {
        let buf = vec![0x10];
        let data2 = u16::deserialize(&mut &buf[..]);
        assert!(data2.is_err());
    }
}
