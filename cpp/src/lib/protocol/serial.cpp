#include "serial.h"
#include <utility>
namespace protocol {
std::expected<CBuffer, serror> deserialize(std::byte &out, CBuffer buf) {
  if (buf.empty()) return std::unexpected(serror::NOT_ENOUGH_DATA);
  out = buf[0];
  return buf.subspan(1);
}
std::expected<Buffer, serror> serialize(Buffer buf, const std::byte &in) {
  if (buf.empty()) return std::unexpected(serror::NOT_ENOUGH_DATA);
  buf[0] = static_cast<std::byte>(in);
  return buf.subspan(1);
}

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
} // namespace protocol
