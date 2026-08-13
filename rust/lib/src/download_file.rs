use crate::protocol::{Deserialize, Serialize};
use std::{io::Read, io::Write};

#[derive(Debug, PartialEq, Eq, Clone)]
pub struct DownloadFile {
    // TODO: fields
}

impl DownloadFile {
    pub fn new() -> Self {
        todo!()
    }
}

impl Serialize for DownloadFile {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Box<dyn std::error::Error>> {
        todo!()
    }
}
impl Deserialize for DownloadFile {
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
