# File sync server

> [!WARNING]
> this is only a programming exercise. It's not meant to be used in production.

A server/client that syncs a single directory between machines. Each server manages multiple clients and is responsible for propagating changes to all other clients.

Each directory is a separate language implementation.

## Usage

Run `./server dir` to start the server.

Run `./client server dir` to sync `dir` to server. You can pass `-u` to upload on startup.
Type `h` to see a list of commands. Type `u` to initiate an upload.
