#include "protocol.h"
#include <assert.h>
#include <md5.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// These functions should not clear the error code if it's set and no error
// occurs. The error may also be null. Return the number of bytes consumed

// Assumes variable called err
#define RET_ERR(val)                                                           \
  do {                                                                         \
    if (err)                                                                   \
      *err = (val);                                                            \
    return 0;                                                                  \
  } while (0)
// Assumes variable called err
#define CHECK_LEN(len, min_len)                                                \
  do {                                                                         \
    if ((len) < (min_len))                                                     \
      RET_ERR(NOT_ENOUGH_DATA);                                                \
  } while (0)
// Requires variables: buf, obuf, len
#define CHECK_REMAIN_LEN(len, min_len) CHECK_LEN(len - (buf - obuf), (min_len))

size_t deserialize_uint32(uint32_t *out, const uint8_t *buf, size_t len,
                          serror_t *err) {
  CHECK_LEN(len, sizeof(uint32_t));
  *out = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
  return sizeof(uint32_t);
}
size_t serialize_uint32(uint8_t *buf, size_t len, const uint32_t in,
                        serror_t *err) {
  CHECK_LEN(len, sizeof(uint32_t));
  buf[0] = in & 0xFF;
  buf[1] = (in >> 8) & 0xFF;
  buf[2] = (in >> 16) & 0xFF;
  buf[3] = (in >> 24) & 0xFF;
  return sizeof(uint32_t);
}

size_t serialize_uint16(uint8_t *buf, size_t len, const uint16_t in,
                        serror_t *err) {
  CHECK_LEN(len, sizeof(uint16_t));
  buf[0] = in & 0xFF;
  buf[1] = (in >> 8) & 0xFF;
  return sizeof(uint16_t);
}
size_t deserialize_uint16(uint16_t *out, const uint8_t *buf, size_t len,
                          serror_t *err) {
  CHECK_LEN(len, sizeof(uint16_t));
  *out = buf[0] | (buf[1] << 8);
  return sizeof(uint16_t);
}

/* Note that it's safe to write a uint8 directly since it's a single byte */
size_t deserialize_client_connect(client_connect_m *out, const uint8_t *buf,
                                  size_t len, serror_t *err) {
  const size_t min_len = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t);
  CHECK_LEN(len, min_len);
  if (buf[0] != CLIENT_VERSION) {
    // stop immediately, we can't parse anything more
    RET_ERR(INVALID_PROTOCOL_VERSION);
  }
  out->version = buf[0];
  out->flags = buf[1];
  out->name_len = buf[2];
  CHECK_LEN(len, min_len + out->name_len);
  memcpy(out->name, buf + 3, out->name_len); // note: no null terminator
  return min_len + out->name_len;
}

size_t serialize_client_connect(uint8_t *buf, size_t len,
                                const client_connect_m *in, serror_t *err) {
  if (in->version != CLIENT_VERSION) // This should never happen
    RET_ERR(INVALID_PROTOCOL_VERSION);
  const size_t min_len = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t);
  CHECK_LEN(len, min_len + in->name_len);
  buf[0] = in->version;
  buf[1] = in->flags;
  buf[2] = in->name_len;
  memcpy(buf + 3, in->name, in->name_len);
  return min_len + in->name_len;
}
size_t deserialize_download(download_m *out, const uint8_t *buf, size_t len,
                            serror_t *err) {
  const size_t min_len = sizeof(uint8_t) + sizeof(uint8_t);
  CHECK_LEN(len, min_len);
  out->flags = buf[0];
  out->file_count = buf[1];
  return min_len;
}
size_t serialize_download(uint8_t *buf, size_t len, const download_m *in,
                          serror_t *err) {
  const size_t min_len = sizeof(uint8_t) + sizeof(uint8_t);
  CHECK_LEN(len, min_len);
  buf[0] = in->flags;
  buf[1] = in->file_count;
  return min_len;
}

