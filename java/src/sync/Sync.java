package sync;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public class Sync {
  // serializes message, then writes size and message to out
  public void writeMessage(DataOutputStream out, Serialize message) throws IOException {
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
  public DataInputStream readMessage(DataInputStream in) throws IOException {
    int size = in.readUnsignedShort();
    byte[] ret = in.readNBytes(size);
    if (ret.length != size) throw new IOException("Not enough bytes");
    return new DataInputStream(new ByteArrayInputStream(ret));
  }
  public void download() {
    throw new UnsupportedOperationException("TODO"); // TODO:
  }
  public void upload() {
    throw new UnsupportedOperationException("TODO"); // TODO:
  }
}
