#include "lib/FileInfo.h"
#include "lib/download.h"
#include "lib/protocol/structs.h"
#include "lib/upload.h"
#include "lib/util.h"
#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#define BUFSIZE 4096

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

volatile bool upload_pending = false;
volatile bool stop = false;

// No locking is required here, since the client is single-threaded
std::vector<FileInfo> global_list;
void update_list(const char *directory) {
  global_list = FileInfo::readList(directory);
}

extern "C" void signal_handler(const int signum) {
  if (signum == SIGUSR1) {
    upload_pending = true;
  } else if (signum == SIGTERM || signum == SIGINT || signum == SIGQUIT) {
    stop = true;
  }
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
  char *server = argv[optind]; // name:port; name may be host or IP
  char *directory = argv[optind + 1];

  if (const auto s = fs::status(directory); //
      s.type() != fs::file_type::directory ||
      fs::perms::none == (s.permissions() & fs::perms::owner_read) ||
      fs::perms::none == (s.permissions() & fs::perms::owner_write)) {
    throw std::runtime_error("directory is not readable or writable\n");
  }

  std::byte buf[BUFSIZE];

  FileDescriptor sockfd = init_connection(server);
  { // send connect message
    const protocol::client_connect_m msg = init_connect_msg(should_upload);
    const auto b = serialize(buf, msg);
    if (!b) throw std::runtime_error("error serializing client connect message\n");
    write_message(sockfd, b.value());
  }

  update_list(directory);

  if (should_upload) upload_pending = true;

  struct sigaction sa = {};
  sa.sa_handler = signal_handler;
  sa.sa_flags = SA_RESTART; // don't fail read() or write()
  for (const int s : {SIGUSR1, SIGTERM, SIGINT, SIGQUIT})
    sigaction(s, &sa, nullptr);

  while (!stop) {
    // TODO: poll() on sockfd and a pipe instead of `upload_pending`
    if (!upload_pending) {
      download(sockfd, global_list, directory);
      // update the list on a successful download
      update_list(directory);
    }
    // note: this value can change during `download`
    if (upload_pending) {
      upload(sockfd, global_list, directory);
      upload_pending = false;
    }
  }
}
