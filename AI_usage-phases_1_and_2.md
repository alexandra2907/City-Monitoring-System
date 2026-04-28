# AI Usage Documentation - Phases 1 and 2

**Tool Used:** Google Gemini
**Purpose:** Technical guidance, understanding POSIX system calls, debugging, and code refactoring.

## Phase 1: File Management and Basic System Calls

**1. Understanding POSIX equivalents of standard C libraries**
* *Why:* The project strictly required system calls (`open`, `read`, `write`, `lseek`, `stat`) instead of the standard C library (`fopen`, `printf`).
* *How:* I used the AI to explain the syntax and flag differences (e.g., `O_WRONLY | O_CREAT | O_APPEND` for `open` instead of `"a"` in `fopen`), and how to correctly use file descriptors.
* *Where:* Applied in `execute_add`, `execute_list`, and the `log_operation` functions.

**2. Symlink and Directory Management**
* *Why:* Creating directories and handling symbolic links via POSIX calls can lead to dangling links if not managed properly.
* *How:* I asked the AI how to check for and remove dangling symlinks upon program startup. The AI suggested the combination of `opendir`, `readdir`, `lstat` (to check if it's a symlink), and `stat` (to check if the target exists).
* *Where:* In the `check_dangling_links` function.

**3. Code Architecture and Clean Code**
* *Why:* My initial approach was monolithic (everything in `main`), which made the code hard to read and maintain.
* *How:* I prompted the AI to help me refactor the code into a modular architecture, splitting operations (`add`, `list`, `filter`, `remove`) into separate functions while maintaining a robust argument parsing loop in `main`.

## Phase 2: Processes and Signals

**1. Process Creation (Fork and Exec)**
* *Why:* The `remove_district` command required spawning a child process to delete a directory using the system's `rm -rf` command.
* *How:* I used the AI to understand the relationship between `fork()`, `exec` family functions (specifically `execlp`), and `wait()`. The AI helped me structure the `if (pid == 0)` logic for the child and the `wait(NULL)` for the parent process to prevent zombie processes.
* *Where:* In the `execute_remove_district` function.

**2. Signal Handling (POSIX `sigaction`)**
* *Why:* The project explicitly forbade the use of `signal()` and required `sigaction()` for inter-process communication.
* *How:* I asked the AI how to properly initialize the `sigaction` struct, use `sigemptyset`, and assign handlers for `SIGUSR1` (for notifications) and `SIGINT` (for graceful shutdown).
* *Where:* In the `monitor_reports.c` file.

**3. Resolving Compilation Errors**
* *Why:* While compiling `monitor_reports.c`, I encountered undefined struct errors for `sigaction` in my IDE.
* *How:* I provided the error context to the AI, which explained the need for the `#define _POSIX_C_SOURCE 200809L` macro to expose the modern POSIX signal API to the compiler.
