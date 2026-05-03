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

inline void debug(const char *msg) { fprintf(stderr, "debug: %s\n", msg); }
inline void warn(const char *msg) { fprintf(stderr, "warning: %s\n", msg); }
inline void error(const char *msg) { fprintf(stderr, "error: %s\n", msg); }
inline void fatal(const char *msg) {
  fprintf(stderr, "fatal: %s\n", msg);
  exit(1);
}
