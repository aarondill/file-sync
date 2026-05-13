import java.nio.file.Path;
import sync.Sync;

public class Client extends Sync {
  Client(Path path) {
    super(path);
  }
  public static void main(String[] args) {
    System.out.println("Hello World: Client!");
  }
}
