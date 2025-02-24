#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
    // A simple buffer to hold user input; 256 is arbitrary for now
    char cmd[256];

    while (1) {
        // Print prompt
        printf("os1> ");
        fflush(stdout);

        // Read a line of input
        if (fgets(cmd, sizeof(cmd), stdin) == NULL) {
            // If EOF or error, exit the loop
            // (EOF can happen if user presses Ctrl+D, for instance)
            break;
        }

        // Remove trailing newline if present
        cmd[strcspn(cmd, "\n")] = '\0';

        // If the user typed "exit", break
        if (strcmp(cmd, "exit") == 0) {
            break;
        }

        // For now, just echo the command
        // Later, you'll tokenize and execute it
        printf("You typed: %s\n", cmd);

        // TODO:
        // 1) Tokenize 'cmd'
        // 2) Check if built-in commands (cd, exit)
        // 3) Search for executable in PATH if not built-in
        // 4) Use fork(), execv(), wait() to run command
        // 5) Handle pipes
    }

    printf("Exiting OS1 shell.\n");
    return 0;
}
