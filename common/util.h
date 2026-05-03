#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
bool write_all(int fd, const uint8_t *buf, size_t len);
ssize_t read_all(int fd, uint8_t *buf, size_t min, size_t max);

// Writes the length of the message as a uint16_t, followed by the message
bool write_message(int fd, const uint8_t *buf, size_t len);
// Reads the length of the message as a uint16_t, followed by the message
ssize_t read_message(int fd, uint8_t *buf, size_t max);

#define _print(fmt, ...)                                                       \
  do {                                                                         \
    fputs(fmt, stderr);                                                        \
    fprintf(stderr, __VA_ARGS__);                                              \
    fputs("\n", stderr);                                                       \
  } while (0)

#define debug(...) _print("debug: ", ##__VA_ARGS__)
#define warn(...) _print("warning: ", ##__VA_ARGS__)
#define error(...) _print("error: ", ##__VA_ARGS__)
#define fatal(...)                                                             \
  do {                                                                         \
    _print("fatal: ", ##__VA_ARGS__);                                          \
    exit(1);                                                                   \
  } while (0)

void printhex(const uint8_t *buf, size_t len);
void printlen(const char *x, size_t len);
