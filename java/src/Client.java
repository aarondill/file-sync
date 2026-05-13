import java.util.List;
import java.util.Map;
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

  // set to false to stop the client gracefully
  boolean running = true;

  record UserCommand(String description, Runnable command) {}

  Map<Character, UserCommand> commands = Map.of( //
      'q', new UserCommand("Quit", () -> running = false), // 
      'u', new UserCommand("Upload", () -> has_upload_pending = true), //
      'h', new UserCommand("Help", () -> printHelp()) //
  );

  private void printHelp() {
    System.out.println("Commands:");
    for (var entry : commands.entrySet())
      System.out.printf("  %c: %s\n", entry.getKey(), entry.getValue().description());
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
      while (running) {
        while (System.in.available() > 0) {
          char c = (char) System.in.read();
          if (Character.isWhitespace(c)) continue; // skip whitespace
          UserCommand cmd = commands.get(c);
          if (cmd != null) cmd.command().run();
          else System.out.println("Unknown command: " + c);
        }
        if (!running) break; // don't make them wait if they requested to quit
        if (in.available() > 0) download(in, out, handler.get_download_state());
        if (has_upload_pending) {
          handler.update_files(); // the files may have changed between the last time we checked and now
          upload(in, out, handler.get_download_state());
        }
        Thread.sleep(1000);
      }
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    } catch (InterruptedException e) {
      e.printStackTrace();
    } finally {
      System.out.println("Closing connection");
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
