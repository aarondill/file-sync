#include "common/download.h"
#include "common/file_list.h"
#include "common/protocol.h"
#include "common/upload.h"
#include "common/util.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define BUFSIZE 4096

typedef struct client_list client_list;
typedef struct client_info client_info;
struct client_list {
  const struct client_info {
    const int connfd;
    const int piperfd;
    const int pipewfd; // write to initiate an upload
  } info;
  // you *must* lock any time you access this
  client_list *next;
};

// each thread receives a pointer to its client_list
// use a read-write lock to protect this
client_list *clients = NULL;
client_list *add_client(int connfd) {
  client_list *client = malloc(sizeof(client_list));
  assert(client);
  int pipefds[2];
  pipe(pipefds);
  client_info ci = {
      .connfd = connfd, .piperfd = pipefds[0], .pipewfd = pipefds[1]};

  // TODO: lock the client_list
  memcpy(client, // use memcpy to solve the const problem
         &(client_list){.info = ci, .next = clients}, sizeof(client_list));
  clients = client;
  // TODO: unlock the client_list

  return client;
}
void remove_client(const client_info *client) {
  // TODO: lock the client_list
  client_list **cur = &clients;
  // find the client info in the list (must be unique)
  while (*cur && &(*cur)->info != client)
    cur = &(*cur)->next;
  if (!*cur) {
    warn("client not found\n");
    return;
  }
  client_list *tmp = *cur;
  *cur = (*cur)->next;
  // TODO: unlock the client_list
  close(client->connfd);
  close(client->piperfd);
  close(client->pipewfd);
  free(tmp);
}

file_list *global_list = NULL;
void update_list(const char *directory) {
  // TODO: use a rwlock on global_list
  file_list_free(global_list);
  global_list = file_list_read(directory);
}

volatile bool stop = false;
void signal_handler(int signum) {
  if (signum == SIGTERM || signum == SIGINT || signum == SIGQUIT) {
    stop = true;
  }
}
// this may not change after startup
const char *directory = NULL;

void *client_thread(void *arg) {
  const client_info *client = arg;
  uint8_t buf[BUFSIZE];
  read_message(client->connfd, buf, sizeof(buf));
  serror_t err = 0;
  client_connect_m msg = {0};
  deserialize_client_connect(&msg, buf, sizeof(buf), &err);
  if (err) {
    error("error deserializing client connect message\n");
    goto cleanup;
  }
  printf("Client connected: ");
  printlen(msg.name, msg.name_len);
  printf("\n");

  // The server starts by sending an upload to the client unless the client
  // explicitly requests otherwise
  if (msg.flags & INTENT_TO_UPLOAD) {
    // TODO: write to pipewfd
  }

  while (!stop) {
    bool upload_pending = false; // TODO: read from piperfd
    if (!upload_pending) {
      // TODO: select() on connfd and piperfd

      if (download(client->connfd, global_list, directory)) {
        // NOTE: don't update on interupt, since the interrupting thread
        // should have already updated it
        update_list(directory);
      } else if (errno != EINTR) {
        error("error downloading files\n");
        goto cleanup;
      }
      // TODO: mark other threads as uploading
    }
    // note: this value can change during `download`
    if (upload_pending) {
      if (!upload(client->connfd, global_list, directory)) {
        error("error uploading files\n");
        goto cleanup;
      }
      upload_pending = false;
    }
  }

cleanup:
  remove_client(client);
  return NULL;
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
  directory = argv[1];
  if (access(directory, R_OK | W_OK)) {
    error("directory %s is not readable or writable\n", directory);
    return 3;
  }

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

  struct sigaction sa = {.sa_handler = signal_handler};
  /* sigaction(SIGUSR1, &sa, NULL); */
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  // block SIGPIPE
  sigaction(SIGPIPE, &(struct sigaction){.sa_handler = SIG_IGN}, NULL);

  update_list(directory); // update the list before starting the server
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  while (!stop) {
    // TODO: create a thread here for dedicated server
    int connfd = accept(listenfd, NULL, NULL);
    if (connfd < 0) {
      perror("accept");
      return 1;
    }
    client_list *client = add_client(connfd);
    pthread_t tid; // TODO: use pthread_create
    if (pthread_create(&tid, NULL, client_thread, (void *)&client->info)) {
      perror("pthread_create");
      return 1;
    }
  }
  printf("Closing server\n");
  file_list_free(global_list);
  close(listenfd);
  pthread_exit(NULL); // allow other threads to exit cleanly
}
