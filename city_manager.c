#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "reports.h"

int main(int argc, char *argv[]) {
    char *role = NULL;
    char *user = NULL;
    char *command = NULL;
    char *district = NULL;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            role = argv[++i];
        } else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            user = argv[++i];
        } else if (argv[i][0] == '-' && argv[i][1] == '-') {
            // Check for commands like --add, --list
            command = argv[i] + 2;
            if (i + 1 < argc) {
                district = argv[++i];
            }
        }
    }

    if (!role || !user || !command || !district) {
        fprintf(stderr, "Usage: %s --role <role> --user <user> --<command> <district> [args]\n", argv[0]);
        return 1;
    }

    printf("Role: %s\n", role);
    printf("User: %s\n", user);
    printf("Command: %s\n", command);
    printf("District: %s\n", district);

    return 0;
}