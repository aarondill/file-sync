#include "util.h"
#include "protocol.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#define BUFSIZE 4096

bool write_all(int fd, const uint8_t *buf, size_t len) {
  size_t written = 0;
  while (written < len) {
    ssize_t ret = write(fd, buf + written, len - written);
    if (ret < 0) {
      if (errno == EINTR)
        continue;
      perror("write");
      return false;
    }
    written += ret;
  }
  return true;
}

// min is the minimum number of bytes read, max is the maximum number of bytes
// (buffer size) read
ssize_t read_all(int fd, uint8_t *buf, size_t min, size_t max) {
  size_t got = 0;
  while (got < min) {
    size_t to_read = max - got;
    if (to_read > min)
      to_read = min;
    ssize_t ret = read(fd, buf + got, to_read);
    if (ret <= 0) {
      // if we read something, keep going
      if (errno == EINTR && got > 0)
        continue;
      return -1;
    }
    got += ret;
  }
  assert(got == min);
  return got;
}

bool write_message(int fd, const uint8_t *buf, size_t len) {
  uint8_t lenbuf[sizeof(uint16_t)];
  serror_t err = 0;
  size_t lenbuflen = serialize_uint16(lenbuf, sizeof(lenbuf), len, &err);
  if (err) {
    fprintf(stderr, "error serializing message\n");
    return false;
  }
  return write_all(fd, lenbuf, lenbuflen) && // write length
         write_all(fd, buf, len);            // write message
}

ssize_t read_message(int fd, uint8_t *buf, size_t max) {
  // read length
  uint8_t lenbuf[sizeof(uint16_t)];
  ssize_t n = read_all(fd, lenbuf, sizeof(uint16_t), sizeof(lenbuf));
  if (n < 0)
    return -1;
  serror_t err = 0;
  uint16_t len = 0;
  deserialize_uint16(&len, lenbuf, n, &err);
  if (err) {
    fprintf(stderr, "error deserializing message\n");
    return -1;
  }

  ssize_t ret = read_all(fd, buf, len, max);
  assert(ret == len);
  return ret;
}

void printhex(const uint8_t *buf, size_t len) {
  for (size_t i = 0; i < len; i++)
    printf("%02x", buf[i]);
}
void printlen(const char *x, size_t len) {
  for (size_t i = 0; i < len; i++)
    printf("%c", x[i]);
}

// sends size bytes from in to out
// aborts if there is not size bytes available
bool transfer_bytes(int out, int in, size_t size) {
  uint8_t buf[BUFSIZE];
  size_t rem = size;
  while (rem > 0) {
    size_t to_read = rem > sizeof(buf) ? sizeof(buf) : rem;
    ssize_t n = read(in, buf, to_read);
    if (n == 0)
      fatal("not enough data\n");
    else if (n < 0) {
      // if interrupted, keep going
      if (errno != EINTR)
        return false;
      else
        continue;
    }
    if (!write_all(out, buf, n)) {
      error("error sending bytes\n");
      return false;
    }
    rem -= n;
  }
  return true;
}
