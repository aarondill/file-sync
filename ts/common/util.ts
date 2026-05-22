import type { Readable, Writable } from "node:stream";
import type { Serializable } from "./messages.ts";

export async function writeMessage(out: Writable, message: Serializable) {
  const buf = message.serialize();
  // TODO: write buf.length to out
  // TODO: write buf to out
  throw new Error("not implemented"); // TODO:
}
export async function readMessage(input: Readable, signal: AbortSignal) {
  // TODO: read length from input
  // TODO: read buf from input
  throw new Error("not implemented"); // TODO:
}
