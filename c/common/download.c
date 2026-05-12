#include "file_list.h"
#include "protocol.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define BUFSIZE 4096

// makes the directory to contain the file
void mkdir_p(const char *file) {
  char *path = strdup(file);
  char *q = path; // find each '/'
  while ((q = strchr(q, '/'))) {
    *q = '\0';
    if (mkdir(path, 0755) < 0 && errno != EEXIST)
      fatal("error creating directory: %s\n", path);
    *q++ = '/';
  }
  free(path);
}

// deletes a directory and all empty parent directories (ie. rmdir -p)
void rmdir_p(const char *path) {
  char *p = strdup(path);
  while (p) {
    if (rmdir(p) < 0) {
      if (errno == ENOTEMPTY || errno == EEXIST)
        break; // we're done. No parents will be empty
      if (errno != ENOENT) {
        warn("error deleting directory: %s\n", p);
        break;
      }
    }
    char *q = strrchr(p, '/');
    if (!q)
      break;
    *q = '\0'; // remove last '/'
  }
  free(p);
}

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
// if include_contents is true, the file contents will be read and written to
// disk
bool read_file_list(int fd, size_t file_count, file_list **list,
                    const char *destdir) {
  assert(*list == NULL);

  { // recv the file info
    uint8_t buf[BUFSIZE];
    file_list **tail = list;
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
  }

  assert(file_count == file_list_len(*list));
  if (destdir) { // recv/write the file contents
    for (file_list *iter = *list; iter; iter = iter->next) {
      char path[PATH_MAX] = {0};
      snprintf(path, sizeof(path), "%s/%s", destdir, iter->name);
      mkdir_p(path);
      int file_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (file_fd < 0)
        fatal("error opening file for writing: %s\n", path);
      printf("receiving %s: ", path);
      printhex(iter->hash, MD5_DIGEST_LENGTH);
      printf("\n");
      bool succ = transfer_bytes(file_fd, fd, iter->size);
      // TODO: verify hash (?)
      close(file_fd);
      if (!succ)
        fatal("error receiving file contents\n");
    }
  }
  return true;

cleanup:
  file_list_free(*list);
  *list = NULL;
  return false;
}

// if destdir is NULL, the files will not be read from the message (ie. the
// uploader must not send the file contents)
bool read_download_message(int fd, download_m *msg, file_list **recvlist,
                           const char *destdir) {
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
  return read_file_list(fd, msg->file_count, recvlist, destdir);
}

bool download(int sockfd, const file_list *files, const char *destdir) {
  //  read the download message
  download_m msg = {0};
  file_list *recvlist = NULL;
  if (!read_download_message(sockfd, &msg, &recvlist, NULL)) {
    // abort if interrupted (to allow an upload to be started)
    if (errno != EINTR)
      perror("read_download_message");
    return false;
  }

  file_list *to_delete = file_list_dup(files);
  { // keep track of files to delete (anything local that isn't in the recvlist)
    file_list **p = &to_delete;
    while (*p) {
      if (file_list_find_path(recvlist, (*p)->name)) {
        file_list *tmp = *p;
        *p = (*p)->next; // move next back
        free(tmp);
      } else {
        p = &(*p)->next; // move p forward
      }
    }
  }

  { // filter the recv list to exclude anything that we already have
    file_list **p = &recvlist;
    const file_list *iter = files;
    while (*p) {
      // guarentee the order of the files is the same as the original
      if ((iter = file_list_find(iter, *p))) {
        // remove p from recvlist
        file_list *tmp = *p;
        *p = (*p)->next; // move next back
        free(tmp);
      } else {
        p = &(*p)->next; // move p forward
      }
    }
  }

  // send download response
  if (!respond_download(sockfd, recvlist)) {
    error("error sending download response");
    goto cleanup;
  }
  file_list_free(recvlist);
  recvlist = NULL;

  // read download message 2
  while (!read_download_message(sockfd, &msg, &recvlist, destdir)) {
    if (errno != EINTR) { // EINTR is okay
      perror("read_download_message");
      goto cleanup;
    }
  }
  file_list_free(recvlist);

  { // delete files that we don't need anymore
    char path[PATH_MAX] = {0};
    for (file_list *iter = to_delete; iter; iter = iter->next) {
      snprintf(path, sizeof(path), "%s/%s", destdir, iter->name);
      printf("deleting %s\n", path);
      unlink(path); // remove the file
      {             // remove the parent director(ies) if empty
        char *slash = strrchr(path, '/');
        *slash = '\0'; // remove the file name
        rmdir_p(path);
      }
    }
  }
  file_list_free(to_delete);
  return true;

cleanup:
  file_list_free(recvlist);
  file_list_free(to_delete);
  return false;
}
