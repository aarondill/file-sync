#include "file_list.h"
#include "util.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <openssl/md5.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

uint8_t *hash_file(const char *filename, uint8_t *hash) {
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL)
    return NULL;

  MD5_CTX ctx;
  MD5_Init(&ctx);

  uint8_t data[1024];
  size_t bytes;
  while ((bytes = fread(data, 1, sizeof(data), fp)) > 0) {
    MD5_Update(&ctx, data, bytes);
  }
  fclose(fp);

  MD5_Final(hash, &ctx);
  return hash;
}

// returns a string containing the next file in the directory. Skips . and ..
// Do not free the returned string. It will change as you call this function.
// Returns NULL if there are no more files in the directory or if an error
// occurs.
char *read_dir(DIR *dir, const char *dirpath) {
  static char buf[PATH_MAX] = {0};
  if (strlen(dirpath) > sizeof(buf) - 2) { // 2 for the slash and null
    errno = ENAMETOOLONG;
    return NULL;
  }
  strcpy(buf, dirpath);
  strcat(buf, "/");

  char *d_name;
  do {
    struct dirent *ent = readdir(dir);
    if (!ent)
      return NULL;
    d_name = ent->d_name;
  } while (!strcmp(d_name, ".") || !strcmp(d_name, "..")); // skip . and ..
  if (strlen(d_name) > sizeof(buf) - 1) {
    errno = ENAMETOOLONG;
    return NULL;
  }
  strcat(buf, d_name);
  return buf;
}

// buf should be used to create the relative path to the file
file_list *file_list_read_impl(const char *path, char *buf, size_t buflen) {
  DIR *dir = opendir(path);
  if (!dir)
    return NULL;
  struct stat st;
  uint8_t hash[MD5_DIGEST_LENGTH];
  file_list *list = NULL;

  // after function call, memset everything after bufend to 0
  char *bufend = buf + strlen(buf);
  size_t buf_rem = buflen - (bufend - buf);

  char *p;
  while ((p = read_dir(dir, path))) {
    if (lstat(p, &st))
      continue;

    // add last component of path to buf
    char *last_slash = strrchr(p, '/');
    assert(last_slash);
    if (strlen(last_slash) + 1 > buf_rem) {
      warn("path too long: %s/%s", path, last_slash);
      continue;
    }
    strcat(bufend, last_slash);

    if (S_ISDIR(st.st_mode)) {
      file_list_append(&list, file_list_read_impl(p, buf, buflen));
    } else if (S_ISREG(st.st_mode)) {
      file_list_append(&list, file_list_new(buf, strlen(buf), st.st_size,
                                            hash_file(p, hash)));
    } else {
      warn("unknown file type: %s", p);
    }
    memset(bufend, 0, buf_rem); // remove the path
  }
  closedir(dir);

  return list;
}
// fill the list with the recursive contents of the directory at path
file_list *file_list_read(const char *path) {
  char buf[256] = {0}; // 256 because 255 + 1 for the null
  return file_list_read_impl(path, buf, sizeof(buf));
}

void file_list_append(file_list **list, file_list *to_insert) {
  if (!to_insert) {
    return;
  }
  while (*list)
    list = &(*list)->next;
  *list = to_insert;
}

// given ad and bc, insert(a, b) returns abcd
void file_list_insert(file_list **node, file_list *to_insert) {
  if (!to_insert) {
    return;
  } else if (!*node) {
    *node = to_insert;
  } else {
    file_list *tmp = (*node)->next;
    (*node)->next = to_insert;
    while (to_insert->next)
      to_insert = to_insert->next;
    to_insert->next = tmp;
  }
}

const file_list *file_list_find(const file_list *list, file_list *node) {
  for (; list; list = list->next)
    if (!memcmp(list->hash, node->hash, MD5_DIGEST_LENGTH) &&
        !strncmp(list->name, node->name, node->name_len))
      return list;
  return NULL;
}
const file_list *file_list_find_path(const file_list *list, const char *path) {
  for (; list; list = list->next)
    if (!strncmp(list->name, path, list->name_len))
      return list;
  return NULL;
}
const file_list *file_list_find_hash(const file_list *list,
                                     const uint8_t *hash) {
  for (; list; list = list->next)
    if (!memcmp(list->hash, hash, MD5_DIGEST_LENGTH))
      return list;
  return NULL;
}

size_t file_list_len(const file_list *list) {
  size_t len = 0;
  for (; list; list = list->next)
    len++;
  return len;
}
file_list *file_list_dup_node(const file_list *node) {
  file_list *dup = malloc(sizeof(file_list));
  *dup = *node;
  dup->next = NULL;
  return dup;
}

file_list *file_list_dup(const file_list *list) {
  file_list *dup = NULL;
  file_list **tail = &dup;
  for (; list; list = list->next) {
    *tail = file_list_dup_node(list);
    tail = &(*tail)->next;
  }
  return dup;
}

file_list *file_list_new(const char *name, size_t name_len, size_t size,
                         const uint8_t *hash) {
  file_list *new = malloc(sizeof(file_list));
  if (!new)
    return NULL;
  if (name_len > sizeof(new->name) - 1)
    fatal("name too long: %s", name);
  *new = (file_list){
      .name_len = name_len,
      .name = {0},
      .hash = {0},
      .size = size,
      .next = NULL,
  };
  memcpy(new->name, name, name_len);
  memcpy(new->hash, hash, MD5_DIGEST_LENGTH);
  return new;
}

void file_list_free(file_list *list) {
  while (list) {
    file_list *tmp = list;
    list = list->next;
    free(tmp);
  }
}
