#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
int main(int argc, char **argv) {
  bool update = false;
  int opt;
  while ((opt = getopt(argc, argv, "u"))) {
    switch (opt) {
    case 'u':
      update = true;
      break;
    default:
      fprintf(stderr, "usage: %s <server> <directory> [-u]\n", argv[0]);
      return 2;
    }
  }
  if (optind + 2 >= argc) {
    fprintf(stderr, "usage: %s <server> <directory> [-u]\n", argv[0]);
    return 2;
  }
  char *server = argv[optind]; // name:port; name may be host or IP
  char *directory = argv[optind + 1];
  // TODO: connect to server
  //    - TODO: dns lookup
  // TODO: send connection
  // TODO: if update, send update
  // TODO: handle downloads
  return 0;
}
