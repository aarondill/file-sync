#pragma once
#include <cstddef>

// A socket wrapper that closes the file descriptor in its destructor
struct Socket {
  explicit Socket(int fd);
  ~Socket();
  // no copy operators
  Socket(Socket const &) = delete;
  Socket &operator=(const Socket &) = delete;
  // move operators
  Socket(Socket &&) noexcept;
  Socket &operator=(Socket &&) noexcept;

  void write(const void *buffer, size_t size);
  // NOTE: May read less than size bytes if read() returns 0
  size_t read(void *buffer, size_t size);
  void close();

private:
  int fd = -1;
};