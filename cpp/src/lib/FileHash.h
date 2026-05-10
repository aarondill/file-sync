#pragma once
#include "protocol/serial.h"
#include <cstddef>
#include <expected>
#include <filesystem>
#include <md5.h>

struct FileHash {
  static_assert(sizeof(std::byte) == sizeof(uint8_t));
  std::byte hash[MD5_DIGEST_LENGTH];
  static std::expected<protocol::CBuffer, protocol::serror> deserialize(FileHash &out,
                                                                        protocol::CBuffer buf);
  static std::expected<protocol::Buffer, protocol::serror> serialize(protocol::Buffer buf,
                                                                     const FileHash &in);
  explicit FileHash(std::filesystem::path const &path);
};