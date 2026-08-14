use bitflags::bitflags;
use std::{io::Read, io::Write};

use crate::{
    protocol::{Deserialize, Serialize},
    variable_length_string::VariableLengthString,
};

bitflags! {
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub struct Flags: u8 {
        const isError = 0b00000001; /// must be set!
        const _ = !0; // any other bits are reserved for future use
    }
}

#[derive(Debug, PartialEq, Eq, Clone)]
pub struct ProtocolError {
    flags: Flags,
    code: u8,
    message: VariableLengthString,
}

impl ProtocolError {
    pub fn new(flags: Flags, code: u8, message: VariableLengthString) -> Self {
        assert!(flags.contains(Flags::isError));
        Self {
            flags,
            code,
            message,
        }
    }
}

impl Serialize for ProtocolError {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Box<dyn std::error::Error>> {
        self.flags.bits().serialize(writer)?;
        self.code.serialize(writer)?;
        self.message.serialize(writer)?;
        Ok(())
    }
}
impl Deserialize for ProtocolError {
    fn deserialize(reader: &mut dyn Read) -> Result<Self, Box<dyn std::error::Error>> {
        let flags = Flags::from_bits(u8::deserialize(reader)?).unwrap();
        if !flags.contains(Flags::isError) {
            return Err("not an error".into());
        }
        let code = u8::deserialize(reader)?;
        let message = VariableLengthString::deserialize(reader)?;
        Ok(Self {
            flags,
            code,
            message,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn serialize_works() {
        let test = ProtocolError::new(Flags::isError, 0, "test".try_into().unwrap());
        let mut bytes = Vec::new();
        test.serialize(&mut bytes).expect("failed to serialize");
        let expected = [0b00000001, 0, 4, b't', b'e', b's', b't'];
        assert_eq!(bytes, expected);
    }
    #[test]
    fn deserialize_works() {
        let bytes = [0b00000001, 0, 4, b't', b'e', b's', b't'];
        let val = ProtocolError::deserialize(&mut &bytes[..]).expect("failed to deserialize");

        let expected = ProtocolError::new(Flags::isError, 0, "test".try_into().unwrap());
        assert_eq!(val, expected);
    }
    #[test]
    fn it_works() {
        let test = ProtocolError::new(Flags::isError, 0, "test".try_into().unwrap());
        let mut bytes = Vec::new();
        test.serialize(&mut bytes).expect("failed to serialize");

        let mut reader = std::io::Cursor::new(bytes);
        let test2 = ProtocolError::deserialize(&mut reader).expect("failed to deserialize");
        assert_eq!(test, test2);
    }
}
