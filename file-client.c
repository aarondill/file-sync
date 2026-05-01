#include "util.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
int main(int argc, char **argv) {
  bool update = false;
  int opt;
  while ((opt = getopt(argc, argv, "u")) != -1) {
    switch (opt) {
    case 'u':
      update = true;
      break;
    default:
      fprintf(stderr, "usage: %s <server ip> <directory> [-u]\n", argv[0]);
      return 2;
    }
  }
  if (optind + 1 >= argc) {
    fprintf(stderr, "usage: %s <server ip> <directory> [-u]\n", argv[0]);
    return 2;
  }
  char *server = argv[optind]; // name:port; name may be host or IP
  char *directory = argv[optind + 1];

  int port = 8080;
  char *portp = strrchr(server, ':');
  if (portp != NULL) {
    *portp = '\0';
    port = atoi(portp + 1);
  }

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("socket");
    return 1;
  }

  struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr.s_addr = inet_addr(server),
  };
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("connect");
    return 1;
  }

  // TODO: send connection
  printf("connected\n");
  char buf[] = "hello world";
  write_all(sockfd, (uint8_t *)buf, sizeof(buf));
  int n;
  while ((n = read(sockfd, buf, sizeof(buf))) > 0) {
    printf("read %d bytes: \n", n);
    for (int i = 0; i < n; i++) {
      printf("%c ", buf[i]);
    }
    printf("\n");
  }
  // TODO: if update, send update
  // TODO: handle downloads
  close(sockfd);

  return 0;
}
