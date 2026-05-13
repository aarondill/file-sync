import java.io.IOException;
import java.io.UncheckedIOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.file.Path;
import sync.Sync;
import sync.SyncHandler;

class ClientHandler extends Sync {
  public ClientHandler(Socket s, SyncHandler handler) throws IOException {
    super(s, handler);
  }
  @Override
  public void run() {
    try {
      while (true) {
        if (in.available() > 0) download(in, out, handler.get_download_state());
        if (has_upload_pending) upload(in, out, handler.get_download_state());
        Thread.sleep(1000);
      }
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    } catch (InterruptedException e) {
      e.printStackTrace();
    } finally {
      handler.remove(this);
    }
  }
}

public class Server extends SyncHandler {
  Server(Path directory) {
    super(directory);
  }

  public static void main(String[] args) throws IOException {
    if (args.length < 1 || args.length > 2) {
      System.err.println("usage: Server <directory> [port]");
      System.exit(2);
    }
    int port = args.length >= 2 ? Integer.parseInt(args[1]) : 8080;

    Server server = new Server(Path.of(args[0]));
    try (ServerSocket listener = new ServerSocket(port)) {
      listener.setReuseAddress(true);
      System.out.println("Listening on port " + listener.getLocalPort());
      while (true) {
        // Wait for a connection
        System.out.println("Waiting for connection...");
        Socket socket = listener.accept();
        System.out.println("Connection received from " + socket.getRemoteSocketAddress());
        // Handle the connection
        new Thread(new ClientHandler(socket, server)).start();
      }
    }
  }
}
