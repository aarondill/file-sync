use std::fmt::Display;
use std::io::{Read, Write};

use crate::serial::{Deserialize, Serialize};

/// A variable length string stored on the stack as a length byte followed by the bytes.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VariableLengthString {
    length: u8,
    value: [u8; 255],
}

impl Display for VariableLengthString {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let s = String::from_utf8_lossy(&self.value[..self.length as usize]);
        write!(f, "{}", s)
    }
}

impl VariableLengthString {
    pub fn from_bytes(length: u8, value: [u8; 255]) -> Self {
        Self { length, value }
    }

    pub fn new(value: &[u8]) -> Result<Self, std::num::TryFromIntError> {
        u8::try_from(value.len())?;
        Ok(Self::new_truncate(value))
    }

    pub fn new_truncate(value: &[u8]) -> Self {
        let length = value.len().min(255).try_into().expect("impossible");
        let mut value_bytes = [0u8; 255];
        value_bytes[..length as usize].copy_from_slice(value);
        Self::from_bytes(length, value_bytes)
    }

    pub fn len(self: &Self) -> u8 {
        return self.length;
    }

    pub fn slice(self: &Self) -> &[u8] {
        return &self.value[..self.length as usize];
    }
}

impl std::convert::TryFrom<&[u8]> for VariableLengthString {
    type Error = std::num::TryFromIntError;

    fn try_from(value: &[u8]) -> Result<Self, Self::Error> {
        Self::new(value)
    }
}
impl std::convert::TryFrom<&str> for VariableLengthString {
    type Error = std::num::TryFromIntError;

    fn try_from(value: &str) -> Result<Self, Self::Error> {
        Self::new(value.as_bytes())
    }
}

impl std::convert::Into<String> for VariableLengthString {
    fn into(self) -> String {
        return self.to_string();
    }
}

impl Deserialize for VariableLengthString {
    type Error = std::io::Error;

    fn deserialize(reader: &mut dyn Read) -> Result<Self, Self::Error> {
        let length = u8::deserialize(reader)?;
        let mut value = [0u8; 255];
        reader.read_exact(&mut value[..length as usize])?;
        Ok(Self { length, value })
    }
}
impl Serialize for VariableLengthString {
    type Error = std::io::Error;

    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Self::Error> {
        self.length.serialize(writer)?;
        writer.write_all(&self.value[..self.length as usize])?;
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::*;
    #[test]
    fn test_variable_length_string() {
        let value = "hello world";
        let variable_length_string = VariableLengthString::new(value.as_bytes()).unwrap();
        assert_eq!(variable_length_string.len(), value.len() as u8);
        assert_eq!(variable_length_string.to_string(), value);
    }
    #[test]
    fn test_serialize_variable_length_string() {
        let value = "hello world";
        let variable_length_string = VariableLengthString::new(value.as_bytes()).unwrap();

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

        let deserial = VariableLengthString::deserialize(&mut reader).unwrap();

        assert_eq!(deserial.len(), value.len() as u8);
        assert_eq!(deserial.slice(), value.as_bytes());
    }
}
