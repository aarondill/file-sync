#include "download.h"
#include "protocol/structs.h"
#include "util.h"

#include <algorithm>
#include <cassert>
#include <expected>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <vector>
// if destdir is non-null, the file contents will be read and written to
// disk
std::vector<FileInfo> read_file_list(FileDescriptor &fd, size_t file_count,
                                     const fs::path *destdir) {
  std::vector<FileInfo> list;
  { // recv the file info
    std::byte buf[4096];
    for (size_t i = 0; i < file_count; i++) {
      auto msg = read_message(fd, buf);
      protocol::download_file_m f;
      if (!protocol::deserialize(f, msg))
        throw std::runtime_error("error deserializing download file");
      list.emplace_back(std::string_view{f.name, f.name_len}, f.hash, f.size);
    }
  }
  assert(file_count == list.size());
  if (destdir) {
    // recv/write the file contents
    for (const FileInfo &iter : list) {
      const fs::path path = *destdir / iter.path; // resolve path according to destdir
      if (!fs::create_directories(path.parent_path()))
        throw std::runtime_error("error creating directory");
      std::ofstream ofs{path, std::ios::binary};
      if (!ofs) throw std::runtime_error("error opening file for writing\n");
      std::cout << "receiving " << path << ": " << iter.hash << std::endl;
      fd.read_to(ofs, iter.size);
      // TODO: verify hash (?)
    }
  }
  return list;
}

// if destdir is NULL, the files will not be read from the message (ie. the
// uploader must not send the file contents)
std::vector<FileInfo> read_download_message(FileDescriptor &fd, const fs::path *destdir) {
  std::byte buf[4096]; // recv download message
  const auto recv = read_message(fd, buf);
  protocol::download_m msg{};
  if (!deserialize(msg, recv)) throw std::runtime_error("error deserializing download message");
  return read_file_list(fd, msg.file_count, destdir);
}

void download(FileDescriptor &sockfd, std::span<FileInfo> files, const fs::path &destdir) {
  //  read the download message
  auto recvlist = read_download_message(sockfd, nullptr);

  const auto to_delete =
      files | std::views::filter([&recvlist](const FileInfo &f) {
        return std::ranges::none_of(recvlist, [&f](const FileInfo &o) { return o.path == f.path; });
      }) |
      std::ranges::to<std::vector<FileInfo>>();
  // filter the recv list to exclude anything that we already have
  std::erase_if(recvlist, [&files](const FileInfo &f) {
    return std::ranges::any_of(
        files, [&f](const FileInfo &o) { return o.path == f.path && o.hash == f.hash; });
  });

  {                                 // send download response
    assert(recvlist.size() <= 255); // this is a protocol limit
    const protocol::download_response_m resp{recvlist};
    std::byte buf[4096];
    auto end = serialize(buf, resp).value();
    const size_t len = std::distance(std::begin(buf), end.data());
    write_message(sockfd, std::span{buf, len});
  }

  // read download message 2
  read_download_message(sockfd, &destdir);

  // delete files that we don't need anymore
  for (const FileInfo &iter : to_delete) {
    fs::path p = destdir / iter.path;
    std::cout << "deleting " << p << std::endl;
    // remove the file and parent directories
    while (!p.empty()) {
      try {
        remove(p);
      } catch (const std::filesystem::filesystem_error &e) {
        if (e.code() == std::errc::directory_not_empty) break;
        throw;
      }
      p = p.parent_path();
    }
  }
}
