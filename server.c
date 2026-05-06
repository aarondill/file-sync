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
file_list *global_list = NULL;
void update_list(const char *directory) {
  // TODO: use a rwlock on global_list
  file_list_free(global_list);
  global_list = file_list_read(directory);
}
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
  // set SO_REUSEADDR to make debugging easier
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

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

    struct sigaction sa = {.sa_handler = signal_handler};
    /* sigaction(SIGUSR1, &sa, NULL); */
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    while (!stop) {
      if (!upload_pending) {
        if (download(connfd, global_list, directory)) {
          // NOTE: don't update on interupt, since the interrupting thread
          // should have already updated it
          update_list(directory);
        } else if (errno != EINTR) {
          error("error downloading files\n");
          return 1;
        }
        // TODO: mark other threads as uploading
      }
      // note: this value can change during `download`
      if (upload_pending) {
        if (!upload(connfd, global_list, directory)) {
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
