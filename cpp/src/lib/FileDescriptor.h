#pragma once
#include <cstddef>
#include <iosfwd>
#include <span>

// A socket wrapper that closes the file descriptor in its destructor
struct FileDescriptor {
  explicit FileDescriptor() = default;
  explicit FileDescriptor(int fd);
  ~FileDescriptor();
  // no copy operators
  FileDescriptor(FileDescriptor const &) = delete;
  FileDescriptor &operator=(const FileDescriptor &) = delete;
  // move operators
  FileDescriptor(FileDescriptor &&) noexcept;
  FileDescriptor &operator=(FileDescriptor &&) noexcept;
  bool operator==(const FileDescriptor &) const = default;

  void write(std::span<const std::byte> buffer);
  // reads bytes from is to the fd
  void write_from(std::istream &is, size_t bytes);
  // NOTE: May read less than size bytes if read() returns 0
  std::span<std::byte> read(std::span<std::byte> buffer);
  // writes bytes from fd to os
  void read_to(std::ostream &os, size_t bytes);
  void close();
  explicit operator int() const { return fd; }

private:
  int fd = -1;
};