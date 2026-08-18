use bitflags::bitflags;
use std::{io::Read, io::Write};

use crate::{
    serial::{Deserialize, Serialize},
    variable_length_string::VariableLengthString,
};
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
    pub fn new(flags: Flags, client_name: VariableLengthString) -> Self {
        Self {
            protocol_version: PROTOCOL_VERSION,
            flags,
            client_name,
        }
    }
}

impl Serialize for ClientConnect {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Box<dyn std::error::Error>> {
        self.protocol_version.serialize(writer)?;
        self.flags.bits().serialize(writer)?;
        self.client_name.serialize(writer)?;
        Ok(())
    }
}
impl Deserialize for ClientConnect {
    fn deserialize(reader: &mut dyn Read) -> Result<Self, Box<dyn std::error::Error>> {
        let protocol_version = u8::deserialize(reader)?;
        if protocol_version != PROTOCOL_VERSION {
            return Err("protocol version mismatch".into());
        }

        let flags = u8::deserialize(reader)?;
        let flags = Flags::from_bits(flags).expect("invalid flags");

        let client_name = VariableLengthString::deserialize(reader)?;

        Ok(Self {
            protocol_version,
            flags,
            client_name,
        })
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
