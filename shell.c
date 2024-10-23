#define _POSIX_C_SOURCE 200809L  // Enables POSIX-compliant functions

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>   // For setenv()
#include <string.h>
#include <unistd.h>   // For fork(), execvp(), pipe(), etc.
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>   // For signal handling and kill()
#include <errno.h>

#define MAX_COMMAND_LINE_LEN 1024
#define MAX_COMMAND_LINE_ARGS 128

char prompt[] = "> ";
char delimiters[] = " \t\r\n";
extern char **environ;

void sigint_handler(int sig) {
    printf("\nCaught SIGINT, returning to shell...\n");
}

// Function to execute a command with piping if necessary
void execute_command(char **args, bool has_pipe, char **args_pipe) {
    int pipe_fd[2];  // Pipe file descriptors

    if (has_pipe) {
        if (pipe(pipe_fd) == -1) {
            perror("pipe failed");
            exit(EXIT_FAILURE);
        }
    }

    pid_t pid1 = fork();
    if (pid1 == 0) {
        // First child process - executes the command before the pipe
        if (has_pipe) {
            dup2(pipe_fd[1], STDOUT_FILENO);  // Redirect stdout to the pipe
            close(pipe_fd[0]);  // Close read end of the pipe
            close(pipe_fd[1]);  // Close write end of the pipe after duplication
        }

        if (execvp(args[0], args) == -1) {
            perror("execvp failed");
            exit(EXIT_FAILURE);
        }
    } else if (pid1 < 0) {
        perror("fork failed");
    }

    if (has_pipe) {
        pid_t pid2 = fork();
        if (pid2 == 0) {
            // Second child process - executes the command after the pipe
            dup2(pipe_fd[0], STDIN_FILENO);  // Redirect stdin from the pipe
            close(pipe_fd[1]);  // Close write end of the pipe
            close(pipe_fd[0]);  // Close read end of the pipe after duplication

            if (execvp(args_pipe[0], args_pipe) == -1) {
                perror("execvp failed");
                exit(EXIT_FAILURE);
            }
        } else if (pid2 < 0) {
            perror("fork failed");
        }

        // Parent process: close both ends of the pipe
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        // Wait for both child processes to finish
        waitpid(pid1, NULL, 0);
        waitpid(pid2, NULL, 0);
    } else {
        // Parent process: Wait for the first child to finish (no pipe case)
        waitpid(pid1, NULL, 0);
    }
}

int main() {
    // Stores the string typed into the command line.
    char command_line[MAX_COMMAND_LINE_LEN];
    char cmd_bak[MAX_COMMAND_LINE_LEN];
  
    // Stores the tokenized command line input.
    char *arguments[MAX_COMMAND_LINE_ARGS];
    char *arguments_pipe[MAX_COMMAND_LINE_ARGS];  // For the second command in a pipe

    // Install signal handler for Ctrl+C
    signal(SIGINT, sigint_handler);
  
    while (true) {
        // Print the shell prompt, showing the current working directory
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s%s", cwd, prompt);
        } else {
            perror("getcwd() error");
        }
        fflush(stdout);

        // Read input from stdin and store it in command_line
        if ((fgets(command_line, MAX_COMMAND_LINE_LEN, stdin) == NULL) && ferror(stdin)) {
            fprintf(stderr, "fgets error");
            exit(0);
        }

        // Handle empty input (just ENTER pressed)
        if (command_line[0] == 0x0A) {
            continue;  // Skip to the next iteration if input is just ENTER
        }

        // If the user input was EOF (ctrl+d), exit the shell
        if (feof(stdin)) {
            printf("\n");
            fflush(stdout);
            fflush(stderr);
            return 0;
        }

        // Make a backup of the command line
        strcpy(cmd_bak, command_line);

        // Tokenize the command line input
        int arg_count = 0;
        arguments[arg_count] = strtok(command_line, delimiters);
        while (arguments[arg_count] != NULL) {
            arg_count++;
            arguments[arg_count] = strtok(NULL, delimiters);
        }

        // If no command is entered, continue
        if (arguments[0] == NULL) {
            continue;
        }

        // Detect and handle piping
        bool has_pipe = false;
        int pipe_index = -1;
        for (int i = 0; i < arg_count; i++) {
            if (strcmp(arguments[i], "|") == 0) {
                has_pipe = true;
                pipe_index = i;
                break;
            }
        }

        if (has_pipe) {
            arguments[pipe_index] = NULL;  // Split the command at the pipe
            int j = 0;
            for (int i = pipe_index + 1; i < arg_count; i++) {
                arguments_pipe[j++] = arguments[i];
            }
            arguments_pipe[j] = NULL;
        }

        // Implement Built-In Commands: cd, pwd, exit
        if (strcmp(arguments[0], "exit") == 0) {
            return 0;  // Exit the shell
        } else if (strcmp(arguments[0], "cd") == 0) {
            if (arguments[1] == NULL) {
                fprintf(stderr, "cd: expected argument\n");
            } else {
                if (chdir(arguments[1]) != 0) {
                    perror("cd failed");
                }
            }
            continue;  // Skip creating a child process for built-in commands
        } else if (strcmp(arguments[0], "pwd") == 0) {
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
            } else {
                perror("pwd error");
            }
            continue;
        }

        // Handle echo $VAR (environment variables)
        if (strcmp(arguments[0], "echo") == 0 && arguments[1] && arguments[1][0] == '$') {
            char *var = getenv(arguments[1] + 1);  // Skip the '$'
            if (var) {
                printf("%s\n", var);
            } else {
                printf("\n");
            }
            continue;
        }

        // Handle env (print environment variables)
        if (strcmp(arguments[0], "env") == 0) {
            char **env;
            for (env = environ; *env != 0; env++) {
                printf("%s\n", *env);
            }
            continue;
        }

        // Handle setenv VAR value
        if (strcmp(arguments[0], "setenv") == 0 && arguments[1] && arguments[2]) {
            if (setenv(arguments[1], arguments[2], 1) != 0) {
                perror("setenv failed");
            }
            continue;
        }

        // Execute commands (with or without pipes)
        execute_command(arguments, has_pipe, arguments_pipe);
    }
    return 0;
}