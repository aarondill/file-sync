#pragma once
#include "FileDescriptor.h"
#include "FileInfo.h"
#include <filesystem>
namespace fs = std::filesystem;
bool download(FileDescriptor sockfd, const std::vector<FileInfo> &files, fs::path destdir);
