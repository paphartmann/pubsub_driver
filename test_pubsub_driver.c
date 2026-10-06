#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_LENGTH 256
#define CONCURRENT_WORKERS 4
#define CONCURRENT_ITERATIONS 100

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

static int run_concurrent_worker(const char *topic, int start_fd)
{
    char command[BUFFER_LENGTH];
    char receive[BUFFER_LENGTH];
    char token;
    int fd;
    int i;

    fd = open("/dev/pubsub", O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device in worker");
        return 1;
    }

    if (read(start_fd, &token, sizeof(token)) != sizeof(token)) {
        fprintf(stderr, "Failed to receive concurrent-test start signal\n");
        close(fd);
        return 1;
    }
    close(start_fd);

    snprintf(command, sizeof(command), "/subscribe %s", topic);
    if (write_command(fd, command) != 0)
        goto fail;
    snprintf(command, sizeof(command), "/fetch %s", topic);
    if (write_command(fd, command) != 0)
        goto fail;

    for (i = 0; i < CONCURRENT_ITERATIONS; i++) {
        ssize_t bytes_read;

        snprintf(command, sizeof(command), "/publish %s \"worker-%d-%d\"",
                 topic, getpid(), i);
        if (write_command(fd, command) != 0)
            goto fail;

        bytes_read = read(fd, receive, sizeof(receive) - 1);
        if (bytes_read <= 0) {
            if (bytes_read < 0)
                perror("Concurrent worker read failed");
            else
                fprintf(stderr, "Concurrent worker received no message\n");
            goto fail;
        }
    }

    snprintf(command, sizeof(command), "/unsubscribe %s", topic);
    if (write_command(fd, command) != 0)
        goto fail;
    if (close(fd) != 0) {
        perror("Failed to close device in worker");
        return 1;
    }
    return 0;

fail:
    close(fd);
    return 1;
}

static int test_concurrent_processes(void)
{
    int start_pipe[2];
    pid_t workers[CONCURRENT_WORKERS];
    char topic[64];
    int spawned = 0;
    int failed = 0;
    int i;

    snprintf(topic, sizeof(topic), "concurrent-%d", getpid());
    if (pipe(start_pipe) != 0) {
        perror("Failed to create concurrent-test pipe");
        return 1;
    }

    for (i = 0; i < CONCURRENT_WORKERS; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("Failed to fork concurrent worker");
            failed = 1;
            break;
        }
        if (pid == 0) {
            int worker_failed;

            close(start_pipe[1]);
            worker_failed = run_concurrent_worker(topic, start_pipe[0]);
            _exit(worker_failed);
        }
        workers[spawned++] = pid;
    }

    close(start_pipe[0]);
    for (i = 0; i < spawned; i++) {
        char token = 'S';

        if (write(start_pipe[1], &token, sizeof(token)) != sizeof(token)) {
            perror("Failed to release concurrent worker");
            failed = 1;
            break;
        }
    }
    close(start_pipe[1]);

    for (i = 0; i < spawned; i++) {
        int status;

        if (waitpid(workers[i], &status, 0) < 0) {
            perror("Failed to wait for concurrent worker");
            failed = 1;
        } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Concurrent worker %d failed\n", (int)workers[i]);
            failed = 1;
        }
    }

    if (spawned != CONCURRENT_WORKERS)
        failed = 1;
    if (!failed)
        puts("Concurrent device operations passed");
    return failed;
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

    if (test_concurrent_processes() != 0) {
        close(fd);
        return 1;
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
