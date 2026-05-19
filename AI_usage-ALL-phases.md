# AI Usage Documentation - All Phases (1, 2, and 3)

**Tool Used:** Google Gemini
**Purpose:** Technical guidance, POSIX system calls, Inter-Process Communication (IPC), and debugging.

## Phase 1 & 2: Basics, Processes, and Signals
* **File I/O:** Used AI to transition from `stdio.h` to POSIX `open/read/write` for binary file handling in `reports.dat`.
* **Process Creation:** Implemented `fork()` and `execlp()` for directory removal and report management based on AI-provided boilerplate for parent-child synchronization.
* **Signals:** Configured `sigaction` for `SIGUSR1` and `SIGINT` to allow communication between `city_manager` and `monitor_reports`.

## Phase 3: Pipes and Redirects
**1. Inter-Process Communication (Pipes)**
* *Why:* The `city_hub` needed to capture the output of background processes (`monitor_reports` and `scorer`) without them printing directly to the terminal in an unorganized way.
* *How:* AI suggested the `pipe()` and `dup2()` workflow. I learned how to redirect `STDOUT_FILENO` of a child process to the write-end of a pipe, while the parent reads from the other end.

**2. Background Execution & Hub Management**
* *Why:* `start_monitor` required the monitor to stay alive in the background while the Hub remains interactive.
* *How:* The AI explained how to fork a dedicated "manager" process (`hub_mon`) that handles the monitor's lifecycle and pipe reading, allowing the main Hub loop to continue accepting user input.

**3. Data Aggregation (Scorer)**
* *Why:* `calculate_scores` required aggregating data from binary files and returning a plain-text summary.
* *How:* AI assisted in designing a struct-based approach to group inspector scores by name during a single pass of the binary file.

## Conclusion
AI was instrumental in implementing IPC mechanisms. It provided clarity on how file descriptors behave across forks and how to safely manage background processes using pipes and signals.# AI Usage Documentation - All Phases (1, 2, and 3)

**Tool Used:** Google Gemini
**Purpose:** Technical guidance, POSIX system calls, Inter-Process Communication (IPC), and debugging.

## Phase 1 & 2: Basics, Processes, and Signals
* **File I/O:** Used AI to transition from `stdio.h` to POSIX `open/read/write` for binary file handling in `reports.dat`.
* **Process Creation:** Implemented `fork()` and `execlp()` for directory removal and report management based on AI-provided boilerplate for parent-child synchronization.
* **Signals:** Configured `sigaction` for `SIGUSR1` and `SIGINT` to allow communication between `city_manager` and `monitor_reports`.

## Phase 3: Pipes and Redirects
**1. Inter-Process Communication (Pipes)**
* *Why:* The `city_hub` needed to capture the output of background processes (`monitor_reports` and `scorer`) without them printing directly to the terminal in an unorganized way.
* *How:* AI suggested the `pipe()` and `dup2()` workflow. I learned how to redirect `STDOUT_FILENO` of a child process to the write-end of a pipe, while the parent reads from the other end.

**2. Background Execution & Hub Management**
* *Why:* `start_monitor` required the monitor to stay alive in the background while the Hub remains interactive.
* *How:* The AI explained how to fork a dedicated "manager" process (`hub_mon`) that handles the monitor's lifecycle and pipe reading, allowing the main Hub loop to continue accepting user input.

**3. Data Aggregation (Scorer)**
* *Why:* `calculate_scores` required aggregating data from binary files and returning a plain-text summary.
* *How:* AI assisted in designing a struct-based approach to group inspector scores by name during a single pass of the binary file.

## Conclusion
AI was instrumental in implementing IPC mechanisms. It provided clarity on how file descriptors behave across forks and how to safely manage background processes using pipes and signals.