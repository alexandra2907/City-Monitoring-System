#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

void start_monitor() {
    int p[2];
    pipe(p);

    if (fork() == 0) { // hub_mon
        close(p[0]);
        dup2(p[1], STDOUT_FILENO); // Tot ce zice monitorul merge în pipe
        close(p[1]);
        execlp("./monitor_reports", "./monitor_reports", NULL);
        exit(1);
    }

    close(p[1]);
    if (fork() == 0) { // Proces care citește din pipe în background
        char buf[256];
        int br;
        while ((br = read(p[0], buf, sizeof(buf)-1)) > 0) {
            buf[br] = '\0';
            if (strstr(buf, "MON_UPDATE")) printf("\n[HUB] Activity detected: %s", buf + 11);
            else if (strstr(buf, "MON_ERROR")) printf("\n[HUB] Fatal: %s", buf + 10);
            else if (strstr(buf, "MON_END")) { printf("\n[HUB] Monitor has stopped.\n"); break; }
            else printf("\n[MON] %s", buf);
            printf("hub> "); fflush(stdout);
        }
        close(p[0]);
        exit(0);
    }
    close(p[0]);
}

void calculate_scores(int argc, char *argv[]) {
    for (int i = 2; i < argc; i++) {
        int p[2];
        pipe(p);
        if (fork() == 0) {
            close(p[0]);
            dup2(p[1], STDOUT_FILENO); // Redirect scorer output to pipe
            close(p[1]);
            execlp("./scorer", "./scorer", argv[i], NULL);
            exit(1);
        }
        close(p[1]);
        char buf[512];
        int br = read(p[0], buf, sizeof(buf)-1);
        if (br > 0) {
            buf[br] = '\0';
            printf("%s", buf);
        }
        close(p[0]);
        wait(NULL);
    }
}

int main() {
    char cmd[256];
    while (1) {
        printf("hub> "); fflush(stdout);
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        cmd[strcspn(cmd, "\n")] = 0;

        if (strncmp(cmd, "start_monitor", 13) == 0) {
            start_monitor();
        } else if (strncmp(cmd, "calculate_scores", 16) == 0) {
            // Parsăm argumentele
            char *args[20];
            int i = 0;
            char *token = strtok(cmd, " ");
            while (token && i < 20) {
                args[i++] = token;
                token = strtok(NULL, " ");
            }
            calculate_scores(i, args);
        } else if (strcmp(cmd, "exit") == 0) {
            break;
        }
    }
    return 0;
}