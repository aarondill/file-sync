#include "common/download.h"
#include "common/file_list.h"
#include "common/protocol.h"
#include "common/upload.h"
#include "common/util.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

volatile bool upload_pending = false;
volatile bool stop = false;

// No locking is required here, since the client is single-threaded
file_list *global_list = NULL;
void update_list(const char *directory) {
  file_list_free(global_list);
  global_list = file_list_read(directory);
}

void signal_handler(int signum) {
  if (signum == SIGUSR1) {
    upload_pending = true;
  } else if (signum == SIGTERM || signum == SIGINT || signum == SIGQUIT) {
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

  file_list_free(global_list);
  global_list = file_list_read(directory);

  if (should_upload)
    upload_pending = true;

  struct sigaction sa = {.sa_handler = signal_handler};
  sigaction(SIGUSR1, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);

  while (!stop) {
    if (!upload_pending) {
      // download may be interrupted by a signal
      if (download(sockfd, global_list, directory) || errno == EINTR) {
        // update the list on a successful download or on interrupt (so the
        // following upload is valid, since files may have changed)
        update_list(directory);
      } else if (errno != EINTR) {
        error("error downloading files\n");
        return 1;
      }
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
