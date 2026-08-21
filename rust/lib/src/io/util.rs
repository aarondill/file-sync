use std::io::Cursor;
use std::pin::Pin;

use tokio::io::{self, AsyncRead, AsyncReadExt};

pub trait TryRead: AsyncRead {
    fn try_read(&self, buf: &mut [u8]) -> io::Result<usize>;
}

macro_rules! impl_try_read {
    ($ty:ty) => {
        impl TryRead for $ty {
            fn try_read(&self, buf: &mut [u8]) -> io::Result<usize> {
                self.try_read(buf)
            }
        }
    };
}
impl_try_read!(tokio::net::tcp::ReadHalf<'_>);
impl_try_read!(tokio::net::tcp::OwnedReadHalf);
impl_try_read!(tokio::net::TcpStream);

// Returns a read half that can be used to read from the connection if the connection is readable
// or None if the connection is not readable
pub fn check_readable<'a>(
    connection: impl TryRead + Sync + Send + 'a,
) -> io::Result<Option<Pin<Box<dyn AsyncRead + 'a + Send + Sync>>>> {
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

#[cfg(test)]
mod tests {
    use std::sync::Arc;

    use futures::FutureExt;
    use tokio::io::AsyncWriteExt;
    use tokio::net::{TcpListener, TcpStream};

    use super::*;

    #[tokio::test]
    async fn test_check_readable() {
        let server = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let addr = server.local_addr().unwrap();

        let barrier = Arc::new(tokio::sync::Barrier::new(2)); // each (n) call will be left at the same time

        {
            let barrier = barrier.clone();
            tokio::spawn(async move {
                let (mut socket, _) = server.accept().await.unwrap();
                barrier.wait().await; // (1) wait for client to try reading

                socket.write_all(b"hello").await.unwrap();
                socket.flush().await.unwrap();

                barrier.wait().await; // (2) tell the client we're done writing

                barrier.wait().await; // (3) keep socket open until test is over
            });
        }

        let mut socket = TcpStream::connect(addr).await.unwrap();

        assert!(check_readable(socket.split().0).unwrap().is_none());

        barrier.wait().await; // (1) let the server write
        barrier.wait().await; // (2) wait until the server is done writing

        {
            let res = check_readable(socket.split().0).unwrap();
            assert!(res.is_some());
            let mut read = res.unwrap();

            let mut buf = [0; 5];
            read.read_exact(&mut buf).await.unwrap();
            assert_eq!(&buf, b"hello");
        }

        assert!(socket.readable().now_or_never().is_some()); // readable continues to return true until WouldBlock

        assert!(check_readable(socket.split().0).unwrap().is_none());
        barrier.wait().await; // (3) let the server close
    }
}
