#pragma once
#include "protocol/serial.h"
#include <expected>
#include <filesystem>
#include <md5.h>

struct FileHash {
  uint8_t hash[MD5_DIGEST_LENGTH]{};
  static std::expected<protocol::CBuffer, protocol::serror> deserialize(FileHash &out,
                                                                        protocol::CBuffer buf);
  static std::expected<protocol::Buffer, protocol::serror> serialize(protocol::Buffer buf,
                                                                     const FileHash &in);
  friend std::ostream &operator<<(std::ostream &os, const FileHash &fh);
  bool operator==(const FileHash &) const;
  FileHash() = default;
  explicit FileHash(std::filesystem::path const &path);
};
static_assert(protocol::Serializable<FileHash> && protocol::Deserializable<FileHash>);
