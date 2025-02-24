#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // fork, execv, chdir, getcwd, etc.
#include <sys/types.h>
#include <sys/wait.h>   // wait, waitpid
#include <errno.h>
#include <sys/stat.h>   // stat

#define MAX_CMD_LEN 256
#define MAX_TOKENS 64

// ---------------------------------------------------------------------
// Forward Declarations
// ---------------------------------------------------------------------
int tokenize(char *line, char *argv[]);
int is_builtin(char **argv);
void execute_builtin(char **argv);
char* find_executable(char *cmd);
void execute_command(char **argv);
void execute_pipe(char **left_argv, char **right_argv);

// ---------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------
int main(void) {
    char cmdline[MAX_CMD_LEN];

    while (1) {
        // Dynamically get current working directory
        char *cwd = getcwd(NULL, 0);  // NULL + 0 => allocate as needed
        if (cwd) {
            printf("os1:%s> ", cwd);
            free(cwd);               // free after printing
        } else {
            perror("getcwd");
            printf("os1> ");         // fallback
        }
        fflush(stdout);

        // Read input (fgets returns NULL if EOF or error)
        if (!fgets(cmdline, sizeof(cmdline), stdin)) {
            break; // end of file or error
        }

        // Remove trailing newline
        cmdline[strcspn(cmdline, "\n")] = '\0';

        // Tokenize input
        char *argv[MAX_TOKENS];
        int token_count = tokenize(cmdline, argv);
        if (token_count == 0) {
            continue; // empty line, prompt again
        }

        // Detect if we have a pipe
        int pipeIndex = -1;
        for (int i = 0; i < token_count; i++) {
            if (strcmp(argv[i], "|") == 0) {
                pipeIndex = i;
                break;
            }
        }

        // Check for multiple pipes (if you only support one)
        if (pipeIndex != -1) {
            // See if there's another pipe beyond this
            for (int i = pipeIndex + 1; i < token_count; i++) {
                if (strcmp(argv[i], "|") == 0) {
                    fprintf(stderr, "Error: multiple pipes not supported.\n");
                    pipeIndex = -1; // reset to ignore
                    break;
                }
            }
        }

        // If no pipe or ignoring multiple-pipe scenario:
        if (pipeIndex == -1) {
            // Either a built-in or single external command
            if (is_builtin(argv)) {
                execute_builtin(argv);
            } else {
                execute_command(argv);
            }
        } else {
            // We found exactly one pipe at pipeIndex
            // Ensure there's a command on both sides
            if (pipeIndex == 0 || pipeIndex == token_count - 1) {
                fprintf(stderr, "Error: missing command before or after '|'\n");
                continue;
            }

            // Build left_argv
            char *left_argv[MAX_TOKENS];
            int left_count = 0;
            for (int i = 0; i < pipeIndex; i++) {
                left_argv[left_count++] = argv[i];
            }
            left_argv[left_count] = NULL;

            // Build right_argv
            char *right_argv[MAX_TOKENS];
            int right_count = 0;
            for (int i = pipeIndex + 1; i < token_count; i++) {
                right_argv[right_count++] = argv[i];
            }
            right_argv[right_count] = NULL;

            // Execute the pipe
            execute_pipe(left_argv, right_argv);
        }
    }

    printf("Exiting os1 shell.\n");
    return 0;
}

// ---------------------------------------------------------------------
// Tokenize: Splits a line on whitespace into argv[], returns token count
// ---------------------------------------------------------------------
int tokenize(char *line, char *argv[]) {
    int count = 0;
    char *token = strtok(line, " \t\r\n");
    while (token && count < MAX_TOKENS - 1) {
        argv[count++] = token;
        token = strtok(NULL, " \t\r\n");
    }
    argv[count] = NULL;
    return count;
}

