#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include "reports.h"

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

int parse_condition(const char *input, char *field, char *op, char *value) {
    if (sscanf(input, "%[^:]:%[^:]:%s", field, op, value) == 3) return 1;
    return 0;
}

int match_condition(Report *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0) {
        int val = atoi(value);
        if (strcmp(op, "==") == 0) return r->severity == val;
        if (strcmp(op, "!=") == 0) return r->severity != val;
        if (strcmp(op, "<") == 0)  return r->severity < val;
        if (strcmp(op, "<=") == 0) return r->severity <= val;
        if (strcmp(op, ">") == 0)  return r->severity > val;
        if (strcmp(op, ">=") == 0) return r->severity >= val;
    } else if (strcmp(field, "category") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->category, value) == 0;
        if (strcmp(op, "!=") == 0) return strcmp(r->category, value) != 0;
    } else if (strcmp(field, "inspector") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->inspector_name, value) == 0;
        if (strcmp(op, "!=") == 0) return strcmp(r->inspector_name, value) != 0;
    }
    return 0;
}

void log_operation(const char *district, const char *user, const char *role, const char *action) {
    char path[256];
    snprintf(path, sizeof(path), "%s/logged_district", district);
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%ld\t%s\t%s\t%s\n", time(NULL), user, role, action);
        write(fd, buf, strlen(buf));
        close(fd);
        chmod(path, 0644);
    }
}

void update_symlink(const char *district) {
    char linkname[256], target[256];
    snprintf(linkname, sizeof(linkname), "active_reports-%s", district);
    snprintf(target, sizeof(target), "%s/reports.dat", district);
    unlink(linkname);
    symlink(target, linkname);
}

int main(int argc, char *argv[]) {
    char *role = NULL, *user = NULL, *command = NULL, *district = NULL;
    char *conditions[10];
    int cond_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) role = argv[++i];
        else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) user = argv[++i];
        else if (argv[i][0] == '-' && argv[i][1] == '-') {
            command = argv[i] + 2;
            if (i + 1 < argc) district = argv[++i];
            while (i + 1 < argc && argv[i+1][0] != '-') {
                conditions[cond_count++] = argv[++i];
            }
        }
    }

    if (!role || !user || !command || !district) return 1;

    mkdir(district, 0750);
    chmod(district, 0750);

    if (strcmp(command, "add") == 0) {
        log_operation(district, user, role, "add");
        Report nr; memset(&nr, 0, sizeof(Report));
        nr.report_id = (int)time(NULL);
        strncpy(nr.inspector_name, user, 31);
        nr.timestamp = time(NULL);
        printf("X: "); scanf("%f", &nr.latitude);
        printf("Y: "); scanf("%f", &nr.longitude);
        printf("Cat: "); scanf("%s", nr.category);
        printf("Sev: "); scanf("%d", &nr.severity);
        int c; while ((c = getchar()) != '\n' && c != EOF);
        printf("Desc: "); fgets(nr.description, 135, stdin);
        nr.description[strcspn(nr.description, "\n")] = 0;
        char fp[256]; snprintf(fp, sizeof(fp), "%s/reports.dat", district);
        int fd = open(fp, O_WRONLY | O_CREAT | O_APPEND, 0664);
        write(fd, &nr, sizeof(Report));
        close(fd);
        chmod(fp, 0664);
        update_symlink(district);
        printf("OK\n");
    } 
    else if (strcmp(command, "list") == 0) {
        log_operation(district, user, role, "list");
        char fp[256]; snprintf(fp, sizeof(fp), "%s/reports.dat", district);
        struct stat st;
        if (stat(fp, &st) < 0) { printf("No reports.\n"); return 1; }
        char perms[10]; mode_to_string(st.st_mode, perms);
        printf("File: %s | Perms: %s | Size: %ld\n", fp, perms, st.st_size);
        int fd = open(fp, O_RDONLY);
        Report r;
        while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
            printf("[%d] %s: %s\n", r.report_id, r.category, r.description);
        }
        close(fd);
    }
    else if (strcmp(command, "view") == 0) {
        log_operation(district, user, role, "view");
        if (cond_count == 0) return 1;
        int target = atoi(conditions[0]);
        char fp[256]; snprintf(fp, sizeof(fp), "%s/reports.dat", district);
        int fd = open(fp, O_RDONLY);
        Report r;
        while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
            if (r.report_id == target) {
                printf("ID: %d\nInspector: %s\nCat: %s\nDesc: %s\n", r.report_id, r.inspector_name, r.category, r.description);
                break;
            }
        }
        close(fd);
    }
    else if (strcmp(command, "filter") == 0) {
        log_operation(district, user, role, "filter");
        char fp[256]; snprintf(fp, sizeof(fp), "%s/reports.dat", district);
        int fd = open(fp, O_RDONLY);
        Report r;
        while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
            int all_match = 1;
            for (int i = 0; i < cond_count; i++) {
                char f[64], o[10], v[64];
                if (parse_condition(conditions[i], f, o, v)) {
                    if (!match_condition(&r, f, o, v)) { all_match = 0; break; }
                }
            }
            if (all_match) printf("[%d] %s: %s\n", r.report_id, r.category, r.description);
        }
        close(fd);
    }
    else if (strcmp(command, "remove_report") == 0) {
        if (strcmp(role, "manager") != 0) { printf("Access denied.\n"); return 1; }
        log_operation(district, user, role, "remove");
        int target = atoi(conditions[0]);
        char fp[256]; snprintf(fp, sizeof(fp), "%s/reports.dat", district);
        int fd = open(fp, O_RDWR);
        Report r; off_t pos = 0; int found = 0;
        while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
            if (r.report_id == target) { found = 1; break; }
            pos += sizeof(Report);
        }
        if (found) {
            off_t wp = pos;
            while (lseek(fd, pos + sizeof(Report), SEEK_SET) >= 0 && read(fd, &r, sizeof(Report)) == sizeof(Report)) {
                lseek(fd, wp, SEEK_SET); write(fd, &r, sizeof(Report));
                wp += sizeof(Report); pos += sizeof(Report);
            }
            ftruncate(fd, wp);
            printf("Removed.\n");
        }
        close(fd);
        update_symlink(district);
    }
    return 0;
}