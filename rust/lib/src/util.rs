use std::io::Cursor;
use std::pin::Pin;

use tokio::io::{self, AsyncRead, AsyncReadExt};
use tokio::net::tcp::ReadHalf;

// Returns a read half that can be used to read from the connection if the connection is readable
// or None if the connection is not readable
pub fn check_readable<'a>(
    connection: ReadHalf<'a>,
) -> io::Result<Option<Pin<Box<dyn AsyncRead + 'a + Send>>>> {
    let mut buf = [0];
    match connection.try_read(&mut buf) {
        Err(e) if e.kind() == io::ErrorKind::WouldBlock => Ok(None), // false positive
        Ok(0) => Err(io::Error::new(io::ErrorKind::ConnectionAborted, "connection closed")),
        Ok(1) => {
            // This is a hack to fix the false positives from connection.readable()
            let read = Cursor::new(buf).chain(connection); // chain the byte to the socket
            Ok(Some(Box::pin(read)))
        }
        Ok(_) => unreachable!("unexpected amount of data read"),
        Err(e) => Err(e),
    }
}
