package sync;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.nio.file.DirectoryNotEmptyException;
import java.nio.file.FileAlreadyExistsException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import sync.messages.Download;
import sync.messages.DownloadFile;
import sync.messages.DownloadResponse;

record DownloadState(List<FileInfo> files, Path directory) {
  DownloadState {
    files = Collections.unmodifiableList(files);
  }
}

class Util {
  // Tries to read size bytes from in, then writes them to out
  // throws IOException if in returns less than size bytes
  public static void transfer_bytes(InputStream in, OutputStream out, long size) throws IOException {
    byte[] buf = new byte[4096];
    while (size > 0) {
      int n = in.read(buf, 0, (int) Math.min(size, buf.length));
      if (n < 0) throw new IOException("unexpected EOF");
      out.write(buf, 0, n);
      size -= n;
    }
  }
}

public abstract class Sync implements Runnable {
  protected DataInputStream in;
  protected DataOutputStream out;

  protected boolean has_upload_pending = false;
  protected SyncHandler handler; // used to schedule other uploads

  abstract public void run();

  public Sync(DataInputStream in, DataOutputStream out, SyncHandler handler) throws IOException {
    this.in = in;
    this.out = out;
    this.handler = handler;
    handler.add(this);
  }
  public Sync(Socket socket, SyncHandler handler) throws IOException {
    var in = new DataInputStream(socket.getInputStream());
    var out = new DataOutputStream(socket.getOutputStream());
    this(in, out, handler);
  }

  // serializes message, then writes size and message to out
  protected void writeMessage(Serialize message) throws IOException {
    ByteArrayOutputStream baos = new ByteArrayOutputStream();
    { // serialize message
      DataOutputStream tmp = new DataOutputStream(baos);
      message.serialize(tmp);
      tmp.flush();
    }
    if (baos.size() >= Math.pow(2, 16)) throw new IOException("Message too large");
    // write message size
    out.writeShort(baos.size());
    // write message
    out.write(baos.toByteArray());
  }
  // reads size from in, then returns DataInputStream containing message
  protected DataInputStream readMessage() throws IOException {
    int size = in.readUnsignedShort();
    byte[] ret = in.readNBytes(size);
    if (ret.length != size) throw new IOException("Not enough bytes");
    return new DataInputStream(new ByteArrayInputStream(ret));
  }

  private List<FileInfo> readFileList(DataInputStream in, int file_count) throws IOException {
    List<FileInfo> ret = new ArrayList<>();
    for (int i = 0; i < file_count; i++) {
      var file = new DownloadFile(readMessage());
      ret.add(file.toFileInfo());
    }
    return ret;
  }

  public void download(DataInputStream in, DataOutputStream out, DownloadState ds) throws IOException {
    //  read the download message
    var download = new Download(readMessage());
    final List<FileInfo> recvlist = readFileList(in, download.file_count);

    // delete any files that the server didn't send us, but wait until the end to do this
    var to_delete = ds.files().stream().filter(f -> recvlist.stream().noneMatch(o -> o.name.equals(f.name))).toList();

    // write the download response
    // We only want files that we don't already have (either no path, or a different hash)
    var filtered = recvlist.stream()
        .filter(f -> ds.files().stream().noneMatch(o -> o.name.equals(f.name) && o.hash.equals(f.hash))).toList();
    writeMessage(new DownloadResponse(filtered));

    // Read download message 2
    var download2 = new Download(readMessage());
    var ret = readFileList(in, download2.file_count);
    // recv/write the file contents
    for (FileInfo file : ret) {
      Path path = ds.directory().resolve(file.name);
      try {
        Files.createDirectories(path.getParent());
      } catch (FileAlreadyExistsException _) { // this is fine
      }
      var file_stream = Files.newOutputStream(path);
      System.out.println("receiving " + path + ": " + file.hash);
      Util.transfer_bytes(in, file_stream, file.size);
      // TODO: verify hash (?)
    }

    // delete files that we don't need anymore
    for (FileInfo iter : to_delete) {
      Path p = ds.directory().resolve(iter.name);
      System.out.println("deleting " + p);
      // remove the file and parent directories
      while (!p.equals(ds.directory())) {
        try {
          Files.delete(p);
        } catch (DirectoryNotEmptyException e) {
          break;
        }
        p = p.getParent();
      }
    }
    if (ret.size() > 0 || to_delete.size() > 0) { // only schedule upload if we actually did something
      handler.schedule_upload(this);
    }
  }

  protected void upload(DataInputStream in, DataOutputStream out, DownloadState ds) throws IOException {
    has_upload_pending = false;
    // send download message 1
    writeMessage(new Download(ds.files().size()));
    for (FileInfo file : ds.files())
      writeMessage(new DownloadFile(file));

    // receive download response
    DownloadResponse resp = new DownloadResponse(readMessage());
    List<FileInfo> filtered_list = new ArrayList<>();
    for (FileHash hash : resp.files) {
      var p = ds.files().stream().filter(f -> f.hash.equals(hash)).findFirst();
      if (p.isEmpty()) throw new RuntimeException("Requested file not in local list");
      filtered_list.add(p.get());
    }

    // send download message 2
    writeMessage(new Download(filtered_list.size()));
    for (FileInfo file : filtered_list)
      writeMessage(new DownloadFile(file));
    // send file contents
    for (FileInfo f : filtered_list) {
      Path path = ds.directory().resolve(f.name);
      var file_stream = Files.newInputStream(path);
      System.out.println("sending " + path + ": " + f.hash);
      Util.transfer_bytes(file_stream, out, f.size);
      if (file_stream.read() != -1) throw new IOException("file too large");
    }
  }
}
