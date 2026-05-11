#pragma once
#include "FileDescriptor.h"
#include "FileInfo.h"
#include <filesystem>
namespace fs = std::filesystem;
void upload(FileDescriptor &sockfd, const std::span<const FileInfo> files, const fs::path &srcdir);
