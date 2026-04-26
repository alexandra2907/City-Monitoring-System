#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <dirent.h>
#include "reports.h"

#define BUF_SIZE 300

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

void check_dangling_links() {
    DIR *dir = opendir(".");
    if (dir == NULL) return;
    struct dirent *entry;
    struct stat lst, st;
    char *prefix = "active_reports-";
    int plen = strlen(prefix);

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, prefix, plen) == 0) {
            if (lstat(entry->d_name, &lst) == 0 && S_ISLNK(lst.st_mode)) {
                if (stat(entry->d_name, &st) == -1) {
                    char warn[256];
                    int len = snprintf(warn, sizeof(warn), "Warning: Dangling symlink detected: %s\n", entry->d_name);
                    write(STDOUT_FILENO, warn, len);
                    unlink(entry->d_name);
                }
            }
        }
    }
    closedir(dir);
}

void log_operation(const char *district, const char *user, const char *role, const char *action) {
    if (strcmp(role, "inspector") == 0) return;

    char path[256];
    snprintf(path, sizeof(path), "%s/logged_district", district);
    struct stat st;

    if (stat(path, &st) == 0 && !(st.st_mode & S_IWUSR)) {
        char *msg = "Manager lacks write access to log\n";
        write(STDERR_FILENO, msg, strlen(msg));
        return;
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
        char buf[256];
        int len = snprintf(buf, sizeof(buf), "%ld\t%s\t%s\t%s\n", time(NULL), user, role, action);
        write(fd, buf, len);
        close(fd);
    }
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
    } else if (strcmp(field, "timestamp") == 0) {
        time_t val = (time_t)atol(value);
        if (strcmp(op, "==") == 0) return r->timestamp == val;
        if (strcmp(op, "!=") == 0) return r->timestamp != val;
        if (strcmp(op, "<") == 0)  return r->timestamp < val;
        if (strcmp(op, "<=") == 0) return r->timestamp <= val;
        if (strcmp(op, ">") == 0)  return r->timestamp > val;
        if (strcmp(op, ">=") == 0) return r->timestamp >= val;
    } else if (strcmp(field, "category") == 0) {
        int cmp = strcmp(r->category, value);
        if (strcmp(op, "==") == 0) return cmp == 0;
        if (strcmp(op, "!=") == 0) return cmp != 0;
    } else if (strcmp(field, "inspector") == 0) {
        int cmp = strcmp(r->inspector_name, value);
        if (strcmp(op, "==") == 0) return cmp == 0;
        if (strcmp(op, "!=") == 0) return cmp != 0;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    check_dangling_links();

    char *role = NULL, *user = NULL, *command = NULL, *district = NULL;
    char *conditions[10];
    int cond_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) role = argv[++i];
        else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) user = argv[++i];
        else if (argv[i][0] == '-' && argv[i][1] == '-') {
            command = argv[i] + 2;
            if (i + 1 < argc) district = argv[++i];
            while (i + 1 < argc && argv[i+1][0] != '-') conditions[cond_count++] = argv[++i];
        }
    }

    if (!role || !user || !command || !district) {
        char *err = "Usage error: Missing required arguments.\n";
        write(STDERR_FILENO, err, strlen(err));
        return 1;
    }

    if (strcmp(role, "inspector") != 0 && strcmp(role, "manager") != 0) {
        char *err = "Invalid role\n";
        write(STDERR_FILENO, err, strlen(err));
        return 1;
    }

    struct stat dir_st;
    if (stat(district, &dir_st) == -1) {
        if (mkdir(district, 0750) < 0) {
            char *err = "Error creating district directory\n";
            write(STDERR_FILENO, err, strlen(err));
            return 1;
        }
    }

    char fp[256], msg_buf[512], input_buf[BUF_SIZE];
    snprintf(fp, sizeof(fp), "%s/reports.dat", district);
    struct stat file_st;

    if (strcmp(command, "add") == 0) {
        if (stat(fp, &file_st) == 0) {
            int can_write = 0;
            if (strcmp(role, "manager") == 0 && (file_st.st_mode & S_IWUSR)) can_write = 1;
            if (strcmp(role, "inspector") == 0 && (file_st.st_mode & S_IWGRP)) can_write = 1;
            if (!can_write) {
                char *err = "Error: Your role does not have writing privileges\n";
                write(STDERR_FILENO, err, strlen(err));
                return 1;
            }
        }

        int fd = open(fp, O_WRONLY | O_CREAT | O_APPEND, 0664);
        if (fd < 0) return 1;
        chmod(fp, 0664);

        char sym_path[256];
        snprintf(sym_path, sizeof(sym_path), "active_reports-%s", district);
        unlink(sym_path);
        symlink(fp, sym_path);

        Report nr; memset(&nr, 0, sizeof(Report));
        nr.report_id = (int)time(NULL);
        nr.timestamp = time(NULL);
        strncpy(nr.inspector_name, user, sizeof(nr.inspector_name) - 1);

        char *prompt = "X: "; write(STDOUT_FILENO, prompt, strlen(prompt));
        int br = read(STDIN_FILENO, input_buf, BUF_SIZE - 1);
        if (br > 0) { input_buf[br] = '\0'; nr.latitude = strtof(input_buf, NULL); }

        prompt = "Y: "; write(STDOUT_FILENO, prompt, strlen(prompt));
        br = read(STDIN_FILENO, input_buf, BUF_SIZE - 1);
        if (br > 0) { input_buf[br] = '\0'; nr.longitude = strtof(input_buf, NULL); }

        prompt = "Category: "; write(STDOUT_FILENO, prompt, strlen(prompt));
        br = read(STDIN_FILENO, input_buf, BUF_SIZE - 1);
        if (br > 0) {
            if (input_buf[br - 1] == '\n') input_buf[br - 1] = '\0'; else input_buf[br] = '\0';
            strncpy(nr.category, input_buf, sizeof(nr.category) - 1);
        }

        prompt = "Severity: "; write(STDOUT_FILENO, prompt, strlen(prompt));
        br = read(STDIN_FILENO, input_buf, BUF_SIZE - 1);
        if (br > 0) { input_buf[br] = '\0'; nr.severity = atoi(input_buf); }

        prompt = "Description: "; write(STDOUT_FILENO, prompt, strlen(prompt));
        br = read(STDIN_FILENO, input_buf, BUF_SIZE - 1);
        if (br > 0) {
            if (input_buf[br - 1] == '\n') input_buf[br - 1] = '\0'; else input_buf[br] = '\0';
            strncpy(nr.description, input_buf, sizeof(nr.description) - 1);
        }

        write(fd, &nr, sizeof(Report));
        close(fd);
        log_operation(district, user, role, "add");

        char cfg_path[256]; snprintf(cfg_path, sizeof(cfg_path), "%s/district.cfg", district);
        if (stat(cfg_path, &file_st) < 0) {
            int cfd = open(cfg_path, O_RDONLY | O_CREAT, 0640);
            if (cfd >= 0) { chmod(cfg_path, 0640); close(cfd); }
        }
        char *ok = "OK\n"; write(STDOUT_FILENO, ok, strlen(ok));
    } 
    else if (strcmp(command, "list") == 0 || strcmp(command, "view") == 0 || strcmp(command, "filter") == 0) {
        if (stat(fp, &file_st) == -1) {
            char *err = "District or report file does not exist\n"; write(STDERR_FILENO, err, strlen(err)); return 1;
        }

        int can_read = 0;
        if (strcmp(role, "manager") == 0 && (file_st.st_mode & S_IRUSR)) can_read = 1;
        if (strcmp(role, "inspector") == 0 && (file_st.st_mode & S_IRGRP)) can_read = 1;
        if (!can_read) {
            char *err = "No read privileges\n"; write(STDERR_FILENO, err, strlen(err)); return 1;
        }

        if (strcmp(command, "list") == 0) {
            char perms[10]; mode_to_string(file_st.st_mode, perms);
            int m_len = snprintf(msg_buf, sizeof(msg_buf), "File: %s | Perms: %s | Size: %ld\n", fp, perms, file_st.st_size);
            write(STDOUT_FILENO, msg_buf, m_len);
        }

        int fd = open(fp, O_RDONLY); if (fd < 0) return 1;
        Report r; int count = 0;
        while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
            int show = 0;
            if (strcmp(command, "list") == 0) show = 1;
            else if (strcmp(command, "view") == 0 && cond_count > 0 && r.report_id == atoi(conditions[0])) show = 1;
            else if (strcmp(command, "filter") == 0) {
                show = 1;
                for (int i = 0; i < cond_count; i++) {
                    char f[64], o[10], v[64];
                    if (parse_condition(conditions[i], f, o, v) && !match_condition(&r, f, o, v)) { show = 0; break; }
                }
            }

            if (show) {
                int out_len = snprintf(msg_buf, sizeof(msg_buf), "[%d] %s: %s (Sev: %d)\n", r.report_id, r.category, r.description, r.severity);
                write(STDOUT_FILENO, msg_buf, out_len);
                count++;
            }
        }
        close(fd);
        if (count == 0 && strcmp(command, "list") != 0) {
            char *err = "No reports found.\n"; write(STDERR_FILENO, err, strlen(err));
        } else {
            log_operation(district, user, role, command);
        }
    }
    else if (strcmp(command, "remove_report") == 0 || strcmp(command, "remove") == 0) {
        if (strcmp(role, "manager") != 0) {
            char *err = "Only manager can remove.\n"; write(STDERR_FILENO, err, strlen(err)); return 1;
        }
        int target = atoi(conditions[0]);
        int fd = open(fp, O_RDWR); if (fd < 0) return 1;
        
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
            char *ok = "Removed.\n"; write(STDOUT_FILENO, ok, strlen(ok));
            log_operation(district, user, role, "remove");
        }
        close(fd);
    }
    else if (strcmp(command, "update-threshold") == 0) {
        if (strcmp(role, "manager") != 0) {
            char *err = "Only manager can update threshold.\n"; write(STDERR_FILENO, err, strlen(err)); return 1;
        }
        char cfg_path[256]; snprintf(cfg_path, sizeof(cfg_path), "%s/district.cfg", district);
        if (stat(cfg_path, &file_st) == -1 || (file_st.st_mode & 0777) != 0640) {
            char *err = "Config file missing or permissions altered.\n"; write(STDERR_FILENO, err, strlen(err)); return 1;
        }
        int fd = open(cfg_path, O_WRONLY | O_TRUNC);
        if (fd >= 0) {
            int len = snprintf(msg_buf, sizeof(msg_buf), "%s\n", conditions[0]);
            write(fd, msg_buf, len);
            close(fd);
            char *ok = "Threshold updated.\n"; write(STDOUT_FILENO, ok, strlen(ok));
            log_operation(district, user, role, "update-threshold");
        }
    }
    return 0;
}