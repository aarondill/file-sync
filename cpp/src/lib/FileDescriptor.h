#pragma once
#include <cstddef>

// A socket wrapper that closes the file descriptor in its destructor
struct FileDescriptor {
  explicit FileDescriptor(int fd);
  ~FileDescriptor();
  // no copy operators
  FileDescriptor(FileDescriptor const &) = delete;
  FileDescriptor &operator=(const FileDescriptor &) = delete;
  // move operators
  FileDescriptor(FileDescriptor &&) noexcept;
  FileDescriptor &operator=(FileDescriptor &&) noexcept;

  void write(const void *buffer, size_t size);
  // NOTE: May read less than size bytes if read() returns 0
  size_t read(void *buffer, size_t size);
  void close();

private:
  int fd = -1;
};