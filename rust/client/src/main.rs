use std::{
    ffi::OsString,
    fs::File,
    io,
    net::{SocketAddr, TcpStream},
    path::Path,
    process::ExitCode,
    str::FromStr,
};

use lib::{
    client_connect::{self, ClientConnect},
    file_info::FileInfo,
    protocol::write_message,
    serial::Serialize,
    variable_length_string::VariableLengthString,
};

// Initialize a client connect message
fn init_connect_msg(upload: bool) -> ClientConnect {
    let name = std::net::hostname().unwrap_or_else(|_| "unknown".into());
    let flags = if upload {
        client_connect::Flags::IntentToUpload
    } else {
        client_connect::Flags::empty()
    };
    let name = VariableLengthString::new_truncate(name.to_string_lossy().as_bytes());
    ClientConnect::new(flags, name)
}

fn update_list(directory: &Path, list: &mut Vec<FileInfo>) {
    list.clear();
    list.extend(FileInfo::read_list(directory));
}

fn parse_args(iter: impl Iterator<Item = String>) -> (Vec<String>, Vec<String>) {
    let mut positional = Vec::new();
    let mut opts = Vec::new();

    let mut iter = iter.skip(1);
    for arg in iter.by_ref().take_while(|arg| arg != "--") {
        // parse until "--" is reached
        if arg.starts_with("-") {
            opts.push(arg);
        } else {
            positional.push(arg);
        }
    }
    positional.extend(iter);
    (positional, opts)
}

fn main() -> ExitCode {
    let mut global_list = Vec::<FileInfo>::new();
    // TODO: ctrl-c
    // std::atomic_flag stop = false;

    let (args, opts) = parse_args(std::env::args());
    let should_upload = opts.contains(&"-u".to_string());
    if args.len() != 2 {
        eprintln!("usage: {} <server ip> <directory> [-u]", args[0]);
        return ExitCode::from(2);
    }
    let server = &args[1];
    let directory = Path::new(&args[2]);
    if !directory.is_dir()
        || directory
            .metadata()
            .map_or(true, |m| m.permissions().readonly())
    {
        eprintln!("directory is not readable or writable");
        return ExitCode::from(2);
    }

    // The server starts by sending an upload to the client unless the client
    // explicitly requests otherwise
    let mut upload_pending = should_upload;

    update_list(directory, &mut global_list); // update the list before starting the server

    let addr = server
        .rsplit_once(":")
        .map(|(host, port)| (host, port.parse().unwrap()))
        .unwrap_or((server, 8080));
    let mut connection = TcpStream::connect(addr).expect("error connecting to server");

    // send connect message
    {
        let msg = init_connect_msg(should_upload);
        let mut buf = Vec::with_capacity(4096);
        msg.serialize(&mut buf)
            .expect("error serializing client connect message");
        write_message(&mut connection, &buf).expect("error writing connect message");
    }

    //
    //   constexpr int CONN_IND = 0, STDIN_IND = 1;
    //   pollfd p_fds[2];
    //   p_fds[CONN_IND] = {.fd = static_cast<int>(connection), .events = POLL_IN, .revents{}};
    //   p_fds[STDIN_IND] = {.fd = STDIN_FILENO, .events = POLLIN, .revents{}};
    //
    //   std::unordered_map<char, std::pair<std::string_view, std::function<void()>>> commands{
    //       {'q', {"quit", []() { stop.test_and_set(); }}},
    //       {'u', {"upload", []() { upload_pending.test_and_set(); }}},
    //       {'h',
    //        {"help",
    //         [&commands]() {
    //           std::cout << "commands: ";
    //           for (const auto &[c, v] : commands)
    //             std::cout << c << ": " << v.first << std::endl;
    //           std::cout << std::endl;
    //         }}},
    //   };
    //
    //   while (!stop.test()) {
    //     if (!upload_pending.test()) {
    //       p_fds[CONN_IND].revents = 0;
    //       const int ret = poll(p_fds, std::size(p_fds), -1);
    //       assert(ret != 0); // no timeout, so this should be true
    //       // poll can still be interrupted by EINTR
    //       if (ret < 0 && errno != EINTR) throw std::runtime_error(std::strerror(errno));
    //
    //       if (p_fds[STDIN_IND].revents & POLLIN) {
    //         char c;
    //         std::cin.get(c);
    //         if (std::isspace(c)) { // ignore whitespace, we'll just read it next time around
    //         } else if (commands.contains(c)) {
    //           commands.at(c).second();
    //         } else {
    //           std::cerr << "unknown command: " << c << std::endl;
    //         }
    //       }
    //
    //       if (p_fds[CONN_IND].revents & POLLIN) {
    //         download(connection, global_list, directory);
    //         update_list(directory);
    //       }
    //     }
    //
    //     if (upload_pending.test()) {
    //       if (p_fds[CONN_IND].revents & POLLIN)
    //         throw std::runtime_error("upload pending while connection has data!");
    //
    //       update_list(directory); // files may change between downloads
    //       upload(connection, global_list, directory);
    //       upload_pending.clear();
    //     }
    //   }
    return ExitCode::SUCCESS;
}
