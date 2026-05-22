import assert from "node:assert";
import { createHash } from "node:crypto";
import { once } from "node:events";
import { createReadStream, type PathLike } from "node:fs";
import { inspect } from "node:util";
import type { Serializable } from "./messages.ts";

export class FileHash implements Serializable {
  // a file hash is a 16 byte md5 hash
  hash: Uint8Array;
  constructor(hash: Uint8Array) {
    assert.equal(hash.length, 16);
    this.hash = Uint8Array.from(hash); // copy
  }
  static async fromPath(path: PathLike) {
    const hasher = createHash("md5");
    const s = createReadStream(path).on("data", chunk => hasher.update(chunk));
    await once(s, "end");
    return new FileHash(hasher.digest());
  }
  serialize(): Buffer {
    return Buffer.copyBytesFrom(this.hash);
  }
  static deserialize(buffer: Buffer, offset = 0): [FileHash, number] {
    const slice = buffer.subarray(offset, offset + 16);
    return [new FileHash(slice), offset + 16];
  }
  async verify(path: PathLike) {
    return this.equals(await FileHash.fromPath(path));
  }
  equals(other: FileHash) {
    return Buffer.compare(this.hash, other.hash) === 0;
  }
  toString() {
    return Buffer.from(this.hash).toString("hex");
  }
  [inspect.custom] = () => {
    return this.toString();
  };
}
