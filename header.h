//Mohammad Hammad Arif s3668630
#ifndef SHELL_H
#define SHELL_H

//System headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/stat.h>

//Constants
#define MAX_CMD_LEN 256
#define MAX_TOKENS 64

//Tokenizer that takes a line and returns an array of tokens
int tokenize(char *line, char *argv[]);
//Checks if the input command is a builtin command
int is_builtin(char **argv);
//Executes the command
void execute_builtin(char **argv);
//Finds the full path of an executable
char* find_executable(char *cmd);
//Executes a single command
void execute_command(char **argv);
//Executes pipelined commands (only worls for 2 commands)
void execute_pipe(char **left_argv, char **right_argv);

#endif