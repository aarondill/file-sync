use std::io;

use tokio::io::{AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt};

#[derive(Debug, thiserror::Error)]
pub enum ProtocolError {
    #[error(transparent)]
    Io(#[from] std::io::Error),
    #[error("message too long")]
    MessageTooLong,
}

pub async fn read_message(reader: &mut (dyn AsyncRead + Unpin + Send)) -> io::Result<Vec<u8>> {
    let len = reader.read_u16().await? as usize;
    let mut buffer = vec![0u8; len];
    reader.read_exact(&mut buffer).await?;
    Ok(buffer)
}
pub async fn write_message(
    writer: &mut (dyn AsyncWrite + Unpin + Send),
    message: &[u8],
) -> Result<(), ProtocolError> {
    let len: u16 = message.len().try_into().map_err(|| ProtocolError::MessageTooLong)?;
    writer.write_u16(len).await?;
    writer.write_all(message).await?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[tokio::test]
    async fn test_read_message() {
        let data = vec![0x00, 0x04, 0x04, 0x03, 0x02, 0x01];
        let msg = read_message(&mut &data[..]).await.unwrap();
        assert_eq!(msg, vec![0x04, 0x03, 0x02, 0x01]);
    }
    #[tokio::test]
    async fn test_write_message() {
        let data = vec![0x04, 0x03, 0x02, 0x01];
        let mut buf = Vec::new();
        write_message(&mut buf, &data).await.unwrap();
        assert_eq!(buf, vec![0x0, 0x4, 0x04, 0x03, 0x02, 0x01]);
    }
}
