#include "protocol.h"
#include <assert.h>
#include <md5.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Note that it's safe to write a uint8 directly since it's a single byte */
serror_t deserialize_client_connect(client_connect_m *out, const uint8_t *buf,
                                    size_t len) {
  const size_t min_len = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t);
  if (len < min_len)
    return NOT_ENOUGH_DATA;
  if (buf[0] != CLIENT_VERSION) {
    // stop immediately, we can't parse anything more
    return INVALID_PROTOCOL_VERSION;
  }
  out->version = buf[0];
  out->flags = buf[1];
  out->name_len = buf[2];
  if (len < min_len + out->name_len) {
    // stop immediately, we can't parse anything more
    return NOT_ENOUGH_DATA;
  }
  memcpy(out->name, buf + 3, out->name_len); // note: no null terminator
  return NO_ERROR;
}

serror_t serialize_client_connect(uint8_t *buf, size_t len,
                                  const client_connect_m *in) {
  const size_t min_len = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t);
  if (in->version != CLIENT_VERSION) // This should never happen
    return INVALID_PROTOCOL_VERSION;
  if (len < min_len + in->name_len)
    return NOT_ENOUGH_DATA;
  buf[0] = in->version;
  buf[1] = in->flags;
  buf[2] = in->name_len;
  assert(len >= min_len + in->name_len);
  memcpy(buf + 3, in->name, in->name_len);
  return NO_ERROR;
}
serror_t deserialize_download(download_m *out, const uint8_t *buf, size_t len) {
  const size_t min_len = sizeof(uint8_t) + sizeof(uint8_t);
  if (len < min_len)
    return NOT_ENOUGH_DATA;
  out->flags = buf[0];
  out->file_count = buf[1];
  return NO_ERROR;
}
serror_t serialize_download(uint8_t *buf, size_t len, const download_m *in) {
  const size_t min_len = sizeof(uint8_t) + sizeof(uint8_t);
  if (len < min_len)
    return NOT_ENOUGH_DATA;
  buf[0] = in->flags;
  buf[1] = in->file_count;
  return NO_ERROR;
}

const size_t HASH_SIZE = sizeof(uint8_t) * MD5_DIGEST_LENGTH;
serror_t deserialize_hash(uint8_t *out, const uint8_t *buf, size_t len) {
  if (len < HASH_SIZE)
    return NOT_ENOUGH_DATA;
  memcpy(out, buf, HASH_SIZE);
  return NO_ERROR;
}
serror_t serialize_hash(uint8_t *buf, size_t len, const uint8_t *in) {
  if (len < HASH_SIZE)
    return NOT_ENOUGH_DATA;
  memcpy(buf, in, HASH_SIZE);
  return NO_ERROR;
}

serror_t deserialize_download_file(download_file_m *out, const uint8_t *buf,
                                   size_t len) {
  const size_t min_len = HASH_SIZE + sizeof(uint8_t) + sizeof(uint32_t);
  if (len < min_len)
    return NOT_ENOUGH_DATA;

  serror_t err;
  if ((err = deserialize_hash(out->hash, buf, len)))
    return err;
  buf += HASH_SIZE; // skip hash

  out->name_len = buf[0];
  out->size = buf[1];
  if (len < min_len + out->name_len)
    return NOT_ENOUGH_DATA;
  memcpy(out->name, buf + 2, out->name_len);
  return NO_ERROR;
}
serror_t serialize_download_file(uint8_t *buf, size_t len,
                                 const download_file_m *in) {
  const size_t min_len = HASH_SIZE + sizeof(uint8_t) + sizeof(uint32_t);
  if (len < min_len + in->name_len)
    return NOT_ENOUGH_DATA;

  serror_t err;
  if ((err = serialize_hash(buf, len, in->hash)))
    return err;
  buf += HASH_SIZE;

  buf[0] = in->name_len;
  buf[1] = in->size;
  memcpy(buf + 2, in->name, in->name_len);
  return NO_ERROR;
}
serror_t deserialize_download_response(download_response_m *out,
                                       const uint8_t *buf, size_t len) {
  if (len < sizeof(uint8_t))
    return NOT_ENOUGH_DATA;
  out->flags = buf[0];
  // can't parse more since error is a different size
  if (out->flags & ERROR)
    return IS_ERROR;

  const size_t min_len = sizeof(uint8_t) + sizeof(uint8_t);
  if (len < min_len)
    return NOT_ENOUGH_DATA;

  out->file_count = buf[1];

  buf += min_len;
  len -= min_len;
  for (size_t i = 0; i < out->file_count; i++) {
    serror_t err;
    if ((err = deserialize_hash(out->files[i].hash, buf, len)))
      return err;
    buf += HASH_SIZE;
    len -= HASH_SIZE;
  }

  return NO_ERROR;
}
serror_t serialize_download_response(uint8_t *buf, size_t len,
                                     const download_response_m *in) {
  const size_t min_len = sizeof(uint8_t) + sizeof(uint8_t);
  if (len < min_len)
    return NOT_ENOUGH_DATA;
  if (in->flags & ERROR)
    return IS_ERROR;
  buf[0] = in->flags;
  buf[1] = in->file_count;
  buf += min_len;
  len -= min_len;
  for (size_t i = 0; i < in->file_count; i++) {
    serror_t err;
    if ((err = serialize_hash(buf, len, in->files[i].hash)))
      return err;
    buf += HASH_SIZE;
    len -= HASH_SIZE;
  }
  return NO_ERROR;
}

serror_t deserialize_error(error_m *out, const uint8_t *buf, size_t len) {
  const size_t min_len = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t);
  if (len < min_len)
    return NOT_ENOUGH_DATA;

  out->flags = buf[0];
  out->code = buf[1];
  out->message_len = buf[2];
  if (len < min_len + out->message_len)
    return NOT_ENOUGH_DATA;
  memcpy(out->message, buf + 3, out->message_len);
  return NO_ERROR;
}
serror_t serialize_error(uint8_t *buf, size_t len, const error_m *in) {
  const size_t min_len = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t);
  if (len < min_len + in->message_len)
    return NOT_ENOUGH_DATA;

  buf[0] = in->flags;
  buf[1] = in->code;
  buf[2] = in->message_len;
  memcpy(buf + 3, in->message, in->message_len);
  return NO_ERROR;
}
