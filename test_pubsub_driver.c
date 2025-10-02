#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#define BUFFER_LENGTH 256

int main(int argc, char **argv)
{
	int fd, len;
	char receive[BUFFER_LENGTH];
	char stringToSend[BUFFER_LENGTH];

	fd = open("/dev/pubsub", O_RDWR);
	if (fd < 0) {
		perror("Failed to open the device...");
		return errno;
	}

	for (int i = 1; i < argc; i++) {
		len = sprintf(stringToSend, "/subscribe %s", argv[i]);
		write(fd, stringToSend, len + 1);
	}

	pid_t pid = fork();
	if (pid == 0) {
		for (int i = 1; i < argc; i++) {
			len = sprintf(stringToSend, "/publish %s \"Hello from %d to topic %s\"", argv[i], getpid(), argv[i]);
			write(fd, stringToSend, len + 1);
		}
		close(fd);
		return 0;
	} else {
		
		wait(NULL);
		for (int i = 1; i < argc; i++) {
			len = sprintf(stringToSend, "/fetch %s", argv[i]);
			write(fd, stringToSend, len + 1);
			read(fd, receive, BUFFER_LENGTH);
			printf("USER: %d received %s from %s\n", getpid(), receive, argv[i]);
		}
	}

	for (int i = 1; i < argc; i++) {
		len = sprintf(stringToSend, "/unsubscribe %s", argv[i]);
		write(fd, stringToSend, len + 1);
	}

	close(fd);

	return 0;
}
