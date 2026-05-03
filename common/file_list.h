#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct file_list {
  size_t name_len;
  char name[255];
  uint8_t hash[16];
  size_t size;
  struct file_list *next;
};
typedef struct file_list file_list;

// Returns true if the list contains a node whose hash and name match
bool file_list_contains(const file_list *list, file_list *node);

void file_list_free(file_list *list);
// duplicates a single node.
// node.next = NULL
file_list *file_list_dup_node(const file_list *node);
// duplicates the entire list
file_list *file_list_dup(const file_list *list);

size_t file_list_len(const file_list *list);
// Reads the directory and updates the file list in place
void file_list_update(file_list **list, const char *path);
