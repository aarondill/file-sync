package sync;

import java.io.IOException;
import java.io.UncheckedIOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Collection;
import java.util.List;
import java.util.concurrent.ConcurrentLinkedQueue;

// A class that maintains a list of Syncs and shares state between them
public class SyncHandler {
  List<FileInfo> files;
  Path directory;
  Collection<Sync> syncs = new ConcurrentLinkedQueue<Sync>();

  void update_files() {
    List<FileInfo> new_files;
    try {
      new_files = FileInfo.readList(directory);
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
    // TODO: lock the files before assigning
    files = new_files;
  }
  public SyncHandler(Path directory) {
    this.directory = directory;
    if (!Files.isDirectory(directory)) throw new IllegalArgumentException("not a directory");
    if (!Files.isReadable(directory) || !Files.isWritable(directory))
      throw new IllegalArgumentException("directory is not readable or writable");
    update_files();
  }
  public void add(Sync sync) {
    syncs.add(sync);
  }
  public void remove(Sync sync) {
    syncs.remove(sync);
  }
  public DownloadState get_download_state() {
    return new DownloadState(files, directory);
  }
  public void schedule_upload(Sync except) {
    update_files();
    for (Sync sync : syncs) {
      if (sync == except) continue;
      sync.has_upload_pending = true;
    }
  }
}
