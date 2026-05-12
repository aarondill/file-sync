package sync.messages;

import java.util.List;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.util.EnumSet;
import java.util.stream.Stream;
import sync.FileHash;
import sync.FileInfo;
import sync.Serialize;

public class DownloadResponse implements Serialize {
  public EnumSet<Download.Flags> flags = EnumSet.noneOf(Download.Flags.class);
  public FileHash[] files;

  public DownloadResponse(List<FileInfo> files) {
    if (files.size() > 255) throw new IllegalArgumentException("too many files");
    this.files = files.stream().map(f -> f.hash).toArray(FileHash[]::new);
  }

  public DownloadResponse(List<FileInfo> files, EnumSet<Download.Flags> flags) {
    this(files);
    this.flags.addAll(flags);
  }

  public DownloadResponse(DataInputStream in) throws IOException {
    in.mark(1); // we may need to reset the stream back one byte
    int flags = in.readUnsignedByte();
    Stream.of(Download.Flags.values()).filter(flag -> (flags & flag.bitval()) != 0).forEach(this.flags::add);
    if (this.flags.contains(Download.Flags.ERROR)) {
      in.reset();
      throw new Error.IsError();
    }

    int file_count = in.readUnsignedByte();

    this.files = new FileHash[file_count];
    for (int i = 0; i < file_count; i++)
      this.files[i] = new FileHash(in);
  }

  @Override
  public void serialize(DataOutputStream out) throws IOException {
    /* 8 bits for flags */
    out.writeByte(flags.stream().mapToInt(Bitfield::bitval).sum());
    /* 8 bits for file count */
    int file_count = files.length;
    if (file_count > 255) throw new IllegalArgumentException("too many files");
    out.writeByte(file_count);
    // each:
    /* 128 bits for file hash (MD5) */
    for (FileHash hash : files)
      hash.serialize(out);
  }

}
