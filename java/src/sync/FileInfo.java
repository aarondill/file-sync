package sync;

import java.util.List;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

public class FileInfo {
  public Path name; // a relative path, relative to the base directory
  public FileHash hash;
  public long size;

  public FileInfo(Path name, FileHash hash, long size) {
    this.name = name;
    this.hash = hash;
    this.size = size;
  }
  public FileInfo(Path path, Path base) throws IOException {
    this.name = base.relativize(path);
    this.size = Files.size(path);
    this.hash = new FileHash(path);
  }
  // Reads the directory and returns a list of all files in the directory
  static List<FileInfo> readList(Path dir) throws IOException {
    try (var s = Files.find(dir, Integer.MAX_VALUE, (p, a) -> a.isRegularFile())) {
      return s.map(p -> {
        try {
          return new FileInfo(p, dir);
        } catch (IOException e) {
          throw new RuntimeException(e); // this shouldn't happen, since the file must exist to get here
        }
      }).toList();
    }
  }

  @Override
  public boolean equals(Object o) {
    if (this == o) return true;
    if (o == null || getClass() != o.getClass()) return false;
    FileInfo fileInfo = (FileInfo) o;
    return name.equals(fileInfo.name) && hash.equals(fileInfo.hash) && size == fileInfo.size;
  }
};
