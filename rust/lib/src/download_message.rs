use bitflags::bitflags;
use std::{io::Read, io::Write};

use crate::protocol::{Deserialize, Serialize};

bitflags! {
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub struct Flags: u8 {
        const isError = 0b00000001; /// must not be set!
        const _ = !0; // any other bits are reserved for future use
    }
}
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct DownloadMessage {
    // TODO: fields
}

impl DownloadMessage {
    pub fn new() -> Self {
        todo!()
    }
}

impl Serialize for DownloadMessage {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Box<dyn std::error::Error>> {
        todo!()
    }
}
impl Deserialize for DownloadMessage {
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
