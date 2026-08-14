use core::io;
use std::{
    fs::{self, Dir, DirEntry, File},
    path::{Path, PathBuf},
};

use walkdir::WalkDir;

use crate::file_hash::FileHash;

struct FileInfo {
    // A relative path from the base directory
    path: PathBuf,
    hash: FileHash,
    size: u64,
}
impl FileInfo {
    // explicit FileInfo(const fs::path &path, const fs::path &base)
    //     : path{path.lexically_relative(base)}, hash{path}, size{file_size(path)} {}
    pub fn new(path: PathBuf, hash: FileHash, size: u64) -> FileInfo {
        FileInfo { path, hash, size }
    }
    pub fn new_relative(
        path: PathBuf,
        base: &Path,
    ) -> Result<FileInfo, Box<dyn std::error::Error>> {
        let path = base.join(path);
        let size = path.metadata()?.len();
        let mut file = File::open(path)?;
        let hash = FileHash::new(&mut file)?;
        Ok(FileInfo { path, hash, size })
    }
    pub fn read_list(dir: &Path) -> Vec<FileInfo> {
        WalkDir::new(dir)
            .sort_by_file_name()
            .into_iter()
            .filter_map(|e| e.ok()) // ignore errors
            .filter(|e| !e.file_type().is_dir())
            .map(|entry| {
                let path = entry.path();
                let size = path.metadata().unwrap().len();
                let mut file = File::open(path).unwrap();
                let hash = FileHash::new(&mut file).unwrap();

                let path = path.strip_prefix(dir).unwrap().to_path_buf();
                FileInfo { path, hash, size }
            })
            .collect::<Vec<FileInfo>>()
    }
}
