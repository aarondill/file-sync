#include "upload.h"
#include "file_list.h"
#include "protocol.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define BUFSIZE 4096

bool write_file_list(int fd, const file_list *list, const char *srcdir) {
  // send the file info
  uint8_t buf[BUFSIZE];
  {
    const file_list *node = list;
    while (node) {
      download_file_m f = {
          .name_len = node->name_len,
          // must be zero if not, since there's no body
          .size = srcdir ? node->size : 0,
      };
      memcpy(f.hash, node->hash, MD5_DIGEST_LENGTH);
      memcpy(f.name, node->name, node->name_len);

      serror_t err = 0;
      size_t len = serialize_download_file(buf, sizeof(buf), &f, &err);
      if (err) {
        error("error serializing download file\n");
        return false;
      }
      if (!write_message(fd, buf, len)) {
        error("error sending download file\n");
        return false;
      }
      node = node->next;
    }
  }

  if (srcdir) { // send file contents
    char path[PATH_MAX] = {0};
    while (list) {
      snprintf(path, sizeof(path), "%s/%s", srcdir, list->name);
      int file_fd = open(path, O_RDONLY);
      if (file_fd < 0) {
        error("error opening file for reading: %s\n", path);
        return false;
      }
      bool succ = transfer_bytes(fd, file_fd, list->size);
      close(file_fd);
      if (!succ)
        fatal("error sending file contents\n");
      list = list->next;
    }
  }
  return true;
}
// only reads the file contents if srcdir is not NULL
bool write_download_message(int fd, const file_list *list, const char *srcdir) {
  size_t file_count = file_list_len(list);
  download_m msg = {
      .flags = 0,
      .file_count = file_count,
  };
  { // send download message
    uint8_t buf[BUFSIZE];
    serror_t err = 0;
    size_t len = serialize_download(buf, sizeof(buf), &msg, &err);
    if (err) {
      error("error serializing download message\n");
      return false;
    }
    if (!write_message(fd, buf, len)) {
      error("error sending download message\n");
      return false;
    }
  }
  return write_file_list(fd, list, srcdir);
}

// Sends an upload message to the server
bool upload(int sockfd, const file_list *files, const char *srcdir) {
  assert(srcdir && *srcdir);
  // update the global list
  // send download message 1
  if (!write_download_message(sockfd, files, NULL)) {
    return false;
  }

  // receive download response
  uint8_t buf[BUFSIZE];
  ssize_t n;
  do {
    n = read_message(sockfd, buf, sizeof(buf));
  } while (n < 0 && errno == EINTR); // EINTR is okay
  if (n < 0) {
    error("error reading download response");
    return false;
  }

  download_response_m resp;
  {
    serror_t err = 0;
    deserialize_download_response(&resp, buf, n, &err);
    if (err) {
      error("error deserializing download response");
      return false;
    }
  }

  file_list *filtered_list = NULL;
  {
    file_list **f_tail = &filtered_list;
    for (size_t i = 0; i < resp.file_count; i++) {
      const file_list *p = file_list_find_hash(files, resp.files[i].hash);
      if (!p) {
        char out[MD5_DIGEST_LENGTH * 2 + 1] = {0};
        for (size_t j = 0; j < MD5_DIGEST_LENGTH; j++)
          sprintf(out + 2 * j, "%02x", resp.files[i].hash[j]);
        fatal("file not found in local list: %s", out);
      }
      // p is in the sent list
      *f_tail = file_list_dup_node(p);
      f_tail = &(*f_tail)->next;
    }
  }

  // send download message 2
  bool succ = write_download_message(sockfd, filtered_list, srcdir);
  file_list_free(filtered_list);
  return succ;
}
