#include "protocol.h"
#include "util.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXUSERS 100
// a set of clients
int clientfds[MAXUSERS];
size_t n_clients = 0;
bool add_client(int fd) {
  if (n_clients >= MAXUSERS) {
    return false;
  }
  clientfds[n_clients++] = fd;
  return true;
}
void remove_client(int fd) {
  // TODO: lock around the global array
  size_t i = 0;
  while (i < n_clients && clientfds[i] != fd)
    i++;
  // move rest of the clients to the left
  for (size_t j = i; j < n_clients - 1; j++) {
    clientfds[j] = clientfds[j + 1];
  }
  n_clients--;
}

void initiate_download(int clientfd) {
  // TODO: get file list & hashes
  download_m msg = {
      .flags = 0,
      .file_count = 0,
  };
  uint8_t buf[4096];
  serror_t err;
  size_t len = serialize_download(buf, sizeof(buf), &msg, &err);
  if (err != NO_ERROR) {
    printf("error serializing download message\n");
    return;
  }
  if (!write_all(clientfd, buf, len)) {
    printf("error sending download message\n");
    return;
  }

  download_file_m *files = malloc(sizeof(download_file_m) * msg.file_count);
  for (size_t i = 0; i < msg.file_count; i++) {
    len = serialize_download_file(buf, sizeof(buf), &files[i], &err);
    if (err != NO_ERROR) {
      printf("error serializing download file\n");
      return;
    }
    if (!write_all(clientfd, buf, len)) {
      printf("error sending download file\n");
      return;
    }
  }
  free(files);
  // TODO: read response
  // TODO: send download again
}

const int port = 8080; // TODO: make this a command line argument?
int main(void) {
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    perror("socket");
    return 1;
  }

  struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_addr = {.s_addr = htonl(INADDR_ANY)},
      .sin_port = htons(port),
  };

  if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("bind");
    return 1;
  }
  printf("Connect to the server at port %d\n", port);

  if (listen(listenfd, 10) < 0) {
    perror("listen");
    return 1;
  }

  while (true) {
    int connfd = accept(listenfd, NULL, NULL);
    if (!add_client(connfd)) {
      printf("too many clients\n");
      // TODO: send error message to client (define error codes)
      close(connfd);
      continue;
    }

    // create a  thread here for dedicated server with current numUsers as
    // parameter?
    // TODO: parse client connection message
    // TODO: wait for upload message
    // TODO: send download message

    close(connfd);
    // user has quit - update the global arrays?
    remove_client(connfd);
  }
}
