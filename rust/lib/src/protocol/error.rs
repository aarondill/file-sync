use std::io::{Read, Write};

use bitflags::bitflags;

use crate::protocol::ProtocolString;
use crate::serial::{self, Deserialize, Serialize, deserialize, from_infallible};

bitflags! {
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub struct Flags: u8 {
        const isError = 0b00000001; /// must be set!
        const _ = !0; // any other bits are reserved for future use
    }
}

#[derive(Debug, PartialEq, Eq, Clone)]
pub struct Error {
    flags: Flags,
    code: u8,
    message: ProtocolString,
}

impl Error {
    pub fn new(flags: Flags, code: u8, message: ProtocolString) -> Self {
        assert!(flags.contains(Flags::isError));
        Self { flags, code, message }
    }
}

impl Serialize for Error {
    fn serialize(&self, writer: &mut dyn Write) -> std::io::Result<()> {
        self.flags.bits().serialize(writer)?;
        self.code.serialize(writer)?;
        self.message.serialize(writer)?;
        Ok(())
    }
}

#[derive(Debug, thiserror::Error)]
pub enum SerialError {
    #[error(transparent)]
    String(#[from] crate::protocol::string::SerialError),
    #[error("error flag set to unexpected value")]
    ErrorFlag,
}
from_infallible!(SerialError);

impl Deserialize for Error {
    type Error = SerialError;

    fn deserialize(reader: &mut dyn Read) -> serial::Result<Self, Self::Error> {
        let flags = deserialize!(reader, u8);
        let flags = Flags::from_bits(flags).unwrap();
        if !flags.contains(Flags::isError) {
            return Ok(Err(Self::Error::ErrorFlag));
        }
        let code = deserialize!(reader, u8);
        let message = deserialize!(reader, ProtocolString);
        Ok(Ok(Self { flags, code, message }))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn serialize_works() {
        let test = Error::new(Flags::isError, 0, "test".try_into().unwrap());
        let mut bytes = Vec::new();
        test.serialize(&mut bytes).unwrap();
        let expected = [0b00000001, 0, 4, b't', b'e', b's', b't'];
        assert_eq!(bytes, expected);
    }
    #[test]
    fn deserialize_works() {
        let bytes = [0b00000001, 0, 4, b't', b'e', b's', b't'];
        let val = Error::deserialize(&mut &bytes[..]).unwrap().unwrap();

        let expected = Error::new(Flags::isError, 0, "test".try_into().unwrap());
        assert_eq!(val, expected);
    }
    #[test]
    fn it_works() {
        let test = Error::new(Flags::isError, 0, "test".try_into().unwrap());
        let mut bytes = Vec::new();
        test.serialize(&mut bytes).unwrap();

        let mut reader = std::io::Cursor::new(bytes);
        let test2 = Error::deserialize(&mut reader).unwrap().unwrap();
        assert_eq!(test, test2);
    }
}
