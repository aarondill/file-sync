// TODO: use serde eventually?

use std::io::{Read, Write};

pub trait Deserialize {
    fn deserialize(reader: &mut dyn Read) -> Result<Self, Box<dyn std::error::Error>>
    where
        Self: Sized;
}

pub trait Serialize {
    fn serialize(&self, writer: &mut dyn Write) -> Result<(), Box<dyn std::error::Error>>;
}
