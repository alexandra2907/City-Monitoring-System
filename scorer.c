#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "reports.h"

typedef struct {
    char name[20];
    int score;
} InspectorScore;

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", argv[1]);

    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0; // District gol sau inexistent

    InspectorScore scores[100];
    int count = 0;
    Report r;

    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        int found = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(scores[i].name, r.inspector_name) == 0) {
                scores[i].score += r.severity;
                found = 1; break;
            }
        }
        if (!found) {
            strncpy(scores[count].name, r.inspector_name, 19);
            scores[count].score = r.severity;
            count++;
        }
    }
    close(fd);

    printf("District: %s\n", argv[1]);
    for (int i = 0; i < count; i++) {
        printf("  Inspector %s: Score %d\n", scores[i].name, scores[i].score);
    }
    return 0;
}