package sync;

import java.io.DataOutputStream;
import java.io.IOException;

public interface Serialize {
  void serialize(DataOutputStream out) throws IOException;
}
