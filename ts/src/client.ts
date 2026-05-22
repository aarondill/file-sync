import { once } from "node:events";
import fs from "node:fs/promises";
import { Socket } from "node:net";
import os from "node:os";
import { parseArgs } from "node:util";
import { download } from "./common/download.ts";
import { FileInfo } from "./common/FileInfo.ts";
import { ClientConnect } from "./common/messages.ts";
import { upload } from "./common/upload.ts";
import { access, writeMessage } from "./common/util.ts";

let global_list: FileInfo[] = [];
// No locking is required here, since the client is single-threaded
async function updateList(directory: string) {
  global_list = await FileInfo.readList(directory);
}

async function main(): Promise<number | undefined> {
  const { values, positionals } = parseArgs({
    args: process.argv.slice(2),
    options: {
      u: { type: "boolean", default: false },
    },
    allowNegative: true,
    allowPositionals: true,
  });
  const should_upload = values.u;
  const server = positionals[0],
    directory = positionals[1];
  if (!server || !directory) {
    console.error("usage: client.ts <server ip> <directory> [-u]");
    return 2;
  }

  if (!(await access(directory, fs.constants.R_OK | fs.constants.W_OK))) {
    console.error(`directory ${directory} is not readable or writable`);
    return 3;
  }
  const [host, portstr] = server.split(":");
  const port = parseInt(portstr ?? "8080");
  await using socket = new Socket().connect({ port, host });
  await once(socket, "connect");

  // send connect message
  await writeMessage(
    socket,
    new ClientConnect(
      os.hostname(),
      should_upload ? ClientConnect.FLAGS.INTENT_TO_UPLOAD : 0
    )
  );

  await updateList(directory);
  // call upload.resolve() to start uploading; await upload.promise to wait for a new upload
  let upPending = Promise.withResolvers<void>();
  const resetUpload = () => (upPending = Promise.withResolvers());

  if (should_upload) upPending.resolve();

  const stop = new AbortController();
  process.on("SIGTERM", () => stop.abort());
  process.on("SIGINT", () => stop.abort());
  process.on("SIGQUIT", () => stop.abort());

  const cmds: Record<string, { fn: () => void; desc: string }> = {
    q: { fn: () => stop.abort(), desc: "quit" },
    u: { fn: () => upPending.resolve(), desc: "upload" },
    h: {
      fn: () =>
        console.log(
          `commands: ${Object.entries(cmds)
            .map(([k, v]) => `${k}: ${v.desc}`)
            .join(", ")}`
        ),
      desc: "help",
    },
  };
  using _dispose = new DisposableStack();

  process.stdin.setEncoding("utf8").on("data", data => {
    const str = data.toString();
    for (const char of str) {
      if (!char.trim()) continue; // ignore whitespace
      const cmd = cmds[char];
      if (cmd) cmd.fn();
      else console.error(`unknown command: ${char}`);
    }
  });
  _dispose.defer(() => process.stdin.pause());

  const UPLOAD = "UPLOAD",
    DOWNLOAD = "DOWNLOAD",
    STOP = "STOP";
  while (!stop.signal.aborted) {
    const which = await Promise.race([
      upPending.promise.then(() => UPLOAD),
      once(socket, "readable").then(() => DOWNLOAD),
      once(stop.signal, "abort").then(() => STOP),
    ]);
    switch (which) {
      case UPLOAD:
        await updateList(directory);
        await upload(socket, global_list, directory);
        resetUpload();
        break;
      case DOWNLOAD: {
        if (!socket.readable) return; // socket closed
        await updateList(directory); // a precaution since files may have changed while waiting (warn if they did?)
        const changed = await download(socket, global_list, directory);
        // only need to update the list if we actually did something
        if (changed) await updateList(directory);
        break;
      }
      case STOP:
        return;
      default:
        throw new Error("unreachable");
    }
  }
}
process.addListener("unhandledRejection", (reason, promise) => {
  console.error("unhandled rejection", reason, promise); // don't exit the process
});

process.exitCode = (await main()) ?? 0;