const size_t HASH_SIZE = sizeof(uint8_t) * MD5_DIGEST_LENGTH;
size_t deserialize_hash(uint8_t *out, const uint8_t *buf, size_t len,
                        serror_t *err) {
  CHECK_LEN(len, HASH_SIZE);
  memcpy(out, buf, HASH_SIZE);
  return HASH_SIZE;
}
size_t serialize_hash(uint8_t *buf, size_t len, const uint8_t *in,
                      serror_t *err) {
  CHECK_LEN(len, HASH_SIZE);
  memcpy(buf, in, HASH_SIZE);
  return HASH_SIZE;
}

size_t deserialize_download_file(download_file_m *out, const uint8_t *buf,
                                 size_t len, serror_t *err) {
  const uint8_t *const obuf = buf; // original buf
  serror_t err2 = 0;
  buf += deserialize_hash(out->hash, buf, len, &err2);
  if (err2)
    RET_ERR(err2);

  CHECK_REMAIN_LEN(len, sizeof(uint8_t));
  out->name_len = buf[0];
  buf += sizeof(uint8_t);

  buf += deserialize_uint32(&out->size, buf, len - (buf - obuf), &err2);
  if (err2)
    RET_ERR(err2);

  CHECK_REMAIN_LEN(len, out->name_len);
  memcpy(out->name, buf, out->name_len);
  buf += out->name_len;
  return buf - obuf;
}
size_t serialize_download_file(uint8_t *buf, size_t len,
                               const download_file_m *in, serror_t *err) {
  const uint8_t *const obuf = buf; // original buf
  serror_t err2 = 0;

  buf += serialize_hash(buf, len, in->hash, &err2);
  if (err2)
    RET_ERR(err2);

  CHECK_REMAIN_LEN(len, sizeof(uint8_t));
  buf[0] = in->name_len;
  buf += sizeof(uint8_t);

  buf += serialize_uint32(buf, len - (buf - obuf), in->size, &err2);
  if (err2)
    RET_ERR(err2);

  CHECK_REMAIN_LEN(len, in->name_len);
  memcpy(buf, in->name, in->name_len);
  buf += in->name_len;
  return buf - obuf;
}

size_t deserialize_download_response(download_response_m *out,
                                     const uint8_t *buf, size_t len,
                                     serror_t *err) {
  const uint8_t *const obuf = buf; // original buf

  CHECK_LEN(len, sizeof(uint8_t));
  out->flags = *buf++;

  // can't parse more since error is a different size
  if (out->flags & ERROR)
    RET_ERR(IS_ERROR);

  CHECK_REMAIN_LEN(len, sizeof(uint8_t));
  out->file_count = *buf++;

  for (size_t i = 0; i < out->file_count; i++) {
    serror_t err2 = 0;
    buf += deserialize_hash(out->files[i].hash, buf, len - (buf - obuf), &err2);
    if (err2)
      RET_ERR(err2);
  }
  return buf - obuf;
}
size_t serialize_download_response(uint8_t *buf, size_t len,
                                   const download_response_m *in,
                                   serror_t *err) {
  const uint8_t *const obuf = buf; // original buf
  if (in->flags & ERROR)
    RET_ERR(IS_ERROR);

  CHECK_LEN(len, sizeof(uint8_t) + sizeof(uint8_t));
  *buf++ = in->flags;
  *buf++ = in->file_count;

  for (size_t i = 0; i < in->file_count; i++) {
    serror_t err2 = 0;
    buf += serialize_hash(buf, len - (buf - obuf), in->files[i].hash, &err2);
    if (err2)
      RET_ERR(err2);
  }
  return buf - obuf;
}

size_t deserialize_error(error_m *out, const uint8_t *buf, size_t len,
                         serror_t *err) {
  const size_t min_len = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t);
  CHECK_LEN(len, min_len);
  out->flags = buf[0];
  out->code = buf[1];
  out->message_len = buf[2];
  buf += min_len;

  CHECK_LEN(len, min_len + out->message_len);
  memcpy(out->message, buf + 3, out->message_len);
  return min_len + out->message_len;
}
size_t serialize_error(uint8_t *buf, size_t len, const error_m *in,
                       serror_t *err) {
  const size_t min_len = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t);
  CHECK_LEN(len, min_len + in->message_len);
  buf[0] = in->flags;
  buf[1] = in->code;
  buf[2] = in->message_len;
  memcpy(buf + 3, in->message, in->message_len);
  return min_len + in->message_len;
}
