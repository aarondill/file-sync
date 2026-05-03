# File Sync server

The server will maintain a directory of files to be synced with all connected clients.

- On client connect, the client will send client info which includes an optional "intent to upload" bit.
- If the client has intent to upload, skip the initial download and start an upload

To download:

- The server will send a list of files and hashes to the client. (server→client download message 1 - no file contents)
- The client will then send the list of files that it needs to download (hashes differ) (server→client download response)
- Server sends each file to the client (name, size, hash, data) (server→client download message 2)
- Client receives each file and verifies the hash
- Note: on download, any files not in the list shall be deleted
  - The client may move files to a "trash" directory

To upload:

- Client sends a list of files and hashes to the server. (client→server download message 1 - no file contents)
- Server sends a list of files that need to be uploaded (hashes differ) (client→server download response)
- Client sends each file to the server (name, size, hash, data) (client→server download message 2)
- Server receives each file and verifies the hash
- Server initiates download on each client except the one that initiated the upload

Note that this means the server is effectively a client that additionally maintains a list of connections and initiates downloads post-receive.

## Protocol

All messages start with an uint16 length field, followed by the header length!

### Client connect message

- 8 bits for protocol version
  - Must be 1
  - The server must reject any connection with a different version. No backwards compatibility here.
- 8 bits of flags
  - bit 0 - intent to upload
  - rest of bits are reserved for future use
- 8 bits for client name length in bytes
- human-readable client name (max of 255 bytes, variable length)
  - suggested to use hostname
  - Note: this is not required to be unique
  - Clients _must_ be identified by IP/port
  - It is only for output purposes, the server should indicate if duplicate names are detected (output IP/port)

Total header size is 3 bytes

Note that this may be responded with an error if there are too many clients.

### Download message

1. download message 1 - no file contents
2. download response
3. download message 2 - file contents

- 8 bits for flags
  - bit 0 - error
    - Must be 0
  - rest of bits are reserved for future use
- 8 bits for file count
  - Note: supports up to 255 files
- for each file:
  - 128 bits for file hash (MD5)
  - 8 bits for file name length in bytes
    - Note: supports up to 255 bytes
    - Files with longer names should be rejected (don't truncate because it can cause security issues, and split characters in the middle)
  - 32 bits for file size in bytes
    - Note: supports up to 4 GB
    - Must be zero on initial message → client responds with subset of files to download
  - File name (max of 255 bytes, variable length)
  - File data (variable length)

Total header size is 2 bytes
Each file header is 21 bytes

### Download response

- 8 bits for flags
  - bit 0 - error
    - Must be 0
  - bits 1-7 - reserved
- 8 bits for file count
- for each file:
  - 128 bits for file hash (MD5)
    - server identifies file by hash
    - respond with error if hash not found (must process all names before initiating download)

Total header size is 2 bytes
Each file header is 16 bytes

### Error message

- 8 bits for flags
  - bit 0 - error
    - Must be 1
  - rest of bits are reserved for future use
- 8 bits for error code
- 8 bits for error message length in bytes
  - Note: supports up to 255 bytes
- variable length error message

Total header size is 3 bytes
