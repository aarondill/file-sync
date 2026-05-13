#pragma once
#include "../FileHash.h"
#include "../FileInfo.h"
#include "serial.h"

#include <assert.h>
#include <expected>
#include <ranges>
#include <span>
namespace protocol {

/** Followed by file_size bytes of file data */
struct download_file_m {
  /* 128 bits for file hash (MD5) */
  FileHash hash;
  /* 32 bits for file size in bytes */
  uint32_t size{};
  /* 8 bits for file name length in bytes */
  uint8_t name_len{};
  /* File name (max of 255 bytes, variable length) */
  char name[255]{};
  static std::expected<CBuffer, serror> deserialize(download_file_m &out, CBuffer buf);
  static std::expected<Buffer, serror> serialize(Buffer buf, const download_file_m &in);
};
static_assert(Serializable<download_file_m> && Deserializable<download_file_m>);

struct client_connect_m {
  // Refuse to deserialize if the version is different
  static constexpr uint8_t CLIENT_VERSION = 2;
  /*
   * - 8 bits for protocol version
   *   - The server must reject any connection with a different version.
   */
  uint8_t version = CLIENT_VERSION;
  /* 8 bits of flags, see client_connect_flags */
  uint8_t flags{};
  /* 8 bits for client name length in bytes */
  uint8_t name_len{};
  /* human-readable client name (max of 255 bytes, variable length) */
  char name[255]{};
  client_connect_m() = default;
  client_connect_m(const uint8_t flags, std::string_view name)
      : flags{flags}, name_len{static_cast<uint8_t>(name.length())} {
    assert(name.length() < UINT8_MAX);
    std::ranges::copy(name, this->name);
  }
  static std::expected<CBuffer, serror> deserialize(client_connect_m &out, CBuffer buf);
  static std::expected<Buffer, serror> serialize(Buffer buf, const client_connect_m &in);
};
static_assert(Serializable<client_connect_m> && Deserializable<client_connect_m>);

// -- Client Connect Message --
enum client_connect_flags : uint8_t {
  INTENT_TO_UPLOAD = 1 << 0,
};

// -- Download Message --
enum flags : uint8_t {
  /* Error */
  ERROR = 1 << 0,
};

/** Followed by file_count download_file_m's */
struct download_m {
  /* 8 bits for flags */
  uint8_t flags;
  /* 8 bits for file count */
  uint8_t file_count;
  static std::expected<CBuffer, serror> deserialize(download_m &out, CBuffer buf);
  static std::expected<Buffer, serror> serialize(Buffer buf, const download_m &in);
};
static_assert(Serializable<download_m> && Deserializable<download_m>);

// -- Download Response Message --
struct download_response_m {
  /* 8 bits for flags */
  uint8_t flags{};
  /* 8 bits for file count */
  uint8_t file_count{};
  /* 128 bits for file hash (MD5) */
  FileHash files[255]{};
  static std::expected<CBuffer, serror> deserialize(download_response_m &out, CBuffer buf);
  static std::expected<Buffer, serror> serialize(Buffer buf, const download_response_m &in);
  download_response_m() = default;
  explicit download_response_m(const std::span<const FileInfo> files, const uint8_t flags)
      : flags{flags}, file_count{static_cast<uint8_t>(files.size())} {
    assert(files.size() < std::size(this->files));
    std::ranges::copy(files | std::views::transform([](const FileInfo &file) { return file.hash; }),
                      this->files);
  }
  explicit download_response_m(const std::span<const FileInfo> files)
      : download_response_m(files, 0) {}
};
static_assert(Serializable<download_response_m> && Deserializable<download_response_m>);

// -- Error Message --
/* Values for error_m.code */
enum error_code {
  TOO_MANY_CLIENTS = 1,
};

struct error_m {
  /* 8 bits for flags */
  uint8_t flags;
  /* 8 bits for error code */
  uint8_t code;
  /* 8 bits for error message length in bytes */
  uint8_t message_len;
  /* variable length error message */
  char message[255];
  static std::expected<CBuffer, serror> deserialize(error_m &out, CBuffer buf);
  static std::expected<Buffer, serror> serialize(Buffer buf, const error_m &in);
};
static_assert(Serializable<error_m> && Deserializable<error_m>);
} // namespace protocol