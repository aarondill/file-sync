use bitflags::bitflags;
use std::{io::Read, io::Write};

use crate::protocol::{Deserialize, Serialize};
const PROTOCOL_VERSION: u8 = 2;

bitflags! {
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub struct ClientConnectFlags: u8 {
        const IntentToUpload = 0b00000001;
        const _ = !0; // any other bits are reserved for future use
    }
}
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct ClientConnect {
    protocol_version: u8,
    flags: ClientConnectFlags,
    client_name_length: u8,
    client_name: [u8; 255],
}

impl ClientConnect {
    pub fn new(flags: ClientConnectFlags, client_name: &[u8]) -> Self {
        let client_name_length = client_name.len();
        assert!(client_name_length <= 255, "client name too long");
        let mut name = [0; 255];
        name[..client_name_length].copy_from_slice(client_name);

        Self {
            protocol_version: PROTOCOL_VERSION,
            flags,
            client_name_length: client_name_length as u8,
            client_name: name,
        }
    }
}

impl Serialize for ClientConnect {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Box<dyn std::error::Error>> {
        writer.write_all(&self.protocol_version.to_be_bytes())?;
        writer.write_all(&self.flags.bits().to_be_bytes())?;
        writer.write_all(&self.client_name_length.to_be_bytes())?;
        writer.write_all(&self.client_name[..self.client_name_length as usize])?;
        Ok(())
    }
}
impl Deserialize for ClientConnect {
    fn deserialize(reader: &mut dyn Read) -> Result<Self, Box<dyn std::error::Error>> {
        let mut bytes = reader.bytes();
        let protocol_version = bytes.next().expect("protocol version missing")?;
        if protocol_version != PROTOCOL_VERSION {
            return Err("protocol version mismatch".into());
        }

        let flags: u8 = bytes.next().expect("flags missing")?;
        let flags = ClientConnectFlags::from_bits(flags).expect("invalid flags");

        let client_name_length: u8 = bytes.next().expect("client name length missing")?;

        let mut client_name: [u8; 255] = [0; 255];
        reader.read_exact(&mut client_name[..client_name_length as usize])?;

        Ok(Self {
            protocol_version,
            flags,
            client_name_length,
            client_name,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn serialize_works() {
        let test = ClientConnect::new(ClientConnectFlags::IntentToUpload, "test".as_bytes());
        let mut bytes = Vec::<u8>::new();
        test.serialize(&mut bytes).expect("failed to serialize");

        let expected = [PROTOCOL_VERSION, 0b00000001, 4, b't', b'e', b's', b't'];
        assert_eq!(bytes, expected);
    }
    #[test]
    fn deserialize_works() {
        let bytes = [PROTOCOL_VERSION, 0b00000001, 4, b't', b'e', b's', b't'];
        let res = ClientConnect::deserialize(&mut &bytes[..]).expect("failed to serialize");

        let expected = ClientConnect::new(ClientConnectFlags::IntentToUpload, "test".as_bytes());
        assert_eq!(res, expected);
    }
    #[test]
    fn it_works() {
        let name = "this is a test of the client connect protocol";
        let test = ClientConnect::new(ClientConnectFlags::empty(), name.as_bytes());
        let mut bytes = Vec::<u8>::new();
        test.serialize(&mut bytes).expect("failed to serialize");

        let mut reader = std::io::Cursor::new(bytes);
        let test2 = ClientConnect::deserialize(&mut reader).expect("failed to deserialize");
        assert_eq!(test, test2);
    }
}
