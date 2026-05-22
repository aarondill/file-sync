import { createReadStream } from "node:fs";
import path from "node:path";
import type { Duplex } from "node:stream";
import type { FileInfo } from "./FileInfo.ts";
import { Download, DownloadFile, DownloadResponse } from "./messages.ts";
import { readMessage, transferBytes, writeMessage } from "./util.ts";
export async function upload(io: Duplex, files: FileInfo[], directory: string) {
  // send download message 1
  await writeMessage(io, new Download(files.length));
  for (const file of files)
    await writeMessage(io, new DownloadFile(file.hash, file.size, file.name));

  // receive download response
  const [resp] = DownloadResponse.deserialize(await readMessage(io));
  const filtered_list: FileInfo[] = resp.hashes
    .map(hash => files.find(f => f.hash.equals(hash)))
    .filter(f => f !== undefined); // exclude any files that we didn't send

  // send download message 2
  await writeMessage(io, new Download(filtered_list.length));
  for (const file of filtered_list)
    await writeMessage(io, new DownloadFile(file.hash, file.size, file.name));

  // send file contents
  for (const file of filtered_list) {
    const p = path.join(directory, file.name);
    const stream = createReadStream(p);
    console.log(`sending: ${p}: ${file.hash}`);
    await transferBytes(stream, io, file.size);
    if (stream.read() !== null) throw new Error("file too large");
    stream.close();
  }
}
