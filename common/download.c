// This function takes ownership of the recvlist !
#include "file_list.h"
#include "protocol.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#define BUFSIZE 4096

bool respond_download(int sockfd, const file_list *recvlist) {
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
      return false;
    }
    if (!write_message(sockfd, buf, len)) {
      error("error sending download response\n");
      return false;
    }
  }
  return true;
}

// NOTE: does *not* read the file contents
// *list must be NULL
bool read_file_list(int fd, size_t file_count, file_list **list) {
  assert(*list == NULL);
  file_list **tail = list;
  // recv the file info
  uint8_t buf[BUFSIZE];
  for (size_t i = 0; i < file_count; i++) {
    download_file_m f = {0};
    { // read file info
      ssize_t n = read_message(fd, buf, sizeof(buf));
      if (n < 0) {
        error("error reading download file");
        goto cleanup;
      }
      serror_t err = 0;
      deserialize_download_file(&f, buf, n, &err);
      if (err) {
        error("error deserializing download file");
        goto cleanup;
      }
    }
    file_list *new = file_list_new(f.name, f.name_len, f.size, f.hash);
    *tail = new;
    tail = &new->next;
  }
  assert(file_count == file_list_len(*list));
  return true;

cleanup:
  file_list_free(*list);
  *list = NULL;
  return false;
}

bool read_download_message(int fd, download_m *msg, file_list **recvlist) {
  { // recv download message
    uint8_t buf[BUFSIZE];
    ssize_t n = read_message(fd, buf, sizeof(buf));
    if (n < 0) {
      return NULL;
    }
    serror_t err = 0;
    deserialize_download(msg, buf, n, &err);
    if (err) {
      error("error deserializing download message");
      return NULL;
    }
  }
  return read_file_list(fd, msg->file_count, recvlist);
}

bool download(int sockfd, const file_list *files) {
  //  read the download message
  download_m msg = {0};
  file_list *recvlist = NULL;
  if (!read_download_message(sockfd, &msg, &recvlist)) {
    // abort if interrupted (to allow an upload to be started)
    if (errno != EINTR)
      perror("read_download_message");
    return false;
  }

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
  if (!respond_download(sockfd, recvlist)) {
    error("error sending download response");
    return false;
  }
  file_list_free(recvlist);
  recvlist = NULL;

  // read download message 2
  while (!read_download_message(sockfd, &msg, &recvlist)) {
    if (errno != EINTR) { // EINTR is okay
      perror("read_download_message");
      return false;
    }
  }
  file_list *iter = recvlist;
  while (iter) {
    // TODO: read the file contents
    printf("recv: ");
    printlen(iter->name, iter->name_len);
    printf(" ");
    printhex(iter->hash, MD5_DIGEST_LENGTH);
    printf("\n");
    iter = iter->next;
  }
  file_list_free(recvlist);
  return true;
}
