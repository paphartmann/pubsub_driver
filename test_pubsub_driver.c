#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_LENGTH 256

static int write_command(int fd, const char *command)
{
    size_t len = strlen(command);
    ssize_t written = write(fd, command, len);

    if (written < 0 || (size_t)written != len) {
        perror("write failed");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int fd;
    char receive[BUFFER_LENGTH];
    char stringToSend[BUFFER_LENGTH];
    int i;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <topic> [<topic> ...]\n", argv[0]);
        return 1;
    }

    fd = open("/dev/pubsub", O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device...");
        return errno;
    }

    for (i = 1; i < argc; i++) {
        snprintf(stringToSend, sizeof(stringToSend), "/subscribe %s", argv[i]);
        if (write_command(fd, stringToSend) != 0) {
            close(fd);
            return 1;
        }
    }

    pid_t pid = fork();
    if (pid == 0) {
        for (i = 1; i < argc; i++) {
            snprintf(stringToSend, sizeof(stringToSend),
                     "/publish %s \"Hello from %d to topic %s\"",
                     argv[i], getpid(), argv[i]);
            if (write_command(fd, stringToSend) != 0) {
                close(fd);
                _exit(1);
            }
        }
        close(fd);
        _exit(0);
    }

    waitpid(pid, NULL, 0);

    for (i = 1; i < argc; i++) {
        ssize_t bytes_read;

        snprintf(stringToSend, sizeof(stringToSend), "/fetch %s", argv[i]);
        if (write_command(fd, stringToSend) != 0) {
            close(fd);
            return 1;
        }

        memset(receive, 0, sizeof(receive));
        bytes_read = read(fd, receive, sizeof(receive) - 1);
        if (bytes_read < 0) {
            perror("read failed");
            close(fd);
            return 1;
        }
        if (bytes_read == 0) {
            fprintf(stderr, "No message received for topic %s\n", argv[i]);
            close(fd);
            return 1;
        }
        receive[bytes_read] = '\0';
        printf("USER: %d received %s from %s\n", getpid(), receive, argv[i]);
    }

    for (i = 1; i < argc; i++) {
        snprintf(stringToSend, sizeof(stringToSend), "/unsubscribe %s", argv[i]);
        if (write_command(fd, stringToSend) != 0) {
            close(fd);
            return 1;
        }
    }

    close(fd);
    return 0;
}
