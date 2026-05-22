import assert from "node:assert";
import { once } from "node:events";
import type { Readable, Writable } from "node:stream";
import type { Serializable } from "./messages.ts";

async function writeBytes(out: Writable, bytes: Uint8Array): Promise<void> {
  if (!out.write(bytes)) await once(out, "drain");
}
export async function writeMessage(out: Writable, message: Serializable) {
  const buf = message.serialize();
  // write buf.length to out
  const lenbuf = Buffer.alloc(2);
  lenbuf.writeUInt16BE(buf.length);
  await writeBytes(out, lenbuf);
  // write buf to out
  await writeBytes(out, buf);
}

async function readBytes(input: Readable, bytes: number): Promise<Buffer> {
  assert(input.readable);
  let buf = Buffer.alloc(bytes);
  let got = 0;
  while (got < bytes) {
    await once(input, "readable");
    const toRead = Math.min(bytes - got, input.readableLength);
    const read = input.read(toRead) as unknown;
    if (read === null)
      throw new Error(`unexpected EOF after reading ${got} of ${bytes} bytes`);
    assert(read instanceof Buffer);
    buf.set(read, got);
    got += read.length;
  }
  return buf;
}

// note: input must not have an encoding set and must be paused
export async function readMessage(input: Readable) {
  // read buf.length from input
  const bytesBuf = await readBytes(input, 2);
  const bytes = bytesBuf.readUInt16BE(0);
  // read buf from input
  return await readBytes(input, bytes);
}
