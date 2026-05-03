// This function takes ownership of the recvlist !
#include "file_list.h"
#include "protocol.h"
#include "util.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define BUFSIZE 4096

void respond_download(int sockfd, const file_list *recvlist) {
  // construct the response
  size_t file_count = file_list_len(recvlist);
  assert(file_count <= 255); // this is a protocol limit
  download_response_m resp = {.flags = 0, .file_count = file_count};

  // NOTE: should we send the hashes seperately? (avoids a copy)
  { // copy hashes into response
    for (size_t i = 0; i < resp.file_count; i++) {
      memcpy(resp.files[i].hash, recvlist->hash, MD5_DIGEST_LENGTH);
      recvlist = recvlist->next;
    }
  }

  { // send download response
    uint8_t buf[BUFSIZE];
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
}

// NOTE: does *not* read the file contents
file_list *read_file_list(int fd, size_t file_count) {
  file_list *list = NULL, **tail = &list;
  // recv the file info
  uint8_t buf[BUFSIZE];
  for (size_t i = 0; i < file_count; i++) {
    download_file_m f = {0};
    { // read file info
      ssize_t n = read_message(fd, buf, sizeof(buf));
      if (n < 0) {
        error("error reading download file");
        return NULL;
      }
      serror_t err = 0;
      deserialize_download_file(&f, buf, n, &err);
      if (err) {
        error("error deserializing download file");
        return NULL;
      }
    }
    file_list *new = file_list_new(f.name, f.name_len, f.size, f.hash);
    *tail = new;
    tail = &new->next;
  }
  assert(file_count == file_list_len(list));
  return list;
}

file_list *read_download_message(int fd, download_m *msg) {
  { // recv download message
    uint8_t buf[BUFSIZE];
    ssize_t n = read_message(fd, buf, sizeof(buf));
    if (n < 0) {
      error("error reading download message");
      return NULL;
    }
    serror_t err = 0;
    deserialize_download(msg, buf, n, &err);
    if (err) {
      error("error deserializing download message");
      return NULL;
    }
  }
  file_list *recvlist = read_file_list(fd, msg->file_count);
  return recvlist;
}

void download(int sockfd, const file_list *files) {
  //  read the download message
  download_m msg = {0};
  file_list *recvlist = read_download_message(sockfd, &msg);

  { // filter the recv list to exclude anything that we already have
    file_list **p = &recvlist;
    const file_list *iter = files;
    while (*p) {
      // guarentee the order of the files is the same as the original
      if ((iter = file_list_find(iter, *p))) {
        file_list *tmp = *p; // remove p from recvlist
        *p = (*p)->next;
        free(tmp);
      }
      p = &(*p)->next;
    }
  }

  // send download response
  respond_download(sockfd, recvlist);
  file_list_free(recvlist);

  // read download message 2
  recvlist = read_download_message(sockfd, &msg);
  file_list *iter = recvlist;
  while (iter) {
    // TODO: read the file contents
    printf("recv: ");
    for (size_t i = 0; i < iter->name_len; i++)
      printf("%c", iter->name[i]);
    printf(" ");
    for (size_t i = 0; i < MD5_DIGEST_LENGTH; i++)
      printf("%02x", iter->hash[i]);
    printf("\n");
    iter = iter->next;
  }
  file_list_free(recvlist);
}
