package sync.messages;

import sync.Serialize;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import sync.FileHash;

/** Followed by file_size bytes of file data */
public class DownloadFile implements Serialize {
  /* 128 bits for file hash (MD5) */
  FileHash hash;
  /* 32 bits for file size in bytes */
  long size;
  /* 8 bits for file name length in bytes */
  /* File name (max of 255 bytes, variable length) */
  String name;

  public DownloadFile(FileHash hash, long size, String name) {
    this.hash = hash;
    this.size = size;
    this.name = name;
  }
  public DownloadFile(DataInputStream s) throws IOException {
    hash = new FileHash(s);
    size = Integer.toUnsignedLong(s.readInt());
    int name_len = s.readUnsignedByte();
    byte[] name_bytes = new byte[name_len];
    s.readFully(name_bytes);
    name = new String(name_bytes);
  }
  @Override
  public void serialize(DataOutputStream s) throws IOException {
    if (size >= Math.pow(2, 32)) throw new IllegalArgumentException("File size too large");
    if (name.length() >= Math.pow(2, 8)) throw new IllegalArgumentException("File name too long");
    hash.serialize(s);
    s.writeInt((int) size);
    s.writeByte(name.length());
    s.writeBytes(name);
  }
}
