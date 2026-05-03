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

#define debug(fmt, ...) fprintf(stderr, "debug: " fmt "\n", ##__VA_ARGS__)
#define warn(fmt, ...) fprintf(stderr, "warning: " fmt "\n", ##__VA_ARGS__)
#define error(fmt, ...) fprintf(stderr, "error: " fmt "\n", ##__VA_ARGS__)
#define fatal(fmt, ...)                                                        \
  do {                                                                         \
    fprintf(stderr, "fatal: " fmt "\n", ##__VA_ARGS__);                        \
    exit(1);                                                                   \
  } while (0)
