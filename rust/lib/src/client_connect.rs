use std::io::{Read, Write};

use bitflags::bitflags;

use crate::serial::{Deserialize, Serialize};
use crate::variable_length_string::VariableLengthString;
const PROTOCOL_VERSION: u8 = 2;

bitflags! {
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub struct Flags: u8 {
        const IntentToUpload = 0b00000001;
        const _ = !0; // any other bits are reserved for future use
    }
}
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct ClientConnect {
    protocol_version: u8,
    flags: Flags,
    client_name: VariableLengthString,
}

impl ClientConnect {
    pub fn protocol_version(&self) -> u8 {
        self.protocol_version
    }

    pub fn flags(&self) -> &Flags {
        &self.flags
    }

    pub fn client_name(&self) -> &VariableLengthString {
        &self.client_name
    }

    pub fn new(flags: Flags, client_name: VariableLengthString) -> Self {
        Self { protocol_version: PROTOCOL_VERSION, flags, client_name }
    }
}

#[derive(Debug, thiserror::Error)]
enum SerialError {
    #[error(transparent)]
    Io(#[from] std::io::Error),
    #[error("protocol version mismatch: got {0}, expected {PROTOCOL_VERSION}")]
    ProtocolVersionMismatch(u8),
}

impl Serialize for ClientConnect {
    type Error = std::io::Error;

    fn serialize(&self, writer: &mut dyn Write) -> Result<(), SerialError> {
        self.protocol_version.serialize(writer)?;
        self.flags.bits().serialize(writer)?;
        self.client_name.serialize(writer)?;
        Ok(())
    }
}
impl Deserialize for ClientConnect {
    type Error = SerialError;

    fn deserialize(reader: &mut dyn Read) -> Result<Self, SerialError> {
        let protocol_version = u8::deserialize(reader)?;
        if protocol_version != PROTOCOL_VERSION {
            return Err(SerialError::ProtocolVersionMismatch(protocol_version));
        }

        let flags = u8::deserialize(reader)?;
        let flags = Flags::from_bits(flags).expect("invalid flags");

        let client_name = VariableLengthString::deserialize(reader)?;

        Ok(Self { protocol_version, flags, client_name })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn serialize_works() {
        let test = ClientConnect::new(Flags::IntentToUpload, "test".try_into().unwrap());
        let mut bytes = Vec::<u8>::new();
        test.serialize(&mut bytes).expect("failed to serialize");

        let expected = [PROTOCOL_VERSION, 0b00000001, 4, b't', b'e', b's', b't'];
        assert_eq!(bytes, expected);
    }
    #[test]
    fn deserialize_works() {
        let bytes = [PROTOCOL_VERSION, 0b00000001, 4, b't', b'e', b's', b't'];
        let res = ClientConnect::deserialize(&mut &bytes[..]).expect("failed to serialize");

        let expected = ClientConnect::new(Flags::IntentToUpload, "test".try_into().unwrap());
        assert_eq!(res, expected);
    }
    #[test]
    fn it_works() {
        let name = "this is a test of the client connect protocol";
        let test = ClientConnect::new(Flags::empty(), name.try_into().unwrap());
        let mut bytes = Vec::<u8>::new();
        test.serialize(&mut bytes).expect("failed to serialize");

        let mut reader = std::io::Cursor::new(bytes);
        let test2 = ClientConnect::deserialize(&mut reader).expect("failed to deserialize");
        assert_eq!(test, test2);
    }
}
