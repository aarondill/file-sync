#pragma once
#include "file_list.h"
bool download(int sockfd, const file_list *files, const char *destdir);
