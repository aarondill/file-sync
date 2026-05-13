package sync.messages;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.util.Arrays;
import java.util.EnumSet;
import java.util.stream.Stream;
import sync.Serialize;

// -- Client Connect Message --
public class ClientConnect implements Serialize {
  public static enum Flags implements Bitfield {
    INTENT_TO_UPLOAD; // =0
  }

  static final int CLIENT_VERSION = 1;
  public int version = CLIENT_VERSION;
  public EnumSet<Flags> flags = EnumSet.noneOf(Flags.class);
  public String name;

  public ClientConnect(String name, Flags... flags) {
    this.name = name;
    Arrays.stream(flags).forEach(this.flags::add);
  }
  @Override
  public void serialize(DataOutputStream s) throws IOException {
    if (name.length() > 255) throw new IllegalArgumentException("Name too long");
    if (name.chars().anyMatch(c -> c > Character.MAX_VALUE))
      throw new IllegalArgumentException("Name contains invalid characters");
    if (version != CLIENT_VERSION)
      throw new IllegalArgumentException("Unsupported version serialize. This shouldn't be possible.");

    /*
     * - 8 bits for protocol version (unsigned)
     * - The server must reject any connection with a different version.
     */
    s.writeByte(version);
    /* 8 bits of flags */
    s.writeByte(flags.stream().mapToInt(Flags::bitval).sum());
    /* 8 bits for client name length in bytes */
    s.writeByte(name.length());
    /* human-readable client name (max of 255 bytes, variable length) */
    s.writeBytes(name);
  }
  public ClientConnect(DataInputStream s) throws IOException {
    version = s.readUnsignedByte();
    if (version != CLIENT_VERSION) throw new IllegalArgumentException("Unsupported version");

    int flags = s.readUnsignedByte();
    this.flags = EnumSet.noneOf(Flags.class);
    Stream.of(Flags.values()).filter(flag -> (flags & flag.bitval()) != 0).forEach(this.flags::add);

    int name_len = s.readUnsignedByte();

    byte[] name_bytes = new byte[name_len];
    s.readFully(name_bytes);
    name = new String(name_bytes);
  }
}
