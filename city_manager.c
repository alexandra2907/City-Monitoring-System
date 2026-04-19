#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include "reports.h"

// Funcția care transformă biții în text (rwx)
void mode_to_string(mode_t mode, char *str) {
    strcpy(str, "---------");
    if (mode & S_IRUSR) str[0] = 'r';
    if (mode & S_IWUSR) str[1] = 'w';
    if (mode & S_IXUSR) str[2] = 'x';
    if (mode & S_IRGRP) str[3] = 'r';
    if (mode & S_IWGRP) str[4] = 'w';
    if (mode & S_IXGRP) str[5] = 'x';
    if (mode & S_IROTH) str[6] = 'r';
    if (mode & S_IWOTH) str[7] = 'w';
    if (mode & S_IXOTH) str[8] = 'x';
}

// Funcția NOUĂ: Scrie în log cine și ce a făcut
void log_operation(const char *district, const char *user, const char *role, const char *action) {
    char path[256];
    snprintf(path, sizeof(path), "%s/logged_district", district);
    
    // Deschidem/creăm fișierul cu permisiuni 644
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%ld\t%s\t%s\t%s\n", time(NULL), user, role, action);
        write(fd, buf, strlen(buf));
        close(fd);
        chmod(path, 0644); // Forțăm permisiunile
    }
}

int main(int argc, char *argv[]) {
    char *role = NULL;
    char *user = NULL;
    char *command = NULL;
    char *district = NULL;
    char *extra_arg = NULL; // Pentru ID-ul de la 'view' sau 'remove_report'

    // Parsare îmbunătățită
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) role = argv[++i];
        else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) user = argv[++i];
        else if (argv[i][0] == '-' && argv[i][1] == '-') {
            command = argv[i] + 2; // Scoatem "--"
            if (i + 1 < argc) district = argv[++i];
            if (i + 1 < argc && argv[i+1][0] != '-') extra_arg = argv[++i];
        }
    }

    if (!role || !user || !command || !district) {
        fprintf(stderr, "Usage: %s --role <role> --user <user> --<command> <district> [extra_arg]\n", argv[0]);
        return 1;
    }

    // Creăm folderul districtului preventiv pentru orice comandă
    mkdir(district, 0750);
    chmod(district, 0750);

    // --- COMANDA ADD ---
    if (strcmp(command, "add") == 0) {
        log_operation(district, user, role, "add"); // LOGAM ACTIUNEA

        Report new_report;
        memset(&new_report, 0, sizeof(Report));
        
        new_report.report_id = time(NULL);
        strncpy(new_report.inspector_name, user, sizeof(new_report.inspector_name) - 1);
        new_report.timestamp = time(NULL);

        printf("X (Latitude): "); scanf("%f", &new_report.latitude);
        printf("Y (Longitude): "); scanf("%f", &new_report.longitude);
        printf("Category (road/lighting/flooding/other): "); scanf("%s", new_report.category);
        printf("Severity level (1/2/3): "); scanf("%d", &new_report.severity);
        
        int c; while ((c = getchar()) != '\n' && c != EOF); 
        printf("Description: ");
        fgets(new_report.description, sizeof(new_report.description), stdin);
        new_report.description[strcspn(new_report.description, "\n")] = 0;

        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s/reports.dat", district);

        int fd = open(filepath, O_WRONLY | O_CREAT | O_APPEND, 0664);
        if (fd < 0) { perror("Error"); return 1; }
        write(fd, &new_report, sizeof(Report));
        close(fd);
        chmod(filepath, 0664);
        printf("OK\n");
    } 
    // --- COMANDA LIST ---
    else if (strcmp(command, "list") == 0) {
        log_operation(district, user, role, "list"); // LOGAM ACTIUNEA

        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s/reports.dat", district);

        struct stat st;
        if (stat(filepath, &st) < 0) { perror("Error"); return 1; }

        char perms[10];
        mode_to_string(st.st_mode, perms);
        char time_str[64];
        struct tm *tm_info = localtime(&st.st_mtime);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

        printf("File: %s | Perms: %s | Size: %ld bytes | Last Mod: %s\n", filepath, perms, st.st_size, time_str);
        printf("-------------------------------------------------------------------\n");

        int fd = open(filepath, O_RDONLY);
        if (fd < 0) { perror("Error"); return 1; }

        Report r;
        while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
            printf("[%d] By: %s | Cat: %s | Sev: %d | Desc: %s\n", r.report_id, r.inspector_name, r.category, r.severity, r.description);
        }
        close(fd);
    }
    // --- COMANDA VIEW NOUA ---
    else if (strcmp(command, "view") == 0) {
        log_operation(district, user, role, "view");

        if (!extra_arg) {
            printf("Error: Missing report ID for view command.\n");
            return 1;
        }

        int target_id = atoi(extra_arg); // Convertim textul in numar
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s/reports.dat", district);

        int fd = open(filepath, O_RDONLY);
        if (fd < 0) { perror("Error"); return 1; }

        Report r;
        int found = 0;
        while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
            if (r.report_id == target_id) {
                printf("\n--- Report Details ---\n");
                printf("ID: %d\nInspector: %s\nCoords: %.4f, %.4f\nCategory: %s\nSeverity: %d\nDescription: %s\n",
                       r.report_id, r.inspector_name, r.latitude, r.longitude, r.category, r.severity, r.description);
                found = 1;
                break;
            }
        }
        if (!found) printf("Report with ID %d not found.\n", target_id);
        close(fd);
    }
    else {
        printf("Not implemented\n");
    }

    return 0;
}