use std::path::{Path, PathBuf};

use futures::{Stream, StreamExt};
use tokio::fs;
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

    pub async fn new_relative(
        path: PathBuf,
        base: &Path,
    ) -> Result<FileInfo, Box<dyn std::error::Error>> {
        let path = path.strip_prefix(base).unwrap_or(&path).to_path_buf();
        // path must be relative or a subpath of base
        if !path.is_relative() {
            return Err("path is not relative".into());
        }
        let abs_path = base.join(&path);
        let size = fs::metadata(&abs_path).await?.len();
        let mut file = fs::File::open(&abs_path).await?;
        let hash = FileHash::new(&mut file).await?;
        Ok(FileInfo { path, hash, size })
    }

    pub fn read_list(dir: &Path) -> impl Stream<Item = FileInfo> {
        let iter = WalkDir::new(dir)
            .sort_by_file_name()
            .into_iter()
            .filter_map(Result::ok) // ignore errors
            .filter(|e| !e.file_type().is_dir());
        futures::stream::iter(iter)
            .then(async move |entry| FileInfo::new_relative(entry.path().to_path_buf(), dir).await)
            .map(Result::unwrap) // they're all relative already
    }
}

#[cfg(test)]
mod tests {
    use std::fs::File;
    use std::io::Write;

    use futures::StreamExt;
    use tempdir::TempDir;

    use super::*;

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

    #[tokio::test]
    async fn test_new_relative() {
        let base = setup();

        let info =
            FileInfo::new_relative("test.txt".into(), base.path()).await.expect("new_relative");

        let full_path = base.path().join("test.txt");
        let expected =
            FileInfo::new(full_path, "5eb63bbbe01eeed093cb22bb8f5acdc3".parse().unwrap(), 11);
        assert_eq!(info, expected);

        base.close().unwrap();
    }
    #[tokio::test]
    async fn test_new_relative_fail() {
        let base = setup();
        let info = FileInfo::new_relative("../parent-path/test.txt".into(), base.path()).await;
        assert!(info.is_err());
        base.close().unwrap();
    }
    #[tokio::test]
    async fn test_read_list() {
        let base = setup();
        let infos: Vec<_> = FileInfo::read_list(base.path()).collect().await;
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
