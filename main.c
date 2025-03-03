//Mohammad Hammad Arif s3668630
#include "header.h"

int main(void) {
    char cmdline[MAX_CMD_LEN];

    while (1) {
        //Dynamically gets the current working directory
        char *cwd = getcwd(NULL, 0);  
        if (cwd) {
            printf("os1:%s> ", cwd);
            free(cwd);              
        } else {
            perror("getcwd");
            printf("os1> ");         
        }
        fflush(stdout);

        //Reads input
        if (!fgets(cmdline, sizeof(cmdline), stdin)) {
            break; 
        }

        //Removes newline
        cmdline[strcspn(cmdline, "\n")] = '\0';

        //Tokenizes input
        char *argv[MAX_TOKENS];
        int token_count = tokenize(cmdline, argv);
        if (token_count == 0) {
            continue; 
        }

        //Check for pipeline
        int pipeIndex = -1;
        for (int i = 0; i < token_count; i++) {
            if (strcmp(argv[i], "|") == 0) {
                pipeIndex = i;
                break;
            }
        }

        //Check for multiple pipelines
        if (pipeIndex != -1) {
            for (int i = pipeIndex + 1; i < token_count; i++) {
                if (strcmp(argv[i], "|") == 0) {
                    fprintf(stderr, "Error: multiple pipes not supported.\n");
                    pipeIndex = -1; 
                    break;
                }
            }
        }

        //If no pipeline
        if (pipeIndex == -1) {
            if (is_builtin(argv)) {
                execute_builtin(argv);
            } else {
                execute_command(argv);
            }
        } 
        //If pipeline
        else {
            if (pipeIndex == 0 || pipeIndex == token_count - 1) {
                fprintf(stderr, "Error: missing command before or after '|'\n");
                continue;
            }

            //Build left_argv
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

            execute_pipe(left_argv, right_argv);
        }
    }

    printf("Exiting os1 shell.\n");
    return 0;
}


//Tokenizer that takes a line and returns an array of tokens
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

//Checks if the input command is a builtin command
int is_builtin(char **argv) {
    if (strcmp(argv[0], "exit") == 0) {
        return 1;
    } else if (strcmp(argv[0], "cd") == 0) {
        return 1;
    }
    return 0;
}

//Executes the command
void execute_builtin(char **argv) {
    if (strcmp(argv[0], "exit") == 0) {
        exit(0); 
    } else if (strcmp(argv[0], "cd") == 0) {
        if (argv[1] == NULL) {
            fprintf(stderr, "cd: missing argument\n");
            return;
        }
        if (chdir(argv[1]) != 0) {
            perror("cd");
        }
    }
}

//Finds the full path of an executable
char* find_executable(char *cmd) {
    if (strchr(cmd, '/')) {
        return strdup(cmd);
    }
    //Get PATH
    char *path_env = getenv("PATH");
    if (!path_env) {
        // default PATH
        path_env = "/bin:/usr/bin";
    }
    //Make a copy of PATH
    char *paths = strdup(path_env);
    if (!paths) {
        return NULL;
    }
    //Iterate over each PATH
    char *token = strtok(paths, ":");
    while (token) {
        size_t len = strlen(token) + 1 + strlen(cmd) + 1;
        char *fullpath = malloc(len);
        if (!fullpath) {
            free(paths);
            return NULL;
        }
        snprintf(fullpath, len, "%s/%s", token, cmd);

        //Check if it's executable
        struct stat st;
        if (stat(fullpath, &st) == 0 && (st.st_mode & S_IXUSR)) {
            free(paths);
            return fullpath;  
        }

        free(fullpath);
        token = strtok(NULL, ":");
    }
    free(paths);
    return NULL; 
}

//Executes a single command
void execute_command(char **argv) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    } else if (pid == 0) {
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
        int status;
        waitpid(pid, &status, 0);
    }
}

//Executes pipelined commands (only worls for 2 commands)
void execute_pipe(char **left_argv, char **right_argv) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        return;
    }

    //Fork for left command
    pid_t left_pid = fork();
    if (left_pid < 0) {
        perror("fork (left)");
        return;
    } else if (left_pid == 0) {

        close(pipefd[0]);                
        dup2(pipefd[1], STDOUT_FILENO); 
        close(pipefd[1]);

        //Execute left command
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

    //Fork for right command
    pid_t right_pid = fork();
    if (right_pid < 0) {
        perror("fork (right)");
        return;
    } else if (right_pid == 0) {
        // Right child
        close(pipefd[1]);              
        dup2(pipefd[0], STDIN_FILENO);  
        close(pipefd[0]);

        //Execute right command
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

    close(pipefd[0]);
    close(pipefd[1]);

    int status;
    waitpid(left_pid, &status, 0);
    waitpid(right_pid, &status, 0);
}