use crate::serial::{Deserialize, Serialize};
use std::io::Read;

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

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn test_read_message() {
        let data = vec![4, 0x04, 0x03, 0x02, 0x01];
        let msg = read_message(&mut &data[..]).unwrap();
        assert_eq!(msg, vec![0x04, 0x03, 0x02, 0x01]);
    }
    #[test]
    fn test_write_message() {
        let data = vec![0x04, 0x03, 0x02, 0x01];
        let mut buf = Vec::new();
        write_message(&mut buf, &data).unwrap();
        assert_eq!(buf, vec![4, 0x03, 0x02, 0x01]);
    }
}
