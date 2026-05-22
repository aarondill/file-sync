import assert from "node:assert";
import { Buffer } from "node:buffer";
import { FileHash } from "./FileHash.ts";
export interface Serializable {
  serialize(): Buffer;
}

export class ClientConnect implements Serializable {
  static PROTOCOL_VERSION = 2;
  static FLAGS = {
    INTENT_TO_UPLOAD: 1 << 0,
  };
  protocolVersion: number = ClientConnect.PROTOCOL_VERSION;
  flags: number;
  clientName: string;
  constructor(clientName: string, flags: number = 0) {
    this.clientName = clientName;
    this.flags = flags;
  }
  static deserialize(buffer: Buffer, offset = 0): [ClientConnect, number] {
    const protocolVersion = buffer.readUint8(offset);
    offset += 1;
    assert.equal(
      protocolVersion,
      ClientConnect.PROTOCOL_VERSION,
      "protocol version mismatch"
    );
    const flags = buffer.readUint8(offset);
    offset += 1;
    const clientNameLength = buffer.readUint8(offset);
    offset += 1;
    const clientName = buffer.toString(
      "utf8",
      offset,
      offset + clientNameLength
    );
    offset += clientNameLength;
    return [new ClientConnect(clientName, flags), offset];
  }
  serialize(): Buffer {
    const buffer = Buffer.alloc(3 + this.clientName.length);
    assert.equal(
      this.protocolVersion,
      ClientConnect.PROTOCOL_VERSION,
      "protocol version mismatch"
    );
    buffer.writeUint8(this.protocolVersion, 0);
    buffer.writeUint8(this.flags, 1);
    assert(this.clientName.length <= 255, "client name too long");
    buffer.writeUint8(this.clientName.length, 2);
    buffer.write(this.clientName, 3, "utf8");
    return buffer;
  }
}

// NOTE: Download is followed by fileCount DownloadFile messages
export class Download implements Serializable {
  static FLAGS = {
    ERROR: 1 << 0,
  };
  flags: number;
  fileCount: number;
  constructor(fileCount: number, flags = 0) {
    this.fileCount = fileCount;
    this.flags = flags;
  }
  static deserialize(buffer: Buffer, offset = 0): [Download, number] {
    const flags = buffer.readUint8(offset);
    offset += 1;
    if (flags & Download.FLAGS.ERROR)
      throw new Error("Attempt to deserialize error message");
    const fileCount = buffer.readUint8(offset);
    offset += 1;
    return [new Download(fileCount, flags), offset];
  }
  serialize(): Buffer {
    assert.equal(this.flags & Download.FLAGS.ERROR, 0, "error flag set");
    const buffer = Buffer.alloc(2);
    buffer.writeUint8(this.flags, 0);
    buffer.writeUint8(this.fileCount, 1);
    return buffer;
  }
}

export class DownloadFile implements Serializable {
  hash: FileHash;
  size: number;
  name: string;
  constructor(hash: FileHash, size: number, name: string) {
    this.hash = hash;
    this.size = size;
    this.name = name;
  }
  static deserialize(buffer: Buffer, offset = 0): [DownloadFile, number] {
    let hash: FileHash;
    [hash, offset] = FileHash.deserialize(buffer, offset);
    const size = buffer.readUint32BE(offset);
    offset += 4;
    const nameLength = buffer.readUint8(offset);
    offset += 1;
    const name = buffer.toString("utf8", offset, offset + nameLength);
    offset += nameLength;
    return [new DownloadFile(hash, size, name), offset];
  }
  serialize(): Buffer {
    const hbuffer = this.hash.serialize();

    const buffer = Buffer.alloc(4 + 1 + this.name.length);
    buffer.writeUint32BE(this.size, 0);
    assert(this.name.length <= 255, "name too long");
    buffer.writeUint8(this.name.length, 4);
    buffer.write(this.name, 5, "utf8");

    return Buffer.concat([hbuffer, buffer]);
  }
}

export class DownloadResponse implements Serializable {
  static FLAGS = {
    ERROR: 1 << 0,
  };
  flags: number;
  hashes: FileHash[];
  constructor(hashes: FileHash[], flags = 0) {
    this.hashes = hashes;
    this.flags = flags;
  }
  static deserialize(buffer: Buffer, offset = 0): [DownloadResponse, number] {
    const flags = buffer.readUint8(offset);
    offset += 1;
    if (flags & DownloadResponse.FLAGS.ERROR)
      throw new Error("Attempt to deserialize error message");
    const fileCount = buffer.readUint8(offset);
    offset += 1;
    const hashes: FileHash[] = [];
    for (let i = 0; i < fileCount; i++) {
      let hash;
      [hash, offset] = FileHash.deserialize(buffer);
      hashes.push(hash);
    }
    return [new DownloadResponse(hashes, flags), offset];
  }
  serialize(): Buffer {
    const buffer = Buffer.alloc(2);
    assert.equal(
      this.flags & DownloadResponse.FLAGS.ERROR,
      0,
      "error flag set"
    );
    buffer.writeUint8(this.flags, 0);
    assert(this.hashes.length <= 255, "file count too large");
    buffer.writeUint8(this.hashes.length, 1);
    return Buffer.concat([
      buffer,
      ...this.hashes.map(hash => hash.serialize()),
    ]);
  }
}

export class ErrorMessage implements Serializable {
  static FLAGS = {
    ERROR: 1 << 0,
  };
  flags: number;
  errorCode: number;
  errorMessage: string;
  constructor(errorCode: number, errorMessage: string, flags = 0) {
    this.errorCode = errorCode;
    this.errorMessage = errorMessage;
    this.flags = flags;
  }
  static deserialize(buffer: Buffer, offset = 0): [ErrorMessage, number] {
    const flags = buffer.readUint8(offset);
    offset += 1;
    assert(flags & ErrorMessage.FLAGS.ERROR, "error flag not set");
    const errorCode = buffer.readUint8(offset);
    offset += 1;
    const errorMessageLength = buffer.readUint8(offset);
    offset += 1;
    const errorMessage = buffer.toString(
      "utf8",
      offset,
      offset + errorMessageLength
    );
    offset += errorMessageLength;
    return [new ErrorMessage(errorCode, errorMessage, flags), offset];
  }
  serialize(): Buffer {
    const buffer = Buffer.alloc(2 + 1 + this.errorMessage.length);
    assert(this.flags & ErrorMessage.FLAGS.ERROR, "error flag not set");
    buffer.writeUint8(this.flags, 0);
    buffer.writeUint8(this.errorCode, 1);
    assert(this.errorMessage.length <= 255, "error message too long");
    buffer.writeUint8(this.errorMessage.length, 2);
    buffer.write(this.errorMessage, 3, "utf8");
    return buffer;
  }
}
