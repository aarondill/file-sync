#include "upload.h"
#include "file_list.h"
#include "protocol.h"
#include "util.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define BUFSIZE 4096
void write_file_list(int fd, const file_list *list, bool include_contents) {
  // send the file info
  uint8_t buf[BUFSIZE];
  while (list) {
    download_file_m f = {
        .name_len = list->name_len,
        // must be zero if not, since there's no body
        .size = include_contents ? list->size : 0,
    };
    memcpy(f.hash, list->hash, MD5_DIGEST_LENGTH);
    memcpy(f.name, list->name, list->name_len);

    serror_t err = 0;
    size_t len = serialize_download_file(buf, sizeof(buf), &f, &err);
    if (err) {
      error("error serializing download file\n");
      return;
    }
    if (!write_message(fd, buf, len)) {
      error("error sending download file\n");
      return;
    }
    if (include_contents) {
      // TODO: send file contents
    }
    list = list->next;
  }
}
void write_download_message(int fd, const file_list *list,
                            bool include_contents) {
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
      printf("error serializing download message\n");
      return;
    }
    if (!write_message(fd, buf, len)) {
      printf("error sending download message\n");
      return;
    }
  }
  write_file_list(fd, list, include_contents);
}

// Sends an upload message to the server
void upload(int sockfd, const file_list *files) {
  // update the global list
  // send download message 1
  write_download_message(sockfd, files, false);

  // receive download response
  uint8_t buf[BUFSIZE];
  ssize_t n = read_message(sockfd, buf, sizeof(buf));
  if (n < 0) {
    error("error reading download response");
    return;
  }

  download_response_m resp;
  {
    serror_t err = 0;
    deserialize_download_response(&resp, buf, n, &err);
    if (err) {
      error("error deserializing download response");
      return;
    }
  }

  file_list *filtered_list = NULL;
  {
    size_t i = 0;
    const file_list *p = files;

    file_list **f_tail = &filtered_list;
    // NOTE: The recipient *must* send the files in the same order as they were
    // sent
    while (i < resp.file_count && p) {
      if (!memcmp(p->hash, resp.files[i].hash, MD5_DIGEST_LENGTH)) {
        // p is in the sent list
        *f_tail = file_list_dup_node(p);
        f_tail = &(*f_tail)->next;
        i++;
      }
      p = p->next;
    }
  }

  // send download message 2
  write_download_message(sockfd, filtered_list, true);
  file_list_free(filtered_list);
}
