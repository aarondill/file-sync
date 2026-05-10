#pragma once
#include "FileDescriptor.h"
#include "FileInfo.h"
#include <filesystem>
namespace fs = std::filesystem;
bool upload(FileDescriptor sockfd, const std::vector<FileInfo> &files, fs::path srcdir);
