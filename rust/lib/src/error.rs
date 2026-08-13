use bitflags::bitflags;
use std::{io::Read, io::Write};

use crate::protocol::{Deserialize, Serialize};

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
    message: String,
}

impl ProtocolError {
    pub fn new(flags: Flags, code: u8, message: &[u8]) -> Self {
        todo!()
    }
}

impl Serialize for ProtocolError {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Box<dyn std::error::Error>> {
        todo!()
    }
}
impl Deserialize for ProtocolError {
    fn deserialize(reader: &mut dyn Read) -> Result<Self, Box<dyn std::error::Error>> {
        todo!()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn serialize_works() {
        todo!()
    }
    #[test]
    fn deserialize_works() {
        todo!()
    }
    #[test]
    fn it_works() {
        todo!()
    }
}
