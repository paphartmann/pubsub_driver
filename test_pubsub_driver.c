#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#define BUFFER_LENGTH 256

int main(int argc, char **argv)
{
	int fd, len;
	char receive[BUFFER_LENGTH];
	char stringToSend[BUFFER_LENGTH];

	printf("USER: Starting device test code example...\n");

	fd = open("/dev/pubsub", O_RDWR);
	if (fd < 0) {
		perror("Failed to open the device...");
		return errno;
	}

	srandom(getpid());
	for (int i = 1; i < argc; i++) {
		len = sprintf(stringToSend, "/subscribe %s\n", argv[i]);
		write(fd, stringToSend, len);
	}
	for (int _ = 0; _ < 10; _++) {
		const char *publish_str = "/publish %s \"Hello from %d\"\n";
		for (int i = 1; i < argc; i++) {
			len = sprintf(stringToSend, publish_str, argv[i], getpid());
			printf("USER: Process %d publishing \"%s\" to topic %s\n", getpid(), stringToSend, argv[i]);
			write(fd, stringToSend, len);
		}
		sleep(2 + random() % 3);

		for (int i = 1; i < argc; i++) {
			len = sprintf(stringToSend, "/fetch %s\n", argv[i]);
			write(fd, stringToSend, len);
			read(fd, receive, BUFFER_LENGTH);
			printf("USER: Process %d received \"%s\" from topic %s\n", getpid(), receive, argv[i]);
		}

		sleep(2 + sleep(random() % 3));
	}

	for (int i = 1; i < argc; i++) {
		len = sprintf(stringToSend, "/unsubscribe %s\n", argv[i]);
		write(fd, stringToSend, len);
	}

	return 0;
}
