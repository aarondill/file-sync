#include "file_list.h"
#include <md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void file_list_update(file_list **list, const char *path) {
  // TODO: implement
  fprintf(stderr, "UNIMPLEMENTED: file_list_update(%p, %s)\n", (void *)list,
          path);
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
