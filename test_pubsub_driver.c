#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

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

	printf("Starting device test code example...\n");

	fd = open("/dev/pubsub", O_RDWR);
	if (fd < 0) {
		perror("Failed to open the device...");
		return errno;
	}

	for (int _ = 0; _ < 10; _++) {
		const char *publish_str = "/publish \"Hello from %d\"\n";
		len = sprintf(stringToSend, publish_str, getpid());
		printf("Process %d publishing \"%s\" in %s\n", getpid(), stringToSend, argv[1]);
		write(fd, stringToSend, len);
		
		len = sprintf(stringToSend, "/fetch %s\n", argv[1]);
		write(fd, stringToSend, len);
		read(fd, receive, BUFFER_LENGTH);
		printf("Process %d received %s from topic %s\n", getpid(), receive, argv[1]);

		sleep(3);
	}

	return 0;
}
