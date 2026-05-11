#pragma once
#include "FileDescriptor.h"
#include "FileInfo.h"
#include <filesystem>
namespace fs = std::filesystem;
void download(FileDescriptor &sockfd, std::span<FileInfo> files, const fs::path &destdir);
