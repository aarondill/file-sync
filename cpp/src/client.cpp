#include "lib/FileInfo.h"
#include "lib/download.h"
#include "lib/protocol/structs.h"
#include "lib/upload.h"
#include "lib/util.h"
#include <arpa/inet.h>
#include <atomic>
#include <cassert>
#include <csignal>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_set>

// returns a socket file descriptor
// if it fails, exits
FileDescriptor init_connection(std::string_view server) {
  uint16_t port = 8080;
  if (const size_t i = server.find(':'); i != std::string::npos) {
    if (const auto p = server.substr(i + 1);
        std::from_chars(p.begin(), p.end(), port).ec != std::errc())
      throw std::runtime_error("invalid port");
    server = server.substr(0, i);
  }

  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) throw std::runtime_error(std::strerror(errno));
  FileDescriptor sockfd{fd};

  sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = {.s_addr = inet_addr(std::string{server}.c_str())},
      .sin_zero{},
  };
  if (::connect(fd, reinterpret_cast<sockaddr *>(&serv_addr), sizeof(serv_addr)) < 0)
    throw std::runtime_error(std::strerror(errno));

  return sockfd;
}
// Initialize a client connect message
// If name is NULL, the name will be set to the hostname returned by gethostname
protocol::client_connect_m init_connect_msg(const bool upload) {
  char buf[256]; // null terminate
  if (gethostname(buf, sizeof(buf)) < 0) throw std::runtime_error(std::strerror(errno));
  std::string_view name = {buf, strnlen(buf, std::size(buf))};

  uint8_t flags = upload ? protocol::client_connect_flags::INTENT_TO_UPLOAD : 0;
  return {flags, name};
}

std::vector<FileInfo> global_list;
void update_list(const fs::path &directory) {
  global_list = FileInfo::readList(directory);
}

std::atomic_flag stop = false;
std::atomic_flag upload_pending = false;
void signal_handler(const int signum) {
  if (signum == SIGTERM || signum == SIGINT || signum == SIGQUIT) { stop.test_and_set(); }
  if (signum == SIGUSR1) upload_pending.test_and_set();
}

int main(const int argc, char **argv) {
  bool should_upload = false;
  int opt;
  while ((opt = getopt(argc, argv, "u")) != -1) {
    switch (opt) {
    case 'u': should_upload = true; break;
    default:
      std::cerr << "usage: " << argv[0] << " <server ip> <directory> [-u]" << std::endl;
      return 2;
    }
  }
  if (optind + 1 >= argc) {
    std::cerr << "usage: " << argv[0] << " <server ip> <directory> [-u]" << std::endl;
    return 2;
  }
  const char *server = argv[optind]; // name:port; name may be host or IP
  const fs::path directory = argv[optind + 1];
  if (const auto s = fs::status(directory); //
      s.type() != fs::file_type::directory ||
      fs::perms::none == (s.permissions() & fs::perms::owner_read) ||
      fs::perms::none == (s.permissions() & fs::perms::owner_write)) {
    throw std::runtime_error("directory is not readable or writable\n");
  }

  struct sigaction sa = {};
  sa.sa_handler = signal_handler;
  sa.sa_flags = SA_RESTART; // don't fail read() or write()
  for (const int s : {SIGUSR1, SIGTERM, SIGINT, SIGQUIT})
    sigaction(s, &sa, nullptr);

  update_list(directory); // update the list before starting the server

  std::byte buf[4096];
  FileDescriptor connection{init_connection(server)};
  { // send connect message
    const protocol::client_connect_m msg = init_connect_msg(should_upload);
    const auto b = serialize(buf, msg);
    if (!b) throw std::runtime_error("error serializing client connect message\n");
    write_message(connection, b.value());
  }

  protocol::client_connect_m msg;
  { // read the client connect message
    const auto b = read_message(connection, buf);
    if (!deserialize(msg, b))
      throw std::runtime_error("error deserializing client connect message");
    std::cout << "Client connected: " << std::string_view{msg.name, msg.name_len} << std::endl;
  }

  // The server starts by sending an upload to the client unless the client
  // explicitly requests otherwise
  if (!(msg.flags & protocol::INTENT_TO_UPLOAD)) upload_pending.test_and_set();

  constexpr int CONN_IND = 0;
  pollfd p_fds[1];
  p_fds[CONN_IND] = {.fd = static_cast<int>(connection), .events = POLL_IN, .revents{}};

  while (!stop.test()) {
    p_fds[CONN_IND].revents = 0;
    const int ret = poll(p_fds, std::size(p_fds), -1);
    assert(ret != 0); // no timeout, so this should be true
    // poll can still be interrupted by EINTR
    if (ret < 0 && errno != EINTR) throw std::runtime_error(std::strerror(errno));
    if (p_fds[CONN_IND].revents & POLLIN) {
      download(connection, global_list, directory);
      update_list(directory);
    }

    if (upload_pending.test()) {
      if (p_fds[CONN_IND].revents & POLLIN)
        throw std::runtime_error("upload pending while connection has data!");

      upload(connection, global_list, directory);
    }
  }
}
