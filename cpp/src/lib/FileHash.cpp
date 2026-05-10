#include "FileHash.h"
#include <fstream>
using protocol::CBuffer, protocol::serror, protocol::Buffer;
std::expected<CBuffer, serror> FileHash::deserialize(FileHash &out, const CBuffer buf) {
  using protocol::deserialize;
  return deserialize(std::span{out.hash, std::size(out.hash)}, buf);
}
std::expected<Buffer, serror> FileHash::serialize(const Buffer buf, const FileHash &in) {
  using protocol::serialize;
  return serialize(buf, std::span{in.hash, std::size(in.hash)});
}
FileHash::FileHash(std::filesystem::path const &path) : hash{} {
  std::ifstream ifs{path};
  if (!ifs.is_open()) throw std::runtime_error{"Cannot open file"};
  ifs.exceptions(std::ios_base::failbit); // throw if there's a read error

  MD5_CTX ctx;
  MD5Init(&ctx);

  uint8_t data[1024];
  while (ifs) { // false at EOF
    ifs.read(reinterpret_cast<char *>(data), std::size(data));
    MD5Update(&ctx, data, ifs.gcount());
  }

  uint8_t result[MD5_DIGEST_LENGTH];
  MD5Final(result, &ctx);
  std::ranges::copy(as_bytes(std::span{result}), hash);
}
