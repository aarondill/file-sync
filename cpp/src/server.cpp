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
#include <mutex>
#include <netinet/in.h>
#include <poll.h>
#include <shared_mutex>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <unordered_set>

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
    pipe.read = FileDescriptor{pipes[0]};
    pipe.write = FileDescriptor{pipes[1]};
  }
  bool operator==(const client_info &) const = default;
};

// each thread receives a pointer to its client_list
std::shared_mutex client_mutex; // TODO:
// you *must* lock any time you access this
std::vector<client_info> clients;
client_info &add_client(FileDescriptor conn) {
  client_info c{std::move(conn)}; // construct, then lock and move. construct has a syscall.
  std::unique_lock l{client_mutex};
  return clients.emplace_back(std::move(c));
}
void remove_client(const client_info &client) {
  std::unique_lock l{client_mutex};
  const auto i = std::ranges::find(clients, client);
  if (i == clients.end()) throw std::logic_error("remove client that's not in the list");
  clients.erase(i);
}
// writes a single byte to all clients except the one specified
void write_other_clients(const client_info &cur) {
  std::shared_lock l{client_mutex};
  constexpr std::byte buf[1]{};
  for (client_info &info : clients)
    if (info != cur) info.pipe.write.write(buf);
}

std::shared_mutex file_mutex; // TODO:
std::vector<FileInfo> global_list;
void update_list(const fs::path &directory) {
  auto list = FileInfo::readList(directory);
  std::unique_lock l{file_mutex};
  global_list = std::move(list);
}

std::atomic_flag stop = false;
void signal_handler(const int signum) {
  if (signum == SIGTERM || signum == SIGINT || signum == SIGQUIT) { stop.test_and_set(); }
  // SIGUSR1 in client
}

// this may not change after startup
fs::path directory;

void client_thread(client_info &client) {
  protocol::client_connect_m msg;
  std::byte buf[4096];
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

  constexpr int CONN_IND = 0, PIPE_IND = 1;
  pollfd p_fds[2];
  p_fds[CONN_IND] = {.fd = static_cast<int>(client.conn), .events = POLL_IN, .revents{}};
  p_fds[PIPE_IND] = {.fd = static_cast<int>(client.pipe.read), .events = POLL_IN, .revents{}};

  while (!stop.test()) {
    const int ret = poll(p_fds, 2, -1);
    assert(ret != 0); // no timeout, so this should be true
    if (ret < 0) throw std::runtime_error(std::strerror(errno));
    if (p_fds[PIPE_IND].revents & POLLIN) { // pipe has data, should upload
      if (p_fds[CONN_IND].revents & POLLIN)
        throw std::runtime_error("both pipe and conn have data!");

      {
        std::shared_lock l{file_mutex};
        upload(client.conn, global_list, directory);
      }
      std::byte b[1]; // empty the pipe
      client.pipe.read.read(b);
    } else if (p_fds[CONN_IND].revents & POLLIN) { // conn has data, should download
      {
        std::shared_lock l{file_mutex};
        download(client.conn, global_list, directory);
      }
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

  const int socket_ret = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_ret < 0) throw std::runtime_error(std::strerror(errno));
  const FileDescriptor fd{socket_ret};
  { // set SO_REUSEADDR to make debugging easier
    constexpr int val = 1;
    setsockopt(static_cast<int>(fd), SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  }

  sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = {.s_addr = htonl(INADDR_ANY)},
      .sin_zero{} //
  };

  if (bind(static_cast<int>(fd), reinterpret_cast<sockaddr *>(&serv_addr), sizeof(serv_addr)) < 0)
    throw std::runtime_error(std::strerror(errno));
  std::cout << "Connect to the server at port " << port << std::endl;

  if (listen(static_cast<int>(fd), 10) < 0) throw std::runtime_error(std::strerror(errno));

  struct sigaction sa = {};
  sa.sa_handler = signal_handler;
  sa.sa_flags = SA_RESTART; // don't fail read() or write()
  for (const int s : {SIGUSR1, SIGTERM, SIGINT, SIGQUIT})
    sigaction(s, &sa, nullptr);

  update_list(directory); // update the list before starting the server

  auto client_wrapper = [](client_info &i) {
    try {
      return client_thread(i);
    } catch (const std::exception &e) {
      std::cerr << "Thread exited with exception: " << e.what() << std::endl;
    }
  };

  while (!stop.test()) {
    const int conn = accept(static_cast<int>(fd), nullptr, nullptr);
    if (conn < 0) throw std::runtime_error(std::strerror(errno));
    client_info &client = add_client(FileDescriptor{conn});
    std::jthread t{client_wrapper, std::ref(client)};
    t.detach();
  }
  std::cout << "Closing server" << std::endl;
}
