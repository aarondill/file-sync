#include "structs.h"
#include "serial.h"
#include <utility>
namespace protocol {
std::expected<CBuffer, serror> client_connect_m::deserialize(client_connect_m &out,
                                                             const CBuffer buf) {
  using protocol::deserialize;
  return deserialize(out.version, buf)
      .and_then([&out](CBuffer &&b) -> std::expected<CBuffer, serror> {
        if (out.version != CLIENT_VERSION) return std::unexpected(serror::INVALID_PROTOCOL_VERSION);
        return b;
      })
      .and_then([&out](CBuffer &&b) { return deserialize(out.flags, b); })
      .and_then([&out](CBuffer &&b) { return deserialize(out.name_len, b); })
      .and_then([&out](CBuffer &&b) { return deserialize(std::span{out.name, out.name_len}, b); });
}
std::expected<Buffer, serror> client_connect_m::serialize(const Buffer buf,
                                                          const client_connect_m &in) {
  if (in.version != CLIENT_VERSION) return std::unexpected(serror::INVALID_PROTOCOL_VERSION);
  using protocol::serialize;
  return serialize(buf, in.version)
      .and_then([&in](Buffer &&b) { return serialize(b, in.flags); })
      .and_then([&in](Buffer &&b) { return serialize(b, in.name_len); })
      .and_then([&in](Buffer &&b) { return serialize(b, std::span{in.name, in.name_len}); });
}

std::expected<CBuffer, serror> download_m::deserialize(download_m &out, const CBuffer buf) {
  using protocol::deserialize;
  return deserialize(out.flags, buf)
      .and_then([&out](CBuffer &&b) -> std::expected<CBuffer, serror> {
        if (out.flags & std::to_underlying(flags::ERROR)) return std::unexpected(serror::IS_ERROR);
        return b;
      })
      .and_then([&out](CBuffer &&b) { return deserialize(out.file_count, b); });
}
std::expected<Buffer, serror> download_m::serialize(const Buffer buf, const download_m &in) {
  using protocol::serialize;
  if (in.flags & std::to_underlying(flags::ERROR)) return std::unexpected(serror::IS_ERROR);
  return serialize(buf, in.flags) //
      .and_then([&in](Buffer &&b) { return serialize(b, in.file_count); });
}

std::expected<CBuffer, serror> download_file_m::deserialize(download_file_m &out,
                                                            const CBuffer buf) {
  using protocol::deserialize;
  return deserialize(out.hash, buf)
      .and_then([&out](CBuffer &&b) { return deserialize(out.name_len, b); })
      .and_then([&out](CBuffer &&b) { return deserialize(out.size, b); })
      .and_then([&out](CBuffer &&b) { return deserialize(std::span{out.name, out.name_len}, b); });
}
std::expected<Buffer, serror> download_file_m::serialize(const Buffer buf,
                                                         const download_file_m &in) {
  using protocol::serialize;
  return serialize(buf, in.hash)
      .and_then([&in](Buffer &&b) { return serialize(b, in.name_len); })
      .and_then([&in](Buffer &&b) { return serialize(b, in.size); })
      .and_then([&in](Buffer &&b) { return serialize(b, std::span{in.name, in.name_len}); });
}

std::expected<CBuffer, serror> download_response_m::deserialize(download_response_m &out,
                                                                const CBuffer buf) {
  using protocol::deserialize;
  return deserialize(out.flags, buf)
      .and_then([&out](CBuffer &&b) -> std::expected<CBuffer, serror> {
        if (out.flags & std::to_underlying(flags::ERROR)) return std::unexpected(serror::IS_ERROR);
        return b;
      })
      .and_then([&out](CBuffer &&b) { return deserialize(out.file_count, b); })
      .and_then(
          [&out](CBuffer &&b) { return deserialize(std::span{out.files, out.file_count}, b); });
}
std::expected<Buffer, serror> download_response_m::serialize(const Buffer buf,
                                                             const download_response_m &in) {
  if (in.flags & std::to_underlying(flags::ERROR)) return std::unexpected(serror::IS_ERROR);
  using protocol::serialize;
  return serialize(buf, in.flags)
      .and_then([&in](Buffer &&b) { return serialize(b, in.file_count); })
      .and_then([&in](Buffer &&b) { return serialize(b, std::span{in.files, in.file_count}); });
}

std::expected<CBuffer, serror> error_m::deserialize(error_m &out, CBuffer buf) {
  using protocol::deserialize;
  return deserialize(out.flags, buf)
      .and_then([&out](CBuffer &&b) { return deserialize(out.code, b); })
      .and_then([&out](CBuffer &&b) { return deserialize(out.message_len, b); })
      .and_then(
          [&out](CBuffer &&b) { return deserialize(std::span{out.message, out.message_len}, b); });
}
std::expected<Buffer, serror> error_m::serialize(const Buffer buf, const error_m &in) {
  using protocol::serialize;
  return serialize(buf, in.flags)
      .and_then([&in](Buffer &&b) { return serialize(b, in.code); })
      .and_then([&in](Buffer &&b) { return serialize(b, in.message_len); })
      .and_then([&in](Buffer &&b) { return serialize(b, std::span{in.message, in.message_len}); });
}
} // namespace protocol