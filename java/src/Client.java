import java.util.List;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.UncheckedIOException;
import java.net.Socket;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import sync.Sync;
import sync.SyncHandler;
import sync.messages.ClientConnect;

class Handler extends Sync {
  boolean initial_upload;

  public Handler(Socket s, SyncHandler handler, boolean initial_upload) throws IOException {
    super(s, handler);
    this.initial_upload = initial_upload;
  }
  @Override
  public void run() {
    try {
      // hacky way to get the hostname
      Process hostnameProcess = Runtime.getRuntime().exec(new String[] {"hostname"});
      BufferedReader stdInput = new BufferedReader(new InputStreamReader(hostnameProcess.getInputStream()));
      String hostname = stdInput.readLine();
      hostnameProcess.destroy();

      if (initial_upload) has_upload_pending = true;
      ClientConnect msg = initial_upload ? new ClientConnect(hostname, ClientConnect.Flags.INTENT_TO_UPLOAD)
          : new ClientConnect(hostname);
      this.writeMessage(msg);

      System.in.skip(System.in.available()); // clear input buffer
      while (true) {
        if (System.in.available() > 0) {
          char c = (char) System.in.read();
          if (c == 'q') break;
          if (c == 'u') has_upload_pending = true;
        }
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

public class Client extends SyncHandler {
  Client(Path directory) {
    super(directory);
  }

  public static void main(String[] args) throws IOException {
    // argument parsing
    List<String> flags = new ArrayList<>(), params = new ArrayList<>();
    for (int i = 0; i < args.length; i++) {
      if (args[i].equals("--")) {
        Arrays.stream(args, i + 1, args.length).forEach(params::add);
        break;
      } else if (args[i].startsWith("-")) {
        flags.add(args[i]);
      } else {
        params.add(args[i]);
      }
    }
    if (params.size() != 2) {
      System.err.println("usage: Client -u <server-url> <directory>");
      System.exit(2);
    }

    boolean upload = flags.contains("-u");
    String[] split = params.get(0).split(":");
    String server = split[0];
    int port = split.length > 1 ? Integer.parseInt(split[1]) : 8080;
    Path directory = Path.of(params.get(1));

    SyncHandler h = new Client(directory); // no threads here
    try (Socket socket = new Socket(server, port)) {
      new Handler(socket, h, upload).run();
    }
  }
}
