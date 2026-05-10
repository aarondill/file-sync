#include "Socket.h"
#include <cstring>
#include <stdexcept>
#include <unistd.h>

Socket::Socket(const int fd) : fd{fd} {}
Socket::~Socket() { close(); }

void Socket::write(const void *buffer, const size_t size) {
  size_t rem = size;
  while (rem) {
    const ssize_t ret = ::write(fd, buffer, rem);
    if (ret < 0) {
      if (errno == EINTR) continue;
      throw std::runtime_error(std::strerror(errno));
    }
    rem -= ret;
    buffer = static_cast<const char *>(buffer) + ret;
  }
}

// Attempts to read the full buffer, may return with less data if read returns
// zero.
size_t Socket::read(void *buffer, const size_t size) {
  size_t got = 0;
  while (got < size) {
    const ssize_t ret = ::read(fd, buffer, size - got);
    if (ret == 0) break;
    if (ret < 0) {
      if (errno == EINTR) continue;
      throw std::runtime_error(std::strerror(errno));
    }
    got += ret;
    buffer = static_cast<char *>(buffer) + ret;
  }
  return got;
}

void Socket::close() {
  if (::close(fd) < 0) throw std::runtime_error("Close failed");
  fd = -1;
}

// move assign/construct
Socket::Socket(Socket &&in) noexcept { std::swap(fd, in.fd); }
Socket &Socket::operator=(Socket &&in) noexcept {
  std::swap(fd, in.fd);
  return *this;
}