#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int main() {
  int fd = open("/dev/pubsub", O_RDWR);

  while (1) {
    char line[255];
    fgets(line, 255, stdin);
    write(fd, line, 255);
    if (strstr(line, "/fetch")) {
      read(fd, line, 255);
      puts(line);
    }
  }
}
