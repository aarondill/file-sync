use std::fmt::Display;
use std::io;
use std::str::FromStr;

use crate::serial::{self, Deserialize, Serialize, deserialize, from_infallible};

/// An ascii string with a max length of 255 bytes
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ProtocolString(String);

impl Display for ProtocolString {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("string is too long (max 255 bytes)")]
    LengthTooLong,
    #[error("string is not ascii")]
    NotAscii,
}
impl ProtocolString {
    /// The string must be ascii and less than 255 bytes
    pub fn new_unchecked(value: String) -> Self {
        debug_assert!(value.len() <= 255);
        debug_assert!(value.is_ascii());
        Self(value)
    }

    pub fn new(value: &str) -> Result<Self, Error> {
        if value.len() > 255 {
            return Err(Error::LengthTooLong);
        }
        Self::new_truncate(value)
    }

    pub fn new_truncate(value: &str) -> Result<Self, Error> {
        if !value.is_ascii() {
            return Err(Error::NotAscii);
        }
        let len = value.len().min(255);
        let value = value[..len].to_string();
        Ok(Self(value))
    }

    pub fn into_inner(self) -> String {
        self.0
    }

    pub fn from_bytes(value: &[u8]) -> Result<Self, Error> {
        match std::str::from_utf8(value) {
            Ok(value) => Self::new(value),
            Err(_) => Err(Error::NotAscii),
        }
    }

    pub fn len(self: &Self) -> u8 {
        return self.0.len() as u8;
    }
}

impl TryFrom<&str> for ProtocolString {
    type Error = Error;

    fn try_from(value: &str) -> Result<Self, Self::Error> {
        Self::new(value)
    }
}
impl FromStr for ProtocolString {
    type Err = Error;

    fn from_str(value: &str) -> Result<Self, Self::Err> {
        Self::new(value)
    }
}
impl From<ProtocolString> for String {
    fn from(value: ProtocolString) -> Self {
        value.0
    }
}

#[derive(Debug, thiserror::Error)]
pub enum SerialError {
    #[error("bytes were not valid ascii")]
    NotAscii,
}
from_infallible!(SerialError);

impl Deserialize for ProtocolString {
    type Error = SerialError;

    fn deserialize(reader: &mut dyn io::Read) -> serial::Result<Self, Self::Error> {
        let length = deserialize!(reader, u8) as usize;
        let mut buf = [0u8; 255];
        let slice = &mut buf[..length];
        reader.read_exact(slice)?;
        Ok(match Self::from_bytes(slice) {
            Ok(val) => Ok(val),
            Err(Error::LengthTooLong) => unreachable!("length was checked"),
            Err(Error::NotAscii) => Err(SerialError::NotAscii),
        })
    }
}
impl Serialize for ProtocolString {
    fn serialize(&self, writer: &mut dyn io::Write) -> io::Result<()> {
        self.len().serialize(writer)?;
        writer.write_all(self.0.as_bytes())?;
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::*;
    #[test]
    fn test_variable_length_string() {
        let value = "hello world";
        let variable_length_string = ProtocolString::new(value).unwrap();
        assert_eq!(variable_length_string.len(), value.len() as u8);
        assert_eq!(variable_length_string.to_string(), value);
    }
    #[test]
    fn test_serialize_variable_length_string() {
        let value = "hello world";
        let variable_length_string = ProtocolString::new(value).unwrap();

        let mut buffer = Vec::new();
        variable_length_string.serialize(&mut buffer).unwrap();

        let mut expected = vec![value.len() as u8];
        expected.extend_from_slice(value.as_bytes());

        assert_eq!(buffer, expected);
    }
    #[test]
    fn test_deserialize_variable_length_string() {
        let value = "hello world";

        let mut buf = vec![value.len() as u8];
        buf.extend_from_slice(value.as_bytes());
        let mut reader = std::io::Cursor::new(buf);

        let deserial = ProtocolString::deserialize(&mut reader).unwrap().unwrap();

        assert_eq!(deserial.len(), value.len() as u8);
        assert_eq!(deserial.0, value);
    }
}
