#pragma once
#include <openssl/md5.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// (De)serialization error
enum serror_t {
  NO_ERROR,
  IS_ERROR, // tried to deserialize a message with the error flag set
  INVALID_PROTOCOL_VERSION,
  NOT_ENOUGH_DATA,
};
typedef enum serror_t serror_t;

size_t deserialize_uint32(uint32_t *out, const uint8_t *buf, size_t len,
                          serror_t *err);
size_t serialize_uint32(uint8_t *buf, size_t len, const uint32_t in,
                        serror_t *err);

size_t deserialize_uint16(uint16_t *out, const uint8_t *buf, size_t len,
                          serror_t *err);
size_t serialize_uint16(uint8_t *buf, size_t len, const uint16_t in,
                        serror_t *err);

size_t deserialize_hash(uint8_t *out, const uint8_t *buf, size_t len,
                        serror_t *err);
size_t serialize_hash(uint8_t *buf, size_t len, const uint8_t *in,
                      serror_t *err);

// -- Client Connect Message --
enum client_connect_flags {
  INTENT_TO_UPLOAD = 1 << 0,
};
// Refuse to deserialize if the version is different
static const uint8_t CLIENT_VERSION = 1;

typedef struct client_connect_m client_connect_m;
struct client_connect_m {
  /*
   * - 8 bits for protocol version
   *   - The server must reject any connection with a different version.
   */
  uint8_t version;
  /* 8 bits of flags, see client_connect_flags */
  uint8_t flags;
  /* 8 bits for client name length in bytes */
  uint8_t name_len;
  /* human-readable client name (max of 255 bytes, variable length) */
  char name[255];
};

size_t deserialize_client_connect(client_connect_m *out, const uint8_t *buf,
                                  size_t len, serror_t *err);
size_t serialize_client_connect(uint8_t *buf, size_t len,
                                const client_connect_m *in, serror_t *err);

// -- Download Message --
enum flags {
  /* Error */
  ERROR = 1 << 0,
};

typedef struct download_m download_m;
/** Followed by file_count download_file_m's */
struct download_m {
  /* 8 bits for flags */
  uint8_t flags;
  /* 8 bits for file count */
  uint8_t file_count;
};

typedef struct download_file_m download_file_m;
/** Followed by file_size bytes of file data */
struct download_file_m {
  /* 128 bits for file hash (MD5) */
  uint8_t hash[MD5_DIGEST_LENGTH];
  /* 8 bits for file name length in bytes */
  uint8_t name_len;
  /* 32 bits for file size in bytes */
  uint32_t size;
  /* File name (max of 255 bytes, variable length) */
  char name[255];
};

size_t deserialize_download(download_m *out, const uint8_t *buf, size_t len,
                            serror_t *err);
size_t serialize_download(uint8_t *buf, size_t len, const download_m *in,
                          serror_t *err);

size_t deserialize_download_file(download_file_m *out, const uint8_t *buf,
                                 size_t len, serror_t *err);
size_t serialize_download_file(uint8_t *buf, size_t len,
                               const download_file_m *in, serror_t *err);

// -- Download Response Message --
typedef struct download_response_m download_response_m;
struct download_response_m {
  /* 8 bits for flags */
  uint8_t flags;
  /* 8 bits for file count */
  uint8_t file_count;
  struct {
    /* 128 bits for file hash (MD5) */
    uint8_t hash[MD5_DIGEST_LENGTH];
  } files[255];
};

size_t deserialize_download_response(download_response_m *out,
                                     const uint8_t *buf, size_t len,
                                     serror_t *err);
size_t serialize_download_response(uint8_t *buf, size_t len,
                                   const download_response_m *in,
                                   serror_t *err);

// -- Error Message --
/* Values for error_m.code */
enum error_code {
  TOO_MANY_CLIENTS = 1,
};
typedef struct error_m error_m;
struct error_m {
  /* 8 bits for flags */
  uint8_t flags;
  /* 8 bits for error code */
  uint8_t code;
  /* 8 bits for error message length in bytes */
  uint8_t message_len;
  /* variable length error message */
  char message[255];
};

size_t deserialize_error(error_m *out, const uint8_t *buf, size_t len,
                         serror_t *err);
size_t serialize_error(uint8_t *buf, size_t len, const error_m *in,
                       serror_t *err);
