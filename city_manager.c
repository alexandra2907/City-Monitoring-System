#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <dirent.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <signal.h>
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
    
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, prefix, strlen(prefix)) == 0) {
            if (lstat(entry->d_name, &lst) == 0 && S_ISLNK(lst.st_mode)) {
                if (stat(entry->d_name, &st) == -1) {
                    char warn[256];
                    int len = snprintf(warn, sizeof(warn), "Warning: Dangling symlink removed: %s\n", entry->d_name);
                    write(STDOUT_FILENO, warn, len);
                    unlink(entry->d_name);
                }
            }
        }
    }
    closedir(dir);
}

void log_operation(const char *district, const char *user, const char *role, const char *action) {
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

void notify_monitor(const char *district, const char *user, const char *role) {
    int fd = open(".monitor_pid", O_RDONLY);
    int monitor_informed = 0;

    if (fd >= 0) {
        char pid_buf[16];
        int br = read(fd, pid_buf, sizeof(pid_buf) - 1);
        close(fd);
        if (br > 0) {
            pid_buf[br] = '\0';
            pid_t monitor_pid = atoi(pid_buf);
            if (kill(monitor_pid, SIGUSR1) == 0) {
                monitor_informed = 1;
            }
        }
    }

    if (monitor_informed) {
        log_operation(district, user, role, "add (Monitor notified)");
    } else {
        log_operation(district, user, role, "add (Monitor could not be informed)");
    }
}

int has_permission(const char *path, const char *role, int check_write) {
    struct stat st;
    if (stat(path, &st) == -1) return 1;
    
    if (check_write) {
        if (strcmp(role, "manager") == 0 && (st.st_mode & S_IWUSR)) return 1;
        if (strcmp(role, "inspector") == 0 && (st.st_mode & S_IWGRP)) return 1;
    } else {
        if (strcmp(role, "manager") == 0 && (st.st_mode & S_IRUSR)) return 1;
        if (strcmp(role, "inspector") == 0 && (st.st_mode & S_IRGRP)) return 1;
    }
    return 0;
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
    }
    return 0;
}

void execute_add(const char *district, const char *user, const char *role) {
    struct stat st;
    if (stat(district, &st) == -1) {
        if (mkdir(district, 0750) < 0) {
            char *err = "Error creating district directory\n";
            write(STDERR_FILENO, err, strlen(err));
            return;
        }
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    if (!has_permission(path, role, 1)) {
        char *err = "Error: Your role does not have writing privileges\n";
        write(STDERR_FILENO, err, strlen(err));
        exit(-1);
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0664);
    if (fd < 0) return;
    chmod(path, 0664);

    char sym_path[256];
    snprintf(sym_path, sizeof(sym_path), "active_reports-%s", district);
    unlink(sym_path);
    symlink(path, sym_path);

    char input_buf[BUF_SIZE];
    Report nr;
    memset(&nr, 0, sizeof(Report));
    nr.report_id = rand() % 900000 + 100000;
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

    int valid_sev = 0;
    while (!valid_sev) {
        prompt = "Severity (1-4): "; write(STDOUT_FILENO, prompt, strlen(prompt));
        br = read(STDIN_FILENO, input_buf, BUF_SIZE - 1);
        if (br > 0) {
            input_buf[br] = '\0';
            nr.severity = atoi(input_buf);
            if (nr.severity >= 1 && nr.severity <= 4) {
                valid_sev = 1;
            } else {
                char *err = "Invalid severity! Must be 1, 2, 3 or 4.\n";
                write(STDOUT_FILENO, err, strlen(err));
            }
        } else {
            break;
        }
    }

    prompt = "Description: "; write(STDOUT_FILENO, prompt, strlen(prompt));
    br = read(STDIN_FILENO, input_buf, BUF_SIZE - 1);
    if (br > 0) {
        if (input_buf[br - 1] == '\n') input_buf[br - 1] = '\0'; else input_buf[br] = '\0';
        strncpy(nr.description, input_buf, sizeof(nr.description) - 1);
    }

    write(fd, &nr, sizeof(Report));
    close(fd);

    notify_monitor(district, user, role);

    char cfg_path[256];
    snprintf(cfg_path, sizeof(cfg_path), "%s/district.cfg", district);
    if (stat(cfg_path, &st) < 0) {
        int cfd = open(cfg_path, O_RDONLY | O_CREAT, 0640);
        if (cfd >= 0) { chmod(cfg_path, 0640); close(cfd); }
    }
    char *ok = "OK\n"; write(STDOUT_FILENO, ok, strlen(ok));
}

void execute_list(const char *district, const char *user, const char *role) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);
    struct stat st;

    if (stat(path, &st) == -1) {
        char *err = "District or report file does not exist\n";
        write(STDERR_FILENO, err, strlen(err));
        return;
    }

    if (!has_permission(path, role, 0)) {
        char *err = "Declared role does not have reading privileges\n";
        write(STDERR_FILENO, err, strlen(err));
        exit(-1);
    }

    char perms[10], out[512];
    mode_to_string(st.st_mode, perms);
    int len = snprintf(out, sizeof(out), "File: %s | Perms: %s | Size: %ld\n", path, perms, st.st_size);
    write(STDOUT_FILENO, out, len);

    int fd = open(path, O_RDONLY);
    if (fd < 0) return;

    Report r;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        len = snprintf(out, sizeof(out), "ID: %d | Inspector: %s | Cat: %s | Sev: %d\nDesc: %s\n---\n",
                       r.report_id, r.inspector_name, r.category, r.severity, r.description);
        write(STDOUT_FILENO, out, len);
    }
    close(fd);
    log_operation(district, user, role, "list");
}

