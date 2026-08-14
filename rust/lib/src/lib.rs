use crate::protocol::{Deserialize, Serialize};
use std::io::Read;
pub mod client_connect;
pub mod download_file;
pub mod download_message;
pub mod download_response;
pub mod error;
pub mod file_hash;
pub mod file_info;
pub mod protocol;
pub mod variable_length_string;

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
