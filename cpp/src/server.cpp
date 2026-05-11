#include "lib/FileInfo.h"
#include "lib/download.h"
#include "lib/protocol/serial.h"
#include "lib/protocol/structs.h"
#include "lib/upload.h"
#include "lib/util.h"
#include <arpa/inet.h>
#include <cassert>
#include <csignal>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <shared_mutex>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_set>
#define BUFSIZE 4096

struct client_info {
  FileDescriptor conn;
  // write to initiate an upload
  struct pipe {
    FileDescriptor read;
    FileDescriptor write;
    bool operator==(const pipe &) const = default;
  } pipe;
  explicit client_info(FileDescriptor fd) : conn{std::move(fd)} {
    int pipes[2];
    if (::pipe(pipes) == -1) throw std::runtime_error("pipe failed");
    pipe = {pipes[0], pipes[1]};
  }
  bool operator==(const client_info &) const = default;
};

// each thread receives a pointer to its client_list
std::shared_mutex TODO_clients; // TODO:
// you *must* lock any time you access this
std::vector<client_info> clients;
client_info &add_client(FileDescriptor connfd) {
  // lock write client list
  // pthread_rwlock_wrlock(&rwlock_clients);
  return clients.emplace_back(std::move(connfd));
}
void remove_client(const client_info &client) {
  // lock write client list
  // pthread_rwlock_wrlock(&rwlock_clients);
  const auto i = std::ranges::find(clients, client);
  if (i == clients.end()) throw std::logic_error("remove client that's not in the list");
  clients.erase(i);
}
// writes a single byte to all clients except the one specified
void write_other_clients(const client_info &cur) {
  // pthread_rwlock_rdlock(&rwlock_clients);
  constexpr std::byte buf[1]{};
  for (client_info &info : clients)
    if (info != cur) info.pipe.write.write(buf);
}

std::shared_mutex TODO_files; // TODO:
std::vector<FileInfo> global_list;
void update_list(const fs::path &directory) {
  // pthread_rwlock_wrlock(&rwlock_files);
  global_list = FileInfo::readList(directory);
}

volatile bool stop = false;
void signal_handler(const int signum) {
  if (signum == SIGTERM || signum == SIGINT || signum == SIGQUIT) { stop = true; }
  // SIGUSR1 in client
}

// this may not change after startup
fs::path directory;

void client_thread(void *arg) {
  auto &client = *static_cast<client_info *>(arg);
  protocol::client_connect_m msg;
  std::byte buf[BUFSIZE];
  { // read the client connect message
    const auto b = read_message(client.conn, buf);
    if (!deserialize(msg, b))
      throw std::runtime_error("error deserializing client connect message");
    std::cout << "Client connected: " << std::string_view{msg.name, msg.name_len} << std::endl;
  }

  // The server starts by sending an upload to the client unless the client
  // explicitly requests otherwise
  if (!(msg.flags & protocol::INTENT_TO_UPLOAD)) {
    constexpr std::byte b[1]{};
    client.pipe.write.write(b); // write to initiate an upload
  }

  const int CONNIND = 0, PIPEIND = 1;
  pollfd pfds[2] = {
      {.fd = static_cast<int>(client.conn), .events = POLL_IN, .revents{}},
      {.fd = static_cast<int>(client.pipe.read), .events = POLL_IN, .revents{}},
  };

  while (!stop) {
    int ret = poll(pfds, 2, -1);
    assert(ret != 0); // no timeout, so this should be true
    if (ret < 0) throw std::runtime_error(std::strerror(errno));
    if (pfds[PIPEIND].revents & POLLIN) { // pipe has data, should upload
      if (pfds[CONNIND].revents & POLLIN) throw std::runtime_error("both pipe and conn have data!");

      // pthread_rwlock_rdlock(&rwlock_files);
      upload(client.conn, global_list, directory);
      // pthread_rwlock_unlock(&rwlock_files);
      std::byte b[1]; // empty the pipe
      client.pipe.read.read(b);
    } else if (pfds[CONNIND].revents & POLLIN) { // conn has data, should download
                                                 // pthread_rwlock_rdlock(&rwlock_files);
      download(client.conn, global_list, directory);
      // pthread_rwlock_unlock(&rwlock_files);
      update_list(directory);
      write_other_clients(client);
    } else
      throw std::runtime_error("neither pipe nor conn has data!"); // how'd we get here?
  }
}

int main(const int argc, char **argv) {
  if (argc > 3 || argc < 2) {
    std::cerr << "usage: " << argv[0] << " <dir> [port]" << std::endl;
    return 2;
  }
  int port = 8080;
  if (argc == 3) { port = atoi(argv[2]); }
  directory = argv[1];

  if (const auto s = fs::status(directory); //
      s.type() != fs::file_type::directory ||
      fs::perms::none == (s.permissions() & fs::perms::owner_read) ||
      fs::perms::none == (s.permissions() & fs::perms::owner_write)) {
    throw std::runtime_error("directory is not readable or writable\n");
  }

  int socket_ret = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_ret < 0) throw std::runtime_error(std::strerror(errno));
  FileDescriptor listenfd{socket_ret};
  { // set SO_REUSEADDR to make debugging easier
    constexpr int val = 1;
    setsockopt(static_cast<int>(listenfd), SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  }

  sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = {.s_addr = htonl(INADDR_ANY)},
      .sin_zero{} //
  };

  if (bind(static_cast<int>(listenfd), reinterpret_cast<sockaddr *>(&serv_addr),
           sizeof(serv_addr)) < 0)
    throw std::runtime_error(std::strerror(errno));
  std::cout << "Connect to the server at port " << port << std::endl;

  if (listen(static_cast<int>(listenfd), 10) < 0) throw std::runtime_error(std::strerror(errno));

  struct sigaction sa = {};
  sa.sa_handler = signal_handler;
  sa.sa_flags = SA_RESTART; // don't fail read() or write()
  for (const int s : {SIGUSR1, SIGTERM, SIGINT, SIGQUIT})
    sigaction(s, &sa, nullptr);

  update_list(directory); // update the list before starting the server
  // pthread_attr_t attr;
  // pthread_attr_init(&attr);
  // pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  while (!stop) {
    int connfd = accept(static_cast<int>(listenfd), nullptr, nullptr);
    if (connfd < 0) throw std::runtime_error(std::strerror(errno));
    client_info &client = add_client(connfd);
    client_thread(&client);
    // pthread_t tid;
    // if (pthread_create(&tid, NULL, client_thread, (void *)&client->info)) {
    //   perror("pthread_create");
    //   return 1;
    // }
  }
  std::cout << "Closing server" << std::endl;
  // pthread_exit(NULL); // allow other threads to exit cleanly
}
