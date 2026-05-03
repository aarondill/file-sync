#include "file_list.h"
#include <md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

file_list *file_list_update(file_list *list, const char *path) {
  // TODO: implement
  fprintf(stderr, "UNIMPLEMENTED: file_list_update(%s)\n", path);
  return list;
}

bool file_list_contains(file_list *list, file_list *node) {
  while (list) {
    if (!memcmp(list->hash, node->hash, MD5_DIGEST_LENGTH) &&
        !strcmp(list->name, node->name)) {
      return true;
    }
    list = list->next;
  }
  return false;
}

size_t file_list_len(file_list *list) {
  size_t len = 0;
  while (list) {
    len++;
    list = list->next;
  }
  return len;
}
file_list *file_list_dup_node(file_list *node) {
  file_list *dup = malloc(sizeof(file_list));
  *dup = *node;
  dup->next = NULL;
  return dup;
}

file_list *file_list_dup(file_list *list) {
  file_list *dup = NULL;
  while (list) {
    dup = file_list_dup_node(list);
    dup->next = dup;
    list = list->next;
  }
  return dup;
}

file_list *file_list_free(file_list *list) {
  file_list *next = list;
  while (next) {
    file_list *tmp = next;
    next = next->next;
    free(tmp);
  }
  return NULL;
}
