// -- Download Message --
package sync.messages;

import sync.Serialize;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.util.EnumSet;
import java.util.stream.Stream;

/** Followed by file_count download_file_m's */
class Download implements Serialize {
  static enum Flags implements Bitfield {
    ERROR; // =0
  };

  public EnumSet<Flags> flags;
  public int file_count;

  public Download(EnumSet<Flags> flags, int file_count) {
    this.flags = flags;
    this.file_count = file_count;
  }

  @Override
  public void serialize(DataOutputStream out) throws IOException {
    //* 8 bits for flags */
    out.writeByte(flags.stream().mapToInt(Bitfield::bitval).sum());
    if (file_count > 255) throw new IllegalArgumentException("too many files");
    //* 8 bits for file count */
    out.writeByte(file_count);
  }
  public Download(DataInputStream in) throws IOException {
    int flags = in.readUnsignedByte();
    this.flags = EnumSet.noneOf(Flags.class);
    Stream.of(Flags.values()).filter(flag -> (flags & flag.bitval()) != 0).forEach(this.flags::add);

    file_count = in.readUnsignedByte();
  }
}