// ---------------------------------------------------------------------
// Check if argv[0] is a builtin command
// ---------------------------------------------------------------------
int is_builtin(char **argv) {
    if (strcmp(argv[0], "exit") == 0) {
        return 1;
    } else if (strcmp(argv[0], "cd") == 0) {
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------
// Execute Builtin: exit, cd
// ---------------------------------------------------------------------
void execute_builtin(char **argv) {
    if (strcmp(argv[0], "exit") == 0) {
        exit(0); // directly exit the shell
    } else if (strcmp(argv[0], "cd") == 0) {
        // cd requires second arg
        if (argv[1] == NULL) {
            fprintf(stderr, "cd: missing argument\n");
            return;
        }
        if (chdir(argv[1]) != 0) {
            perror("cd");
        }
    }
}

// ---------------------------------------------------------------------
// Manual PATH Search
// Returns a malloc'd string if found, else NULL.
// Caller should free the returned string.
// ---------------------------------------------------------------------
char* find_executable(char *cmd) {
    // If cmd contains '/', assume it's a path
    if (strchr(cmd, '/')) {
        // Just return a copy
        return strdup(cmd);
    }

    // Otherwise, look in PATH
    char *path_env = getenv("PATH");
    if (!path_env) {
        // default if PATH not set
        path_env = "/bin:/usr/bin";
    }

    char *paths = strdup(path_env);
    if (!paths) {
        return NULL;
    }

    char *token = strtok(paths, ":");
    while (token) {
        // Construct full path: token + "/" + cmd
        size_t len = strlen(token) + 1 + strlen(cmd) + 1;
        char *fullpath = malloc(len);
        if (!fullpath) {
            free(paths);
            return NULL;
        }
        snprintf(fullpath, len, "%s/%s", token, cmd);

        // Check if it's executable
        struct stat st;
        if (stat(fullpath, &st) == 0 && (st.st_mode & S_IXUSR)) {
            free(paths);
            return fullpath;  // success
        }

        free(fullpath);
        token = strtok(NULL, ":");
    }
    free(paths);
    return NULL; // not found
}

// ---------------------------------------------------------------------
// Execute a single external command (no pipe)
// ---------------------------------------------------------------------
void execute_command(char **argv) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    } else if (pid == 0) {
        // Child
        char *path = find_executable(argv[0]);
        if (!path) {
            fprintf(stderr, "%s: command not found\n", argv[0]);
            exit(1);
        }
        execv(path, argv);
        perror("execv");
        free(path);
        exit(1);
    } else {
        // Parent
        int status;
        waitpid(pid, &status, 0);
    }
}

// ---------------------------------------------------------------------
// Execute two commands connected by a single pipe
// left_argv writes to pipe, right_argv reads from pipe
// ---------------------------------------------------------------------
void execute_pipe(char **left_argv, char **right_argv) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        return;
    }

    // Fork for left command
    pid_t left_pid = fork();
    if (left_pid < 0) {
        perror("fork (left)");
        return;
    } else if (left_pid == 0) {
        // Left child
        close(pipefd[0]);                // not reading
        dup2(pipefd[1], STDOUT_FILENO);  // redirect stdout to pipe
        close(pipefd[1]);

        // Execute left command
        char *path = find_executable(left_argv[0]);
        if (!path) {
            fprintf(stderr, "%s: command not found\n", left_argv[0]);
            exit(1);
        }
        execv(path, left_argv);
        perror("execv (left)");
        free(path);
        exit(1);
    }

    // Fork for right command
    pid_t right_pid = fork();
    if (right_pid < 0) {
        perror("fork (right)");
        return;
    } else if (right_pid == 0) {
        // Right child
        close(pipefd[1]);               // not writing
        dup2(pipefd[0], STDIN_FILENO);  // redirect stdin from pipe
        close(pipefd[0]);

        // Execute right command
        char *path = find_executable(right_argv[0]);
        if (!path) {
            fprintf(stderr, "%s: command not found\n", right_argv[0]);
            exit(1);
        }
        execv(path, right_argv);
        perror("execv (right)");
        free(path);
        exit(1);
    }

    // Parent: close both ends of pipe
    close(pipefd[0]);
    close(pipefd[1]);

    // Wait for both children
    int status;
    waitpid(left_pid, &status, 0);
    waitpid(right_pid, &status, 0);
}
