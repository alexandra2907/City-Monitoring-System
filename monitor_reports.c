#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

void handle_sigusr1(int sig) {
    char *msg = "Monitor: New report has been added!\n";
    write(STDOUT_FILENO, msg, strlen(msg));
}

void handle_sigint(int sig) {
    char *msg = "\nMonitor: Shutting down gracefully...\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    unlink(".monitor_pid");
    exit(0);
}

int main() {
    struct sigaction sa_usr1, sa_int;

    memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_usr1.sa_handler = handle_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    sigaction(SIGUSR1, &sa_usr1, NULL);

    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    int fd = open(".monitor_pid", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Error creating .monitor_pid");
        return 1;
    }
    char pid_str[16];
    int len = snprintf(pid_str, sizeof(pid_str), "%d", getpid());
    write(fd, pid_str, len);
    close(fd);

    char *msg = "Monitor started. Waiting for signals... (Press Ctrl+C to stop)\n";
    write(STDOUT_FILENO, msg, strlen(msg));

    while (1) {
        pause(); 
    }

    return 0;
}