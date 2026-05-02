#include "protocol.h"
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

// returns a socket file descriptor
// if it fails, exits
int init_connection(const char *server) {
  int port = 8080;
  char *portp = strrchr(server, ':');
  if (portp != NULL) {
    *portp = '\0';
    port = atoi(portp + 1);
  }

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("socket");
    exit(1);
  }

  struct sockaddr_in serv_addr = {
      .sin_addr.s_addr = inet_addr(server),
      .sin_family = AF_INET,
      .sin_port = htons(port),
  };
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("connect");
    exit(1);
  }
  return sockfd;
}

// Initialize a client connect message
// If name is NULL, the name will be set to the hostname returned by gethostname
client_connect_m init_connect_msg(const char *name, bool upload) {
  client_connect_m msg = {
      .version = CLIENT_VERSION,
      .flags = upload ? INTENT_TO_UPLOAD : 0,
  };
  if (name) {
    // truncate name
    size_t len = strnlen(name, sizeof(msg.name));
    memcpy(msg.name, name, len);
    msg.name_len = len;
  } else {
    if (gethostname(msg.name, sizeof(msg.name)) < 0) {
      perror("gethostname");
      exit(1);
    }
    msg.name_len = strnlen(msg.name, sizeof(msg.name));
  }
  return msg;
}

int main(int argc, char **argv) {
  bool upload = false;
  int opt;
  while ((opt = getopt(argc, argv, "u")) != -1) {
    switch (opt) {
    case 'u':
      upload = true;
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
  int sockfd = init_connection(server);

  client_connect_m msg = init_connect_msg(NULL, upload);
  uint8_t buf[4096];
  serror_t err;
  size_t len = serialize_client_connect(buf, sizeof(buf), &msg, &err);
  if (err) {
    fprintf(stderr, "error serializing client connect message\n");
    return 1;
  }
  write_all(sockfd, (uint8_t *)buf, len);

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
