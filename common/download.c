// This function takes ownership of the recvlist !
#include "file_list.h"
#include "protocol.h"
#include "util.h"
#include <stdlib.h>
#define BUFSIZE 4096

void respond_download(int sockfd, file_list *recvlist, const file_list *local) {
  { // filter the recv list to exclude anything that we already have
    file_list **p = &recvlist;
    while (*p) {
      if (file_list_contains(local, *p)) {
        // remove p from recvlist
        file_list *tmp = *p;
        *p = (*p)->next;
        free(tmp);
      }
      p = &(*p)->next;
    }
  }

  { // send download response
    uint8_t buf[BUFSIZE];
    download_response_m resp = {
        .flags = 0,
        .file_count = file_list_len(recvlist),
    };
    serror_t err = 0;
    size_t len = serialize_download_response(buf, sizeof(buf), &resp, &err);
    if (err) {
      error("error serializing download response\n");
      return;
    }
    if (!write_message(sockfd, buf, len)) {
      error("error sending download response\n");
      return;
    }
  }

  { // send the filtered list (hashes only)
    uint8_t buf[BUFSIZE];
    file_list *list = recvlist;
    while (list) {
      serror_t err = 0;
      size_t len = serialize_hash(buf, sizeof(buf), list->hash, &err);
      if (err) {
        error("error serializing download file\n");
        return;
      }
      if (!write_message(sockfd, buf, len)) {
        error("error sending download file\n");
        return;
      }
      list = list->next;
    }
  }

  file_list_free(recvlist);
}

void download(int sockfd, const file_list *files) {
  // TODO: implement
}
