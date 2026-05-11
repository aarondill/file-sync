#include "FileDescriptor.h"
#include <cstring>
#include <istream>
#include <stdexcept>
#include <unistd.h>

FileDescriptor::FileDescriptor(const int fd) : fd{fd} {}
FileDescriptor::~FileDescriptor() {
  close();
}

void FileDescriptor::write(std::span<const std::byte> buffer) {
  while (!buffer.empty()) {
    const ssize_t ret = ::write(fd, buffer.data(), buffer.size_bytes());
    if (ret < 0) {
      if (errno == EINTR) continue;
      throw std::runtime_error(std::strerror(errno));
    }
    buffer = buffer.subspan(ret);
  }
}
void FileDescriptor::write_from(std::istream &is, size_t bytes) {
  char buffer[4096];
  while (bytes) {
    if (!is) throw std::runtime_error("could not read from input stream");
    is.read(buffer, std::size(buffer));
    if (is.bad()) throw std::runtime_error("could not read from input stream");
    const auto n_read = static_cast<size_t>(is.gcount());
    write(as_bytes(std::span{buffer, n_read}));
    bytes -= n_read;
  }
}

// Attempts to read the full buffer, may return with less data if read returns
// zero.
std::span<std::byte> FileDescriptor::read(const std::span<std::byte> buffer) {
  std::span<std::byte> rem = buffer;
  while (!rem.empty()) {
    const ssize_t ret = ::read(fd, rem.data(), rem.size_bytes());
    if (ret == 0) break;
    if (ret < 0) {
      if (errno == EINTR) continue;
      throw std::runtime_error(std::strerror(errno));
    }
    rem = rem.subspan(ret);
  }
  return buffer.first(buffer.size() - rem.size());
}
void FileDescriptor::read_to(std::ostream &os, size_t bytes) {
  char buffer[4096];
  while (bytes) {
    if (!os) throw std::runtime_error("could not write to output stream");
    auto got = read(as_writable_bytes(std::span{buffer}));

    os.write(buffer, static_cast<std::streamsize>(got.size()));
    if (os.bad()) throw std::runtime_error("could not write to output stream");

    bytes -= got.size();
  }
}

void FileDescriptor::close() {
  if (::close(fd) < 0) throw std::runtime_error("Close failed");
  fd = -1;
}

// move assign/construct
FileDescriptor::FileDescriptor(FileDescriptor &&in) noexcept {
  std::swap(fd, in.fd);
}
FileDescriptor &FileDescriptor::operator=(FileDescriptor &&in) noexcept {
  std::swap(fd, in.fd);
  return *this;
}