void execute_filter(const char *district, int cond_count, char **conditions, const char *user, const char *role) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);
    
    if (!has_permission(path, role, 0)) {
        char *err = "No read privileges\n";
        write(STDERR_FILENO, err, strlen(err));
        exit(-1);
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) return;

    Report r;
    int matches = 0;
    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        int show = 1;
        for (int i = 0; i < cond_count; i++) {
            char f[64], o[10], v[64];
            if (parse_condition(conditions[i], f, o, v)) {
                if (!match_condition(&r, f, o, v)) { show = 0; break; }
            }
        }
        if (show) {
            char out[512];
            int len = snprintf(out, sizeof(out), "ID: %d | Inspector: %s | Cat: %s | Sev: %d\n", 
                               r.report_id, r.inspector_name, r.category, r.severity);
            write(STDOUT_FILENO, out, len);
            matches++;
        }
    }
    close(fd);
    if (matches == 0) {
        char *err = "No reports matched the specified filters\n";
        write(STDERR_FILENO, err, strlen(err));
    }
    log_operation(district, user, role, "filter");
}

void execute_remove(const char *district, int target_id, const char *user, const char *role) {
    if (strcmp(role, "manager") != 0) {
        char *err = "Error: Only manager can remove.\n";
        write(STDERR_FILENO, err, strlen(err));
        exit(-1);
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);
    int fd = open(path, O_RDWR);
    if (fd < 0) return;

    Report r;
    off_t pos = 0;
    int found = 0;

    while (read(fd, &r, sizeof(Report)) == sizeof(Report)) {
        if (r.report_id == target_id) { found = 1; break; }
        pos += sizeof(Report);
    }

    if (found) {
        off_t wp = pos;
        while (lseek(fd, pos + sizeof(Report), SEEK_SET) >= 0 && read(fd, &r, sizeof(Report)) == sizeof(Report)) {
            lseek(fd, wp, SEEK_SET);
            write(fd, &r, sizeof(Report));
            wp += sizeof(Report);
            pos += sizeof(Report);
        }
        ftruncate(fd, wp);
        char *ok = "Successfully removed report\n";
        write(STDOUT_FILENO, ok, strlen(ok));
        log_operation(district, user, role, "remove");
    } else {
        char *err = "Report not found\n";
        write(STDERR_FILENO, err, strlen(err));
    }
    close(fd);
}

void execute_remove_district(const char *district, const char *role) {
    if (strcmp(role, "manager") != 0) {
        char *err = "Error: Only manager can remove a district.\n";
        write(STDERR_FILENO, err, strlen(err));
        exit(-1);
    }

    pid_t pid = fork();
    if (pid == 0) {
        char sym_path[256];
        snprintf(sym_path, sizeof(sym_path), "active_reports-%s", district);
        unlink(sym_path);
        execlp("rm", "rm", "-rf", district, NULL);
        exit(1);
    } else if (pid > 0) {
        wait(NULL);
        char *ok = "District removed successfully\n";
        write(STDOUT_FILENO, ok, strlen(ok));
    }
}

void execute_update_threshold(const char *district, int new_limit, const char *user, const char *role) {
    if (strcmp(role, "manager") != 0) {
        char *err = "Error: Only manager can update threshold.\n";
        write(STDERR_FILENO, err, strlen(err));
        exit(-1);
    }

    char cfg_path[256];
    snprintf(cfg_path, sizeof(cfg_path), "%s/district.cfg", district);
    struct stat st;
    
    if (stat(cfg_path, &st) == -1 || (st.st_mode & 0777) != 0640) {
        char *err = "District config file missing or permissions altered\n";
        write(STDERR_FILENO, err, strlen(err));
        exit(-1);
    }

    int fd = open(cfg_path, O_WRONLY | O_TRUNC);
    if (fd >= 0) {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%d\n", new_limit);
        write(fd, buf, len);
        close(fd);
        char *ok = "Threshold updated successfully\n";
        write(STDOUT_FILENO, ok, strlen(ok));
        log_operation(district, user, role, "update-threshold");
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
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
        char *err = "Invalid role\n"; write(STDERR_FILENO, err, strlen(err));
        exit(-1);
    }

    if (strcmp(command, "add") == 0) {
        execute_add(district, user, role);
    } 
    else if (strcmp(command, "list") == 0) {
        execute_list(district, user, role);
    } 
    else if (strcmp(command, "filter") == 0) {
        execute_filter(district, cond_count, conditions, user, role);
    } 
    else if (strcmp(command, "remove") == 0 || strcmp(command, "remove_report") == 0) {
        if (cond_count > 0) execute_remove(district, atoi(conditions[0]), user, role);
    } 
    else if (strcmp(command, "remove_district") == 0) {
        execute_remove_district(district, role);
    }
    else if (strcmp(command, "update-threshold") == 0) {
        if (cond_count > 0) execute_update_threshold(district, atoi(conditions[0]), user, role);
    } 
    else {
        char *err = "Invalid command\n";
        write(STDERR_FILENO, err, strlen(err));
    }

    return 0;
}