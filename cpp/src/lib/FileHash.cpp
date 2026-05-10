#include "FileHash.h"
using protocol::CBuffer, protocol::serror, protocol::Buffer;
std::expected<CBuffer, serror> FileHash::deserialize(FileHash &out, const CBuffer buf) {
  using protocol::deserialize;
  return deserialize(std::span{out.hash, std::size(out.hash)}, buf);
}
std::expected<Buffer, serror> FileHash::serialize(const Buffer buf, const FileHash &in) {
  using protocol::serialize;
  return serialize(buf, std::span{in.hash, std::size(in.hash)});
}
