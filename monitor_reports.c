#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

volatile sig_atomic_t keep_running = 1;

void handle_sigusr1(int sig) {
    char *msg = "MON_UPDATE: New report added!\n";
    write(STDOUT_FILENO, msg, strlen(msg));
}

void handle_sigint(int sig) {
    keep_running = 0;
}

int main() {
    // PASUL CRITIC: Verificam daca exista deja un monitor
    int fd_check = open(".monitor_pid", O_RDONLY);
    if (fd_check >= 0) {
        char old_pid_str[16];
        int br = read(fd_check, old_pid_str, sizeof(old_pid_str) - 1);
        close(fd_check);
        if (br > 0) {
            old_pid_str[br] = '\0';
            // Trimitem eroarea prin pipe (stdout) inainte sa iesim
            printf("MON_ERROR: Another monitor is already running with PID %s\n", old_pid_str);
            fflush(stdout);
            return 1; // Iesim cu eroare
        }
    }

    // Inregistram PID-ul nostru
    int fd_pid = open(".monitor_pid", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_pid >= 0) {
        char pid_str[16];
        int len = snprintf(pid_str, sizeof(pid_str), "%d", getpid());
        write(fd_pid, pid_str, len);
        close(fd_pid);
    }

    struct sigaction sa_usr1, sa_int;
    memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_usr1.sa_handler = handle_sigusr1;
    sa_usr1.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa_usr1, NULL);

    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa_int, NULL);

    printf("MON_START: Monitor active with PID %d\n", getpid());
    fflush(stdout);

    while (keep_running) {
        pause();
    }

    unlink(".monitor_pid");
    printf("MON_END: Shutdown successful\n");
    return 0;
}