#include "upload.h"
#include "protocol/structs.h"
#include "util.h"
#include <fstream>

void write_file_list(FileDescriptor &fd, std::span<const FileInfo> list, const fs::path *srcdir) {
  // send the file info
  std::byte buf[4096];
  for (const FileInfo &node : list) {
    auto name = node.path.string();
    assert(name.length() <= UINT8_MAX);
    assert(node.size <= UINT32_MAX);
    protocol::download_file_m f = {
        .hash = node.hash,
        // must be zero if not, since there's not a body
        .size = static_cast<uint32_t>(srcdir ? node.size : 0),
        .name_len = static_cast<uint8_t>(name.length()),
    };
    std::ranges::copy(name, f.name);

    auto end = serialize(buf, f);
    if (!end) throw std::runtime_error("error serializing download file\n");
    size_t len = std::distance(buf, end->data());
    write_message(fd, {buf, len});
  }

  if (!srcdir) return;
  // send file contents
  for (const FileInfo &node : list) {
    fs::path path = *srcdir / node.path;
    std::ifstream ifs{path};
    if (!ifs.is_open()) throw std::runtime_error("error opening file for reading\n");
    fd.write_from(ifs, node.size);
  }
}
// only reads the file contents if srcdir is not NULL
void write_download_message(FileDescriptor &fd, const std::span<const FileInfo> list,
                            const fs::path *srcdir) {
  const size_t file_count = list.size();
  assert(file_count <= UINT8_MAX);
  const protocol::download_m msg = {
      .flags = 0,
      .file_count = static_cast<uint8_t>(file_count),
  };
  std::byte buf[4096];
  const auto end = serialize(buf, msg);
  if (!end) throw std::runtime_error("error serializing download message\n");
  size_t len = std::distance(buf, end->data());
  write_message(fd, {buf, len});
  write_file_list(fd, list, srcdir);
}

// Sends an upload message to the server
void upload(FileDescriptor &sockfd, const std::span<const FileInfo> files, const fs::path &srcdir) {
  // send download message 1
  write_download_message(sockfd, files, nullptr);

  // receive download response
  std::byte buf[4096];
  const auto msg = read_message(sockfd, buf);
  protocol::download_response_m resp;
  if (!deserialize(resp, msg)) throw std::runtime_error("error deserializing download response");

  std::vector<FileInfo> filtered_list;
  filtered_list.reserve(resp.file_count);
  for (size_t i = 0; i < resp.file_count; i++) {
    auto &hash = resp.files[i];
    auto p = std::ranges::find_if(files, [&hash](const FileInfo &f) { return f.hash == hash; });
    if (p == files.end()) throw std::runtime_error("Requested file not in local list");
    filtered_list.emplace_back(*p);
  }

  // send download message 2
  write_download_message(sockfd, filtered_list, &srcdir);
}
