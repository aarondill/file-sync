package sync;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;

public class FileHash implements Serialize {
  public byte[] hash;

  public FileHash(Path file) throws IOException {
    MessageDigest md;
    try {
      md = MessageDigest.getInstance("md5");
    } catch (NoSuchAlgorithmException e) {
      throw new AssertionError("md5 not supported"); // this isn't allowed
    }
    byte[] bytes = new byte[8092];
    try (InputStream in = Files.newInputStream(file)) {
      for (int n; (n = in.read(bytes)) != -1;)
        md.update(bytes, 0, n);
    }
    hash = md.digest();
    if (hash.length != 16) throw new IllegalArgumentException("Invalid hash length");
  }

  @Override
  public void serialize(DataOutputStream out) throws IOException {
    if (hash.length != 16) throw new IllegalArgumentException("Invalid hash length");
    out.write(hash);
  }
  public FileHash(DataInputStream in) throws IOException {
    hash = new byte[16];
    in.readFully(hash);
  }
  @Override
  public boolean equals(Object o) {
    if (this == o) return true;
    if (o == null || getClass() != o.getClass()) return false;
    return Arrays.equals(hash, ((FileHash) o).hash);
  }
  @Override
  public int hashCode() {
    return Arrays.hashCode(hash);
  }
  @Override
  public String toString() {
    StringBuilder sb = new StringBuilder(2 * hash.length);
    for (byte b : hash)
      sb.append(String.format("%02x", b));
    return sb.toString();
  }
}
