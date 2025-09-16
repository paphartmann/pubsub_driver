#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_LENGTH 256

int main()
{
	int ret, fd, len;
	char receive[BUFFER_LENGTH];
	char stringToSend[BUFFER_LENGTH];

	printf("Starting device test code example...\n");

	fd = open("/dev/pubsub", O_RDWR);
	if (fd < 0) {
		perror("Failed to open the device...");
		return errno;
	}

	while (1) {
	}

	return 0;
}
