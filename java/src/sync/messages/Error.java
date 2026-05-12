package sync.messages;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import sync.Serialize;

class Error implements Serialize {
  static enum Code {
    TOO_MANY_CLIENTS // =0
  }

  public Code code;
  public String message;

  public Error(Code code, String message) {
    this.code = code;
    this.message = message;
  }
  @Override
  public void serialize(DataOutputStream out) throws IOException {
    if (message.length() > 255) throw new IllegalArgumentException("Message too long");
    out.writeByte(code.ordinal());
    out.writeByte(message.length());
    out.writeBytes(message);
  }
  public Error(DataInputStream in) throws IOException {
    code = Code.values()[in.readUnsignedByte()];
    int message_len = in.readUnsignedByte();
    byte[] message_bytes = new byte[message_len];
    in.readFully(message_bytes);
    message = new String(message_bytes);
  }
}
