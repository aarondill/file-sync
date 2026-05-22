import { once } from "node:events";
import fs from "node:fs/promises";
import { createServer, Socket } from "node:net";
import { parseArgs } from "node:util";
import { download } from "./common/download.ts";
import { FileInfo } from "./common/FileInfo.ts";
import { ClientConnect } from "./common/messages.ts";
import { upload } from "./common/upload.ts";
import { access, readMessage } from "./common/util.ts";

let global_list: FileInfo[] = [];
async function updateList(directory: string) {
  global_list = await FileInfo.readList(directory);
}

const stop = new AbortController();
async function main(): Promise<number | undefined> {
  const { positionals } = parseArgs({
    args: process.argv.slice(2),
    options: {},
    allowPositionals: true,
  });
  const directory = positionals[0];
  const port = parseInt(positionals[1] ?? "8080");
  if (!directory) {
    console.error("usage: server.ts <directory> [port]");
    return 2;
  }

  if (!(await access(directory, fs.constants.R_OK | fs.constants.W_OK))) {
    console.error(`directory ${directory} is not readable or writable`);
    return 3;
  }
  updateList(directory);
  process.on("SIGTERM", () => stop.abort());
  process.on("SIGINT", () => stop.abort());
  process.on("SIGQUIT", () => stop.abort());

  // await using socket = new Socket().connect({ port, host });
  createServer(socket => {
    handleClient(socket, directory);
  }).listen(port);
}

// a list of functions to be called to start an upload.
const allClientUploads: Set<() => void> = new Set();

async function handleClient(socket: Socket, directory: string) {
  // recv connect message
  const [msg] = ClientConnect.deserialize(await readMessage(socket));
  console.log(`Client connected: ${msg.clientName}`);

  // call upload.resolve() to start uploading; await upload.promise to wait for a new upload
  let upPending = Promise.withResolvers<void>();
  const resetUpload = () => (upPending = Promise.withResolvers<void>());

  // note: intentionally a closure so it can use the latest value of upPending
  const global_up_fn = () => upPending.resolve();
  allClientUploads.add(global_up_fn);
  // remove from array on return
  using _disposer = new DisposableStack();
  _disposer.adopt(global_up_fn, () => allClientUploads.delete(global_up_fn));

  // we should upload if they client isn't going to
  const should_upload = (msg.flags & ClientConnect.FLAGS.INTENT_TO_UPLOAD) == 0;
  if (should_upload) upPending.resolve();

  const UPLOAD = {},
    DOWNLOAD = {},
    STOP = {};
  while (!stop.signal.aborted) {
    const which = await Promise.race([
      upPending.promise.then(() => UPLOAD),
      once(socket, "readable").then(() => DOWNLOAD),
      once(stop.signal, "aborted").then(() => STOP),
    ]);
    switch (which) {
      case UPLOAD:
        await updateList(directory);
        await upload(socket, global_list, directory);
        resetUpload();
        break;
      case DOWNLOAD:
        const changed = await download(socket, global_list, directory);
        // only need to update the list if we actually did something
        if (changed) {
          await updateList(directory);
          for (const fn of allClientUploads) {
            if (fn === global_up_fn) continue;
            fn();
          }
        }
        break;
      case STOP:
        break; // the next iteration will terminate the loop
      default:
        throw new Error("unreachable");
    }
  }
}

process.addListener("unhandledRejection", (reason, promise) => {
  console.error("unhandled rejection", reason, promise); // don't exit the process
});

process.exitCode = (await main()) ?? 0;
