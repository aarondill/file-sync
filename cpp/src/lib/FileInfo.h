#pragma once
#include "FileHash.h"
#include <vector>

struct FileInfo {
  explicit FileInfo(const std::string_view name, const FileHash hash, const size_t size)
      : path{name}, hash{hash}, size{size} {}
  explicit FileInfo(std::filesystem::path path)
      : path(std::move(path)), hash{this->path}, size{file_size(this->path)} {}
  /** A relative path, which may only be <=255 chars long! */
  std::filesystem::path path;
  FileHash hash;
  size_t size;
  // Reads the directory and returns a list of all files in the directory
  static std::vector<FileInfo> readList(const char *path);
};
