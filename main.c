#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // fork, execv, chdir, etc.
#include <sys/types.h>
#include <sys/wait.h>   // wait
#include <errno.h>
#include <sys/stat.h>   // stat

#define MAX_CMD_LEN 256
#define MAX_TOKENS 64

// Function prototypes
int tokenize(char *line, char *argv[]);
int is_builtin(char **argv);
void execute_builtin(char **argv);
char* find_executable(char *cmd);
void execute_command(char **argv);

int main(void) {
    char cmdline[MAX_CMD_LEN];

    while (1) {
        // Print a prompt
        printf("os1> ");
        fflush(stdout);

        // Read input
        if (!fgets(cmdline, sizeof(cmdline), stdin)) {
            // EOF or error
            break;
        }

        // Remove trailing newline
        cmdline[strcspn(cmdline, "\n")] = '\0';

        // Token array
        char *argv[MAX_TOKENS];
        int token_count = tokenize(cmdline, argv);
        if (token_count == 0) {
            // Empty line -> prompt again
            continue;
        }

        // Check if it's a built-in command
        if (is_builtin(argv)) {
            execute_builtin(argv);
            // If "exit", we'll exit inside execute_builtin().
            continue;
        }

        // Otherwise, it's an external command
        execute_command(argv);
    }

    printf("Exiting os1 shell.\n");
    return 0;
}

// --------------------- Tokenize ---------------------
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

// --------------------- Built-in Checks ---------------------
int is_builtin(char **argv) {
    if (strcmp(argv[0], "exit") == 0) {
        return 1;  // "exit" is a builtin
    }
    else if (strcmp(argv[0], "cd") == 0) {
        return 1;  // "cd" is also builtin
    }
    return 0;
}

void execute_builtin(char **argv) {
    if (strcmp(argv[0], "exit") == 0) {
        // You could do any cleanup if needed
        exit(0); // directly exit the shell
    }
    else if (strcmp(argv[0], "cd") == 0) {
        // If cd has a second argument, use it
        if (argv[1] == NULL) {
            fprintf(stderr, "cd: missing argument\n");
            return;
        }
        if (chdir(argv[1]) != 0) {
            perror("cd");
        }
    }
}

// --------------------- External Command Execution ---------------------
void execute_command(char **argv) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }
    else if (pid == 0) {
        // Child: find the executable in PATH (if needed)
        char *path = find_executable(argv[0]);
        if (path == NULL) {
            fprintf(stderr, "%s: command not found\n", argv[0]);
            exit(1);
        }

        execv(path, argv);
        // If execv returns, it's an error
        perror("execv");
        free(path);
        exit(1);
    }
    else {
        // Parent: wait for child to complete
        int status;
        waitpid(pid, &status, 0);
    }
}

// --------------------- PATH Search ---------------------
char* find_executable(char *cmd) {
    // If cmd contains '/', assume it's an absolute or relative path
    if (strchr(cmd, '/')) {
        // Return a copy of cmd, letting execv either succeed or fail
        return strdup(cmd);
    }

    // Otherwise, search in PATH
    char *path_env = getenv("PATH");
    if (!path_env) {
        // If PATH not set, fallback to something like /bin:/usr/bin
        path_env = "/bin:/usr/bin";
    }

    // Duplicate path_env so we can use strtok on it
    char *paths = strdup(path_env);
    if (!paths) {
        return NULL; // memory error
    }

    char *token = strtok(paths, ":");
    while (token) {
        // Construct potential path
        size_t len = strlen(token) + 1 + strlen(cmd) + 1; // e.g. "/usr/bin" + "/" + "ls"
        char *fullpath = malloc(len);
        if (!fullpath) {
            free(paths);
            return NULL;
        }
        snprintf(fullpath, len, "%s/%s", token, cmd);

        struct stat st;
        if (stat(fullpath, &st) == 0 && (st.st_mode & S_IXUSR)) {
            free(paths);
            return fullpath;  // caller will free
        }
        free(fullpath);

        token = strtok(NULL, ":");
    }

    free(paths);
    // Not found
    return NULL;
}
