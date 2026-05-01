#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void pexit(char *errmsg) {
  fprintf(stderr, "%s\n", errmsg);
  exit(1);
}

#define MAXUSERS 100
#define MAXLEN 100
char userids[MAXUSERS][MAXLEN];
int clientfds[MAXUSERS];
int numUsers = 0;

// introduce a function for thread -- work as dedicated server for a client
void *dedicatedServer(void *ptr) {
  int clientId = (int)(intptr_t)ptr;
  printf("dedicatedServer() called for client %d.\n", clientId);
  return NULL;
}

int main(void) {
  int listenfd = 0, connfd = 0;
  struct sockaddr_in serv_addr;

  char buffer[1025];

  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    pexit("socket() error.");

  memset(&serv_addr, '0', sizeof(serv_addr));
  memset(buffer, '0', sizeof(buffer));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  int port = 4999;
  do {
    port++;
    serv_addr.sin_port = htons(port);
  } while (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) <
           0);
  printf("bind() succeeds for port #%d\n", port);

  if (listen(listenfd, 10) < 0)
    pexit("listen() error.");

  while (1) {
    connfd = accept(listenfd, (struct sockaddr *)NULL, NULL);
    printf("connected to client %d.\n", numUsers);

    // read the userid here & store userid & connfd in global arrays
    read(connfd, buffer, sizeof(buffer));
    char *cptr = strchr(buffer, '\n');
    if (cptr)
      *cptr = '\0';

    strcpy(userids[numUsers], buffer);
    clientfds[numUsers] = connfd;

    // broadcast to all other users that a new user has joined?
    sprintf(buffer, "%s joined.\n", userids[numUsers]);
    // send this to all previous clients!
    for (int i = 0; i < numUsers; i++)
      write(clientfds[i], buffer, strlen(buffer));

    // create a  thread here for dedicated server with current numUsers as
    // parameter?
    numUsers++;

    if (fork() > 0)
      continue;

    int n;
    // read a line from the client and process!
    while ((n = read(connfd, buffer, sizeof(buffer))) > 0) {
      // read command, target userid and remaining string as the message
      char *command = strtok(buffer, " ");
      if (!strcmp(command, "send")) {
        char *targetUserId = strtok(NULL, " ");
        char *message = strtok(NULL, "\n");
        // validate the target userid
        int i;
        for (i = 0; i < numUsers; i++)
          if (!strcmp(targetUserId, userids[i])) {
            // send the message to that target user
            char forward[MAXLEN];
            sprintf(forward, "%s\n", message);
            write(clientfds[i], forward, strlen(forward));
            char *done = "Done.\n";
            write(connfd, done, strlen(done));
            break;
          }
        // indicate whether operation was successful. Done! (or) Invalid user
        if (i == numUsers) {
          char *invalidUser = "Invalid user.\n";
          write(connfd, invalidUser, strlen(invalidUser));
        }
      }
    }

    close(connfd);
    // user has quit - update the global arrays?

    exit(0); // this is child server process. It is done!
  }
}
