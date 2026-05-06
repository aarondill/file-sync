#include "common/download.h"
#include "common/file_list.h"
#include "common/protocol.h"
#include "common/upload.h"
#include "common/util.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
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
pthread_rwlock_t rwlock_clients = PTHREAD_RWLOCK_INITIALIZER;
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

  pthread_rwlock_wrlock(&rwlock_clients);
  // use memcpy to solve the const problem
  clients = memcpy(client, &(client_list){.info = ci, .next = clients},
                   sizeof(client_list));
  pthread_rwlock_unlock(&rwlock_clients);

  return client;
}
void remove_client(const client_info *client) {
  pthread_rwlock_wrlock(&rwlock_clients);
  client_list **cur = &clients;
  // find the client info in the list (must be unique)
  while (*cur && &(*cur)->info != client)
    cur = &(*cur)->next;
  assert(*cur); // undefined behavior if client not in list
  client_list *tmp = *cur;
  *cur = (*cur)->next;
  pthread_rwlock_unlock(&rwlock_clients);
  close(client->connfd);
  close(client->piperfd);
  close(client->pipewfd);
  free(tmp);
}
// writes a single byte to all clients except the one specified
void write_other_clients(const client_info *connfd) {
  pthread_rwlock_rdlock(&rwlock_clients);
  uint8_t buf[1] = {0};
  for (client_list *cur = clients; cur; cur = cur->next)
    if (&cur->info != connfd)
      write_all(cur->info.pipewfd, buf, 1);
  pthread_rwlock_unlock(&rwlock_clients);
}

pthread_rwlock_t rwlock_files = PTHREAD_RWLOCK_INITIALIZER;
file_list *global_list = NULL;
void update_list(const char *directory) {
  pthread_rwlock_wrlock(&rwlock_files);
  file_list_free(global_list);
  global_list = file_list_read(directory);
  pthread_rwlock_unlock(&rwlock_files);
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
  client_connect_m msg = {0};
  { // read the client connect message
    uint8_t buf[BUFSIZE];
    read_message(client->connfd, buf, sizeof(buf));
    serror_t err = 0;
    deserialize_client_connect(&msg, buf, sizeof(buf), &err);
    if (err) {
      error("error deserializing client connect message\n");
      goto cleanup;
    }
    printf("Client connected: ");
    printlen(msg.name, msg.name_len);
    printf("\n");
  }

  // The server starts by sending an upload to the client unless the client
  // explicitly requests otherwise
  if (msg.flags & INTENT_TO_UPLOAD) {
    uint8_t buf[1] = {0};
    write_all(client->pipewfd, buf, 1); // write to initiate an upload
  }

  const int CONNIND = 0, PIPEIND = 1;
  struct pollfd pfds[2] = {
      {.fd = client->connfd, .events = POLL_IN},
      {.fd = client->piperfd, .events = POLL_IN},
  };

  while (!stop) {
    int ret = poll(pfds, 2, -1);
    assert(ret != 0); // no timeout, so this should be true
    if (ret < 0) {
      if (errno == EINTR)
        continue;
      perror("poll");
      goto cleanup;
    }
    if (pfds[PIPEIND].revents & POLLIN) { // pipe has data, should upload
      if (pfds[CONNIND].revents & POLLIN) {
        error("both pipe and conn have data!\n");
        goto cleanup;
      }
      pthread_rwlock_rdlock(&rwlock_files);
      bool success = upload(client->connfd, global_list, directory);
      pthread_rwlock_unlock(&rwlock_files);
      if (!success) {
        error("error uploading files\n");
        goto cleanup;
      }
      char buf[1024];
      while (read(client->piperfd, buf, sizeof(buf)) > 0) // empty the pipe
        ;
    } else if (pfds[CONNIND].revents &
               POLLIN) { // conn has data, should download

      pthread_rwlock_rdlock(&rwlock_files);
      bool success = download(client->connfd, global_list, directory);
      pthread_rwlock_unlock(&rwlock_files);
      if (success) {
        update_list(directory);
        write_other_clients(client);
      } else if (errno != EINTR) {
        error("error downloading files\n");
        goto cleanup;
      }
    } else {
      error("neither pipe nor conn has data!\n"); // how'd we get here?
      goto cleanup;
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
    int connfd = accept(listenfd, NULL, NULL);
    if (connfd < 0) {
      perror("accept");
      return 1;
    }
    client_list *client = add_client(connfd);
    pthread_t tid;
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
