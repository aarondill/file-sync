#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct file_list {
  size_t name_len;
  // Note: name is relative to the directory the file_list was read from
  char name[256];
  uint8_t hash[16];
  size_t size;
  struct file_list *next;
};
typedef struct file_list file_list;

// Returns true if the list contains a node whose hash and name match
const file_list *file_list_find(const file_list *list, file_list *node);
const file_list *file_list_find_path(const file_list *list, const char *path);
const file_list *file_list_find_hash(const file_list *list,
                                     const uint8_t *hash);

file_list *file_list_new(const char *name, size_t name_len, size_t size,
                         const uint8_t *hash);
void file_list_free(file_list *list);
// duplicates a single node.
// node.next = NULL
file_list *file_list_dup_node(const file_list *node);
// duplicates the entire list
file_list *file_list_dup(const file_list *list);

size_t file_list_len(const file_list *list);

// Inserts a list after the given node in the list
// node.next == to_insert; to_insert's tail's next is node.next
void file_list_insert(file_list **node, file_list *to_insert);
void file_list_append(file_list **list, file_list *to_insert);

// Reads the directory and returns a list of all files in the directory
file_list *file_list_read(const char *path);
