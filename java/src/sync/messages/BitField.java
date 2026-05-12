package sync.messages;

interface Bitfield {
  int ordinal();
  default int bitval() {
    return 1 << this.ordinal();
  }
}
