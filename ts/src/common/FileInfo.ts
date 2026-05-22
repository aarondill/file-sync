import fs from "fs/promises";
import path from "path";
import { FileHash } from "./FileHash.ts";

export class FileInfo {
  name: string; // a relative path, relative to the base directory
  hash: FileHash;
  size: number;

  constructor(name: string, hash: FileHash, size: number) {
    this.name = name;
    this.hash = hash;
    this.size = size;
  }
  static async fromPath(file: string, base: string): Promise<FileInfo> {
    const name = path.relative(base, file);
    const [hash, size] = await Promise.all([
      FileHash.fromPath(file),
      fs.stat(file).then(s => s.size),
    ]);
    return new FileInfo(name, hash, size);
  }
  // Reads the directory and returns a list of all files in the directory
  static async readList(dirpath: string): Promise<FileInfo[]> {
    const files: Promise<FileInfo>[] = [];
    for await (const dirent of await fs.opendir(dirpath, { recursive: true })) {
      if (!dirent.isFile()) continue;
      const name = path.join(dirpath, dirent.name);
      files.push(FileInfo.fromPath(name, dirpath));
    }
    return Promise.all(files);
  }
}
