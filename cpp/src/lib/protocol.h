#pragma once
#include <cstddef>
#include <expected>
#include <md5.h>
#include <span>
namespace protocol {
typedef std::span<std::byte> Buffer;
typedef std::span<const std::byte> CBuffer;

// TODO: Move me?
struct Hash {
  std::byte hash[MD5_DIGEST_LENGTH];
};

// (De)serialization error
enum class serror {
  NO_ERROR,
  IS_ERROR, // tried to deserialize a message with the error flag set
  INVALID_PROTOCOL_VERSION,
  NOT_ENOUGH_DATA,
};

std::expected<CBuffer, serror> deserialize(char &out, CBuffer buf);
std::expected<Buffer, serror> serialize(Buffer buf, const char &in);

std::expected<CBuffer, serror> deserialize(uint32_t &out, CBuffer buf);
std::expected<Buffer, serror> serialize(Buffer buf, const uint32_t &in);

std::expected<CBuffer, serror> deserialize(uint16_t &out, CBuffer buf);
std::expected<Buffer, serror> serialize(Buffer buf, const uint16_t &in);

std::expected<CBuffer, serror> deserialize(Hash &out, CBuffer buf);
std::expected<Buffer, serror> serialize(Buffer buf, const Hash &in);

// -- Client Connect Message --
enum class client_connect_flags {
  INTENT_TO_UPLOAD = 1 << 0,
};

struct client_connect_m {
  // Refuse to deserialize if the version is different
  static constexpr uint8_t CLIENT_VERSION = 1;
  /*
   * - 8 bits for protocol version
   *   - The server must reject any connection with a different version.
   */
  uint8_t version;
  /* 8 bits of flags, see client_connect_flags */
  uint8_t flags;
  /* 8 bits for client name length in bytes */
  uint8_t name_len;
  /* human-readable client name (max of 255 bytes, variable length) */
  char name[255];
};

std::expected<CBuffer, serror> deserialize(client_connect_m &out, CBuffer buf);
std::expected<Buffer, serror> serialize(Buffer buf, const client_connect_m &in);

// -- Download Message --
enum class flags : uint8_t {
  /* Error */
  ERROR = 1 << 0,
};

/** Followed by file_count download_file_m's */
struct download_m {
  /* 8 bits for flags */
  uint8_t flags;
  /* 8 bits for file count */
  uint8_t file_count;
};
std::expected<CBuffer, serror> deserialize(download_m &out, CBuffer buf);
std::expected<Buffer, serror> serialize(Buffer buf, const download_m &in);

/** Followed by file_size bytes of file data */
struct download_file_m {
  /* 128 bits for file hash (MD5) */
  Hash hash;
  /* 8 bits for file name length in bytes */
  uint8_t name_len;
  /* 32 bits for file size in bytes */
  uint32_t size;
  /* File name (max of 255 bytes, variable length) */
  char name[255];
};
std::expected<CBuffer, serror> deserialize(download_file_m &out, CBuffer buf);
std::expected<Buffer, serror> serialize(Buffer buf, const download_file_m &in);

// -- Download Response Message --
struct download_response_m {
  /* 8 bits for flags */
  uint8_t flags;
  /* 8 bits for file count */
  uint8_t file_count;
  /* 128 bits for file hash (MD5) */
  Hash files[255];
};

std::expected<CBuffer, serror> deserialize(download_response_m &out, CBuffer buf);
std::expected<Buffer, serror> serialize(Buffer buf, const download_response_m &in);

// -- Error Message --
/* Values for error_m.code */
enum class error_code {
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
};

std::expected<CBuffer, serror> deserialize(error_m &out, CBuffer buf);
std::expected<Buffer, serror> serialize(Buffer buf, const error_m &in);

// must be after other definitions
template <typename T> std::expected<CBuffer, serror> deserialize(std::span<T> out, CBuffer buf) {
  std::expected<CBuffer, serror> r = buf;
  for (T &e : out) {
    r = deserialize(e, r.value());
    if (!r) return r;
  }
  return r;
}

template <typename T> std::expected<Buffer, serror> serialize(Buffer buf, std::span<T> in) {
  std::expected<Buffer, serror> r = buf;
  for (T &e : in) {
    r = serialize(r.value(), e);
    if (!r) return r;
  }
  return r;
}
} // namespace protocol