import assert from "node:assert";
import { createWriteStream } from "node:fs";
import fs from "node:fs/promises";
import path from "node:path";
import type { Duplex, Readable } from "node:stream";
import { FileInfo } from "./FileInfo.ts";
import { Download, DownloadFile, DownloadResponse } from "./messages.ts";
import { readMessage, transferBytes, writeMessage } from "./util.ts";
// `rmdir -p`; remove a directory and all its parents
const rmdir_p = async (dir: string, stop_at?: string) => {
  if (stop_at) {
    stop_at = path.normalize(stop_at);
    if (dir == stop_at) return;
  }
  if (!dir) return;
  const err = await fs.rmdir(dir).catch(e => e);
  if (err) {
    if (err.code === "ENOTEMPTY") return;
    throw err;
  }
  return rmdir_p(path.dirname(dir));
};

async function readFileList(
  input: Readable,
  count: number
): Promise<FileInfo[]> {
  const ret: FileInfo[] = [];
  for (let i = 0; i < count; i++) {
    const [file] = DownloadFile.deserialize(await readMessage(input));
    ret.push(new FileInfo(file.name, file.hash, file.size));
  }
  return ret;
}

export async function download(
  io: Duplex,
  files: FileInfo[],
  directory: string
) {
  //  read the download message
  const [message] = Download.deserialize(await readMessage(io));
  const recvlist: FileInfo[] = await readFileList(io, message.fileCount);

  // delete any files that the server didn't send us, but wait until the end to do this
  const to_delete = files.filter(f => recvlist.every(o => o.name !== f.name));
  // write the download response
  // We only want files that we don't already have (either no path, or a different hash)
  const filtered = recvlist.filter(f =>
    files.every(o => o.name !== f.name || !o.hash.equals(f.hash))
  );
  await writeMessage(io, new DownloadResponse(filtered.map(f => f.hash)));

  // read download message 2
  const [download2] = Download.deserialize(await readMessage(io));
  const ret: FileInfo[] = await readFileList(io, download2.fileCount);
  // recv/write the file contents
  for (const file of ret) {
    const p = path.join(directory, file.name);
    await fs.mkdir(path.dirname(p), { recursive: true });
    const fileStream = createWriteStream(p);
    await transferBytes(io, fileStream, file.size);
    fileStream.close();
    assert(await file.hash.verify(p), `hash mismatch: ${file.name}`);
  }

  // delete files that we don't need anymore
  for (const file of to_delete) {
    const p = path.join(directory, file.name);
    console.log("deleting: " + p);
    await fs.rm(p);

    await rmdir_p(path.dirname(p), directory).catch(e => console.error(e));
  }
  // return true if we actually did something
  return ret.length > 0 || to_delete.length > 0;
}
