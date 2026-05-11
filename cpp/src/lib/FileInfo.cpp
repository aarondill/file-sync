#include "FileInfo.h"
#include <filesystem>
#include <ranges>
namespace fs = std::filesystem;
std::vector<FileInfo> FileInfo::readList(const char *path) {
  return fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied) |
         std::views::filter([](const fs::directory_entry &e) { return !e.is_directory(); }) |
         std::views::transform([](const fs::directory_entry &e) { return e.path(); }) |
         std::ranges::to<std::vector<FileInfo>>();
}
