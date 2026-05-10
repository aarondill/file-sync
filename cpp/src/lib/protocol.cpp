#include "protocol.h"
#include <algorithm>
#include <utility>
namespace protocol {
std::expected<CBuffer, serror> deserialize(char &out, const CBuffer buf) {
  if (buf.empty()) return std::unexpected(serror::NOT_ENOUGH_DATA);
  out = static_cast<char>(std::to_integer<unsigned char>(buf[0]));
  return buf.subspan(1);
}

std::expected<Buffer, serror> serialize(Buffer buf, const char &in) {
  if (buf.empty()) return std::unexpected(serror::NOT_ENOUGH_DATA);
  buf[0] = std::byte{static_cast<unsigned char>(in)};
  return buf.subspan(1);
}

std::expected<CBuffer, serror> deserialize(uint8_t &out, const CBuffer buf) {
  if (buf.size() < sizeof(out)) return std::unexpected(serror::NOT_ENOUGH_DATA);
  out = static_cast<uint8_t>(buf[0]);
  return buf.subspan(sizeof(out));
}
std::expected<Buffer, serror> serialize(const Buffer buf, const uint8_t &in) {
  if (buf.size() < sizeof(in)) return std::unexpected(serror::NOT_ENOUGH_DATA);
  buf[0] = static_cast<std::byte>(in);
  return buf.subspan(sizeof(in));
}

std::expected<CBuffer, serror> deserialize(uint32_t &out, const CBuffer buf) {
  if (buf.size() < sizeof(out)) return std::unexpected(serror::NOT_ENOUGH_DATA);
  out = static_cast<uint32_t>(buf[0]) | static_cast<uint32_t>(buf[1]) << 8 |
        static_cast<uint32_t>(buf[2]) << 16 | static_cast<uint32_t>(buf[3]) << 24;
  return buf.subspan(sizeof(out));
}
std::expected<Buffer, serror> serialize(Buffer buf, const uint32_t &in) {
  if (buf.size() < sizeof(in)) return std::unexpected(serror::NOT_ENOUGH_DATA);
  buf[0] = static_cast<std::byte>(in);
  buf[1] = static_cast<std::byte>(in >> 8);
  buf[2] = static_cast<std::byte>(in >> 16);
  buf[3] = static_cast<std::byte>(in >> 24);
  return buf.subspan(sizeof(in));
}

std::expected<CBuffer, serror> deserialize(uint16_t &out, CBuffer buf) {
  if (buf.size() < sizeof(out)) return std::unexpected(serror::NOT_ENOUGH_DATA);
  out = static_cast<uint16_t>(buf[0]) | static_cast<uint16_t>(buf[1]) << 8;
  return buf.subspan(sizeof(out));
}
std::expected<Buffer, serror> serialize(Buffer buf, const uint16_t &in) {
  if (buf.size() < sizeof(in)) return std::unexpected(serror::NOT_ENOUGH_DATA);
  buf[0] = static_cast<std::byte>(in);
  buf[1] = static_cast<std::byte>(in >> 8);
  return buf.subspan(sizeof(in));
}

std::expected<CBuffer, serror> deserialize(Hash &out, const CBuffer buf) {
  if (buf.size() < sizeof(out)) return std::unexpected(serror::NOT_ENOUGH_DATA);
  std::ranges::copy(buf.first(sizeof(out.hash)), out.hash);
  return buf.subspan(sizeof(out.hash));
}
std::expected<Buffer, serror> serialize(Buffer buf, const Hash &in) {
  if (buf.size() < sizeof(in)) return std::unexpected(serror::NOT_ENOUGH_DATA);
  std::ranges::copy(in.hash, std::begin(buf));
  return buf.subspan(sizeof(in.hash));
}

std::expected<CBuffer, serror> deserialize(client_connect_m &out, const CBuffer buf) {
  return deserialize(out.version, buf)
      .and_then([&out](CBuffer &&b) -> std::expected<CBuffer, serror> {
        if (out.version != client_connect_m::CLIENT_VERSION)
          return std::unexpected(serror::INVALID_PROTOCOL_VERSION);
        return b;
      })
      .and_then([&out](CBuffer &&b) { return deserialize(out.flags, b); })
      .and_then([&out](CBuffer &&b) { return deserialize(out.name_len, b); })
      .and_then([&out](CBuffer &&b) { return deserialize(std::span{out.name, out.name_len}, b); });
}
std::expected<Buffer, serror> serialize(const Buffer buf, const client_connect_m &in) {
  if (in.version != client_connect_m::CLIENT_VERSION)
    return std::unexpected(serror::INVALID_PROTOCOL_VERSION);
  return serialize(buf, in.version)
      .and_then([&in](Buffer &&b) { return serialize(b, in.flags); })
      .and_then([&in](Buffer &&b) { return serialize(b, in.name_len); })
      .and_then([&in](Buffer &&b) { return serialize(b, std::span{in.name, in.name_len}); });
}

std::expected<CBuffer, serror> deserialize(download_m &out, const CBuffer buf) {
  return deserialize(out.flags, buf)
      .and_then([&out](CBuffer &&b) -> std::expected<CBuffer, serror> {
        if (out.flags & std::to_underlying(flags::ERROR)) return std::unexpected(serror::IS_ERROR);
        return b;
      })
      .and_then([&out](CBuffer &&b) { return deserialize(out.file_count, b); });
}
std::expected<Buffer, serror> serialize(const Buffer buf, const download_m &in) {
  if (in.flags & std::to_underlying(flags::ERROR)) return std::unexpected(serror::IS_ERROR);
  return serialize(buf, in.flags) //
      .and_then([&in](Buffer &&b) { return serialize(b, in.file_count); });
}

std::expected<CBuffer, serror> deserialize(download_file_m &out, const CBuffer buf) {
  return deserialize(out.hash, buf)
      .and_then([&out](CBuffer &&b) { return deserialize(out.name_len, b); })
      .and_then([&out](CBuffer &&b) { return deserialize(out.size, b); })
      .and_then([&out](CBuffer &&b) { return deserialize(std::span{out.name, out.name_len}, b); });
}
std::expected<Buffer, serror> serialize(const Buffer buf, const download_file_m &in) {
  return serialize(buf, in.hash)
      .and_then([&in](Buffer &&b) { return serialize(b, in.name_len); })
      .and_then([&in](Buffer &&b) { return serialize(b, in.size); })
      .and_then([&in](Buffer &&b) { return serialize(b, std::span{in.name, in.name_len}); });
}

std::expected<CBuffer, serror> deserialize(download_response_m &out, const CBuffer buf) {
  return deserialize(out.flags, buf)
      .and_then([&out](CBuffer &&b) -> std::expected<CBuffer, serror> {
        if (out.flags & std::to_underlying(flags::ERROR)) return std::unexpected(serror::IS_ERROR);
        return b;
      })
      .and_then([&out](CBuffer &&b) { return deserialize(out.file_count, b); })
      .and_then(
          [&out](CBuffer &&b) { return deserialize(std::span{out.files, out.file_count}, b); });
}
std::expected<Buffer, serror> serialize(const Buffer buf, const download_response_m &in) {
  if (in.flags & std::to_underlying(flags::ERROR)) return std::unexpected(serror::IS_ERROR);
  return serialize(buf, in.flags)
      .and_then([&in](Buffer &&b) { return serialize(b, in.file_count); })
      .and_then([&in](Buffer &&b) { return serialize(b, std::span{in.files, in.file_count}); });
}

std::expected<CBuffer, serror> deserialize(error_m &out, CBuffer buf) {
  return deserialize(out.flags, buf)
      .and_then([&out](CBuffer &&b) { return deserialize(out.code, b); })
      .and_then([&out](CBuffer &&b) { return deserialize(out.message_len, b); })
      .and_then(
          [&out](CBuffer &&b) { return deserialize(std::span{out.message, out.message_len}, b); });
}
std::expected<Buffer, serror> serialize(const Buffer buf, const error_m &in) {
  return serialize(buf, in.flags)
      .and_then([&in](Buffer &&b) { return serialize(b, in.code); })
      .and_then([&in](Buffer &&b) { return serialize(b, in.message_len); })
      .and_then([&in](Buffer &&b) { return serialize(b, std::span{in.message, in.message_len}); });
}
} // namespace protocol