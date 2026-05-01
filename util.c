#include "util.h"
#include <stdio.h>
#include <unistd.h>

bool write_all(int fd, const uint8_t *buf, size_t len) {
  size_t written = 0;
  while (written < len) {
    ssize_t ret = write(fd, buf + written, len - written);
    if (ret < 0) {
      perror("write");
      return false;
    }
    written += ret;
  }
  return true;
}
