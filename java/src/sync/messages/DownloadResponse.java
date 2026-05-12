package sync.messages;

import java.io.DataOutputStream;
import java.io.IOException;
import java.util.EnumSet;
import sync.FileHash;
import sync.Serialize;

public class DownloadResponse implements Serialize {
  public EnumSet<Download.Flags> flags;
  public int file_count;
  public FileHash[] files;

  public DownloadResponse(EnumSet<Download.Flags> flags, int file_count, FileHash[] files) {
    this.flags = flags;
    this.file_count = file_count;
    this.files = files;
  }

  @Override
  public void serialize(DataOutputStream out) throws IOException {
    /* 8 bits for flags */
    out.writeByte(flags.stream().mapToInt(Bitfield::bitval).sum());
    /* 8 bits for file count */
    if (file_count > 255) throw new IllegalArgumentException("too many files");
    out.writeByte(file_count);
    // each:
    /* 128 bits for file hash (MD5) */
    for (FileHash hash : files)
      hash.serialize(out);
  }

}
