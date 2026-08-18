use crate::serial::{Deserialize, Serialize};
use std::{borrow::Borrow, io::Read};

pub fn read_message(reader: &mut dyn Read) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
    let len = u16::deserialize(reader)? as usize;
    let mut buffer = vec![0u8; len];
    reader.read_exact(&mut buffer)?;
    Ok(buffer)
}
pub fn write_message(
    writer: &mut dyn std::io::Write,
    message: &[u8],
) -> Result<(), Box<dyn std::error::Error>> {
    let len: u16 = message.len().try_into()?;
    len.serialize(writer)?;
    writer.write_all(message)?;
    Ok(())
}
