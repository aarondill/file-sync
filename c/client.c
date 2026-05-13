#include "common/download.h"
#include "common/file_list.h"
#include "common/protocol.h"
#include "common/upload.h"
#include "common/util.h"
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define BUFSIZE 4096

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

volatile bool stop = false;

// No locking is required here, since the client is single-threaded
file_list *global_list = NULL;
void update_list(const char *directory) {
  file_list_free(global_list);
  global_list = file_list_read(directory);
}

void signal_handler(int signum) {
  if (signum == SIGTERM || signum == SIGINT || signum == SIGQUIT) {
    stop = true;
  }
}

int main(int argc, char **argv) {
  bool should_upload = false;
  int opt;
  while ((opt = getopt(argc, argv, "u")) != -1) {
    switch (opt) {
    case 'u':
      should_upload = true;
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
  if (access(directory, R_OK | W_OK)) {
    error("directory %s is not readable or writable\n", directory);
    return 3;
  }

  int sockfd = init_connection(server);

  { // send connect message
    client_connect_m msg = init_connect_msg(NULL, should_upload);
    uint8_t buf[BUFSIZE];
    serror_t err = 0;
    size_t len = serialize_client_connect(buf, sizeof(buf), &msg, &err);
    if (err) {
      error("error serializing client connect message\n");
      return 1;
    }

    if (!write_message(sockfd, buf, len)) {
      error("error writing client connect message\n");
      return 1;
    }
  }

  update_list(directory);

  bool upload_pending = false;
  if (should_upload)
    upload_pending = true;

  struct sigaction sa = {.sa_handler = signal_handler};
  sa.sa_flags = SA_RESTART; // no more interrupts :)
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);

  const int CONN_IND = 0, STDIN_IND = 1;
  struct pollfd pfds[2] = {
      {.fd = sockfd, .events = POLL_IN},
      {.fd = STDIN_FILENO, .events = POLL_IN},
  };

  while (!stop) {
    int ret = poll(pfds, 2, -1);
    assert(ret != 0); // no timeout, so this should be true
    if (ret < 0) {
      if (errno == EINTR)
        continue;
      perror("poll");
    }
    if (pfds[STDIN_IND].revents & POLLIN) {
      char c = getchar();
      // ignore whitespace, we'll just read it next time around
      if (!isspace(c)) {
        switch (c) {
        case 'q':
          stop = true;
          break;
        case 'u':
          upload_pending = true;
          break;
        case 'h':
          printf("commands: q: quit, u: upload, h: help\n");
          break;
        default:
          fprintf(stderr, "unknown command: %c\n", c);
          break;
        }
      }
    }
    if (stop)
      break;

    if (pfds[CONN_IND].revents & POLLIN) {
      // conn has data, should download
      if (!download(sockfd, global_list, directory)) {
        error("error downloading files\n");
        return 1;
      }
      // update the list on a successful download
      update_list(directory);
    }
    // note: this value can change during `download`
    if (upload_pending) {
      if (!upload(sockfd, global_list, directory)) {
        error("error uploading files\n");
        return 1;
      }
      upload_pending = false;
    }
  }
  close(sockfd);
  return 0;
}
