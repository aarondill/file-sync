#pragma once
#include "FileDescriptor.h"
#include "protocol/serial.h"
#include <cassert>
#include <cstdint>
#include <stdexcept>

inline void write_message(FileDescriptor &sockfd, const std::span<const std::byte> buf) {
  assert(buf.size_bytes() <= UINT16_MAX);
  std::byte length_buffer[sizeof(uint16_t)];
  if (!protocol::serialize(length_buffer, static_cast<uint16_t>(buf.size_bytes())))
    throw std::runtime_error("error serializing message length\n");
  sockfd.write(length_buffer); // write length

  sockfd.write(buf); // write message
}
inline std::span<std::byte> read_message(FileDescriptor &sockfd, const std::span<std::byte> buf) {
  std::byte length_buffer[sizeof(uint16_t)];
  const auto lb = sockfd.read(length_buffer);

  uint16_t len;
  if (!protocol::deserialize(len, lb))
    throw std::runtime_error("error deserializing message length\n");

  if (len > buf.size_bytes()) throw std::runtime_error("not enough space in buffer");
  return sockfd.read(buf.first(len));
}
