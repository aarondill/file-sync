#include "FileInfo.h"
#include <filesystem>
#include <ranges>
namespace fs = std::filesystem;
namespace r = std::ranges;
std::vector<FileInfo> FileInfo::readList(const char *path) {
  return fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied) |
         r::views::filter([](const fs::directory_entry &e) { return !e.is_directory(); }) |
         r::views::transform([](const fs::directory_entry &e) { return e.path(); }) |
         r::to<std::vector<FileInfo>>();
}
