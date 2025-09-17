#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#define BUFFER_LENGTH 256

int main(int argc, char **argv)
{
	if (argc != 2) {
		puts("program has to be run with ./test_pubsub_driver topic_name");
		return -1;
	}
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
	len = sprintf(stringToSend, "/subscribe %s\n", argv[1]);
	write(fd, stringToSend, len);
	for (int _ = 0; _ < 10; _++) {
		const char *publish_str = "/publish %s \"Hello from %d\"\n";
		len = sprintf(stringToSend, publish_str, argv[1], getpid());
		printf("USER: Process %d publishing \"%s\"\n", getpid(), stringToSend);
		write(fd, stringToSend, len);

		sleep(2 + random() % 3);

		len = sprintf(stringToSend, "/fetch %s\n", argv[1]);
		write(fd, stringToSend, len);
		read(fd, receive, BUFFER_LENGTH);
		printf("USER: Process %d received \"%s\" from topic %s\n", getpid(), receive, argv[1]);

		sleep(2 + sleep(random() % 3));
	}

	sprintf(stringToSend, "/unsubscribe %s\n", argv[1]);

	return 0;
}
