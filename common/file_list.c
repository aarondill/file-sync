#include "file_list.h"
#include "util.h"
#include <assert.h>
#include <dirent.h>
#include <md5.h>
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
  MD5Init(&ctx);

  uint8_t data[1024];
  size_t bytes;
  while ((bytes = fread(data, 1, sizeof(data), fp)) > 0) {
    MD5Update(&ctx, data, bytes);
  }
  fclose(fp);

  MD5Final(hash, &ctx);
  return hash;
}

// fill the list with the recursive contents of the directory at path
file_list *file_list_read(const char *path) {
  DIR *dir = opendir(path);
  if (!dir)
    return NULL;
  file_list *list = NULL;
  struct dirent *ent;
  struct stat st;
  uint8_t hash[MD5_DIGEST_LENGTH];
  while ((ent = readdir(dir))) {
    if (stat(ent->d_name, &st))
      continue;

    if (S_ISDIR(st.st_mode)) {
      file_list_append(&list, file_list_read(ent->d_name));
    } else if (S_ISREG(st.st_mode)) {
      file_list_append(&list,
                       file_list_new(ent->d_name, strlen(ent->d_name),
                                     st.st_size, hash_file(ent->d_name, hash)));
    } else {
      warn("unknown file type: %s", ent->d_name);
    }
  }
  closedir(dir);
  return list;
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
  while (list) {
    if (!memcmp(list->hash, node->hash, MD5_DIGEST_LENGTH) &&
        !strcmp(list->name, node->name)) {
      return list;
    }
    list = list->next;
  }
  return NULL;
}
const file_list *file_list_find_hash(const file_list *list,
                                     const uint8_t *hash) {
  while (list) {
    if (!memcmp(list->hash, hash, MD5_DIGEST_LENGTH))
      return list;
    list = list->next;
  }
  return NULL;
}

size_t file_list_len(const file_list *list) {
  size_t len = 0;
  while (list) {
    len++;
    list = list->next;
  }
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
  while (list) {
    dup = file_list_dup_node(list);
    dup->next = dup;
    list = list->next;
  }
  return dup;
}

file_list *file_list_new(const char *name, size_t name_len, size_t size,
                         const uint8_t *hash) {
  file_list *new = malloc(sizeof(file_list));
  if (!new)
    return NULL;
  assert(name_len < sizeof(new->name));
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
  file_list *next = list;
  while (next) {
    file_list *tmp = next;
    next = next->next;
    free(tmp);
  }
}
