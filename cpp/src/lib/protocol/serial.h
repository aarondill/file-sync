#pragma once
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
namespace protocol {
typedef std::span<std::byte> Buffer;
typedef std::span<const std::byte> CBuffer;

// (De)serialization error
enum class serror {
  NO_ERROR,
  IS_ERROR, // tried to deserialize a message with the error flag set
  INVALID_PROTOCOL_VERSION,
  NOT_ENOUGH_DATA,
};

std::expected<CBuffer, serror> deserialize(std::byte &out, CBuffer buf);
std::expected<Buffer, serror> serialize(Buffer buf, const std::byte &in);

static_assert(sizeof(char) == sizeof(std::byte));
std::expected<CBuffer, serror> deserialize(char &out, CBuffer buf);
std::expected<Buffer, serror> serialize(Buffer buf, const char &in);

std::expected<CBuffer, serror> deserialize(uint8_t &out, CBuffer buf);
std::expected<Buffer, serror> serialize(Buffer buf, const uint8_t &in);

std::expected<CBuffer, serror> deserialize(uint16_t &out, CBuffer buf);
std::expected<Buffer, serror> serialize(Buffer buf, const uint16_t &in);

std::expected<CBuffer, serror> deserialize(uint32_t &out, CBuffer buf);
std::expected<Buffer, serror> serialize(Buffer buf, const uint32_t &in);

template <typename T>
concept Serializable = requires(T x, Buffer buf) {
  { T::serialize(buf, x) } -> std::convertible_to<std::expected<Buffer, serror>>;
};
template <typename T>
concept Deserializable = requires(T x, CBuffer buf) {
  { T::deserialize(x, buf) } -> std::convertible_to<std::expected<CBuffer, serror>>;
};

/* An overload to call the static deserialize method */
template <typename T>
  requires Deserializable<T>
std::expected<CBuffer, serror> deserialize(T out, CBuffer buf) {
  return T::deserialize(out, buf);
}

/* An overload to call the static serialize method */
template <typename T>
  requires Serializable<T>
std::expected<Buffer, serror> serialize(Buffer buf, T in) {
  return T::serialize(buf, in);
}

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