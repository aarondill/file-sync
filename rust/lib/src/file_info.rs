use std::{
    fs::File,
    path::{Path, PathBuf},
};

use walkdir::WalkDir;

use crate::file_hash::FileHash;

#[derive(PartialEq, Eq, Clone, Debug)]
pub struct FileInfo {
    // A relative path from the base directory
    path: PathBuf,
    hash: FileHash,
    size: u64,
}
impl FileInfo {
    pub fn path(&self) -> &Path {
        &self.path
    }

    pub fn hash(&self) -> &FileHash {
        &self.hash
    }

    pub fn size(&self) -> u64 {
        self.size
    }

    pub fn new(path: PathBuf, hash: FileHash, size: u64) -> FileInfo {
        FileInfo { path, hash, size }
    }
    pub fn new_relative(
        path: PathBuf,
        base: &Path,
    ) -> Result<FileInfo, Box<dyn std::error::Error>> {
        let abs_path = base.join(path);
        let size = abs_path.metadata()?.len();
        let mut file = File::open(&abs_path)?;
        let hash = FileHash::new(&mut file)?;

        let path = abs_path.strip_prefix(base)?.to_path_buf();
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

#[cfg(test)]
mod tests {
    use super::*;
    use std::{fs::File, io::Write};
    use tempdir::TempDir;

    fn setup() -> TempDir {
        let tmpdir = TempDir::new("test_file_info").unwrap();
        let dir = tmpdir.path();

        let path = dir.join("test.txt");
        let mut file = File::create(&path).unwrap();
        file.write_all(b"hello world").unwrap(); // 5eb63bbbe01eeed093cb22bb8f5acdc3
        drop(file);

        std::fs::create_dir_all(dir.join("subdir")).unwrap();
        let path = dir.join("subdir/test2.txt");
        let mut file = File::create(&path).unwrap();
        file.write_all(b"goodbye world").unwrap(); // 0949f7eb1f66dad39d488d5d22531166
        drop(file);

        tmpdir
    }

    #[test]
    fn test_new_relative() {
        let base = setup();

        let info = FileInfo::new_relative("test.txt".into(), base.path()).expect("new_relative");

        let full_path = base.path().join("test.txt");
        let expected = FileInfo::new(
            full_path,
            "5eb63bbbe01eeed093cb22bb8f5acdc3".parse().unwrap(),
            11,
        );
        assert_eq!(info, expected);

        base.close().unwrap();
    }
    #[test]
    fn test_new_relative_fail() {
        let base = setup();
        let info = FileInfo::new_relative("../parent-path/test.txt".into(), base.path());
        assert!(info.is_err());
        base.close().unwrap();
    }
    #[test]
    fn test_read_list() {
        let base = setup();
        let infos = FileInfo::read_list(base.path());
        let expected = vec![
            FileInfo::new(
                "test.txt".into(),
                "5eb63bbbe01eeed093cb22bb8f5acdc3".parse().unwrap(),
                11,
            ),
            FileInfo::new(
                "subdir/test2.txt".into(),
                "0949f7eb1f66dad39d488d5d22531166".parse().unwrap(),
                13,
            ),
        ];
        assert_eq!(infos, expected);
        base.close().unwrap();
    }
}
