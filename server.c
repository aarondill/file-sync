#include "common/download.h"
#include "common/file_list.h"
#include "common/protocol.h"
#include "common/upload.h"
#include "common/util.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define BUFSIZE 4096

// #define MAXUSERS 100
// TODO: multithreading. Keep a list of tids and use pthread_kill to signal an
// upload
// TODO: use a rwlock on global_list
file_list *global_list = NULL;
// TODO: make array (bool per thread) of upload_pending
volatile bool upload_pending = false;
volatile bool stop = false;

void signal_handler(int signum) {
  if (signum == SIGTERM || signum == SIGINT || signum == SIGQUIT) {
    stop = true;
  }
}

int main(int argc, char **argv) {
  if (argc > 3 || argc < 2) {
    fprintf(stderr, "usage: %s <dir> [port]\n", argv[0]);
    return 2;
  }
  int port = 8080;
  if (argc == 3) {
    port = atoi(argv[2]);
  }
  char *directory = argv[1];

  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    perror("socket");
    return 1;
  }

  struct sockaddr_in serv_addr = {
      .sin_addr.s_addr = htonl(INADDR_ANY),
      .sin_family = AF_INET,
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

  file_list_free(global_list);
  global_list = file_list_read(directory);
  while (true) {
    // TODO: create a thread here for dedicated server
    int connfd = accept(listenfd, NULL, NULL);
    uint8_t buf[BUFSIZE];
    read_message(connfd, buf, sizeof(buf));
    serror_t err = 0;
    client_connect_m msg = {0};
    deserialize_client_connect(&msg, buf, sizeof(buf), &err);
    if (err) {
      error("error deserializing client connect message\n");
      return 1;
    }
    printf("Client connected: ");
    printlen(msg.name, msg.name_len);
    printf("\n");

    // The server starts by sending an upload to the client unless the client
    // explicitly requests otherwise
    upload_pending = !(msg.flags & INTENT_TO_UPLOAD);

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);

    while (!stop) {
      if (!upload_pending) {
        if (!download(connfd, global_list) && errno != EINTR) {
          error("error downloading files\n");
          return 1;
        }
        // TODO: mark other threads as uploading
      }
      // note: this value can change during `download`
      if (upload_pending) {
        if (!upload(connfd, global_list)) {
          error("error uploading files\n");
          return 1;
        }
        upload_pending = false;
      }
    }

    close(connfd);
  }
  close(listenfd);
  return 0;
}
