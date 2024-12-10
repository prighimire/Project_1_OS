#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_COMMAND_LINE_LEN 1024
#define MAX_COMMAND_LINE_ARGS 128

char prompt[] = "> ";
char delimiters[] = " \t\r\n";
extern char **environ;
pid_t foreground_pid = -1;

void print_prompt();
void tokenize_command(char *command_line, char *arguments[], int *arg_count);
bool builtin_commands(char *arguments[]);
void create_child_process(char *arguments[], bool is_background);
void execute_redirection(char *arguments[], int arg_count);
void execute_pipe(char *arguments[], int arg_count);
void handle_sigint(int sig);
void handle_timer_alarm(int sig);


int main() {
    char command_line[MAX_COMMAND_LINE_LEN];
    char *arguments[MAX_COMMAND_LINE_ARGS];
    int arg_count;

    signal(SIGINT, handle_sigint);
    signal(SIGALRM, handle_timer_alarm);
    
    while (true) {

        do {
            print_prompt();
            if ((fgets(command_line, MAX_COMMAND_LINE_LEN, stdin) == NULL)) {
                if (feof(stdin)) {
                    printf("\n");
                    fflush(stdout);
                    fflush(stderr);
                    return 0;
                }
                fprintf(stderr, "fgets error\n");
                exit(1);
            }
        } while (command_line[0] == 0x0A); 
        
        tokenize_command(command_line, arguments, &arg_count);

        if (builtin_commands(arguments)) {
            continue;
        }

        bool is_background = false;
        if (arg_count > 0) {
            int last_arg_len = strlen(arguments[arg_count - 1]);
            if (last_arg_len > 0 && arguments[arg_count - 1][last_arg_len - 1] == '&') {
                is_background = true;
                arguments[arg_count - 1][last_arg_len - 1] = '\0';
            }
        }

        create_child_process(arguments, is_background);
    }
    return -1;
}

void print_prompt() {
    char cur_dir[MAX_COMMAND_LINE_LEN];
    char *cur_user = getenv("USER");
    char cur_hostname[MAX_COMMAND_LINE_LEN];
    gethostname(cur_hostname, sizeof(cur_hostname));
    
    getcwd(cur_dir, sizeof(cur_dir));
    if (cur_dir != NULL) {
        printf("\033[31mQuash Shell \033[0m%s@%s >> %s >> ", cur_user, cur_hostname, cur_dir);
    } else {
        perror("Error with getcwd");
    }
    fflush(stdout);
}

void tokenize_command(char *command_line, char *arguments[], int *arg_count) {
    char *token;
    int i = 0;
    bool in_quotes = false;
    char quote_char = '\0';
    char *start = command_line;
    char buffer[MAX_COMMAND_LINE_LEN];
    int buf_pos = 0;

    while (*command_line != '\0' && i < MAX_COMMAND_LINE_ARGS - 1) {

        while (*command_line == ' ' || *command_line == '\t') {
            command_line++;
        }
        
        if (*command_line == '\0' || *command_line == '\n') {
            break;
        }

        start = command_line;
        buf_pos = 0;
        
        while (*command_line != '\0' && *command_line != '\n') {
            if (!in_quotes) {
                if (*command_line == '"' || *command_line == '\'') {
                    in_quotes = true;
                    quote_char = *command_line;
                    command_line++;
                    continue;
                }
                if (*command_line == ' ' || *command_line == '\t') {
                    break;
                }
            } else {
                if (*command_line == quote_char) {
                    in_quotes = false;
                    command_line++;
                    continue;
                }
            }
            buffer[buf_pos++] = *command_line++;
        }
        
        buffer[buf_pos] = '\0';
        
        if (buffer[0] == '$') {
            char *env_value = getenv(buffer + 1);
            arguments[i] = env_value ? strdup(env_value) : strdup("");
        } else {
            arguments[i] = strdup(buffer);
        }
        
        i++;
        
        if (*command_line == '\0' || *command_line == '\n') {
            break;
        }
    }
    
    arguments[i] = NULL;
    *arg_count = i;
}


bool builtin_commands(char *arguments[]) {
    int j;
    char **env;
    
    if (strcmp(arguments[0], "cd") == 0) {
        if (arguments[1]) {
            if (chdir(arguments[1]) != 0) {
                perror("Error with chdir");
            }
        } else {
            chdir(getenv("HOME"));
        }
        return true;
    } else if (strcmp(arguments[0], "pwd") == 0) {
        char cur_dir[MAX_COMMAND_LINE_LEN];
        getcwd(cur_dir, sizeof(cur_dir));
        if ( cur_dir != NULL) {
            printf("%s\n", cur_dir);
        } else {
            perror("Error with getcwd");
        }
        return true;
    } else if (strcmp(arguments[0], "echo") == 0) {
        for (j = 1; arguments[j] != NULL; j++) {
            if (arguments[j][0] == '$') {
                char *env_val = getenv(arguments[j] + 1);
                printf("%s ", env_val ? env_val : "");
            } else {
                printf("%s ", arguments[j]);
            }
        }
        printf("\n");
        return true;
    } else if (strcmp(arguments[0], "exit") == 0) {
        exit(0);
    } else if (strcmp(arguments[0], "env") == 0) {
        for (env = environ; *env != NULL; env++) {
            printf("%s\n", *env);
        }
        return true;
    } else if (strcmp(arguments[0], "setenv") == 0) {
    if (arguments[1] && strchr(arguments[1], '=')) {
        char *name = strtok(arguments[1], "=");
        char *value = strtok(NULL, "=");

        if (name && value) {
            setenv(name, value, 1);
        } else {
            printf("Error with setenv: invalid format, use VAR=VALUE\n");
        }
    } else {
        printf("Error with setenv: missing or invalid arguments\n");
    }
    return true;
}
    return false;
}

void create_child_process(char *arguments[], bool is_background) {
    int i;
    for (i = 0; arguments[i] != NULL; i++) {
        if (arguments[i][0] == '>' || arguments[i][0] == '<') {
            execute_redirection(arguments, i);
            return;
        }
        if (arguments[i][0] == '|') {
            execute_pipe(arguments, i);
            return;
        }
    }

    pid_t pid = fork();

    if (pid == 0) {  
        if (execvp(arguments[0], arguments) == -1) {
            perror("execvp error");
            exit(1);
        }
    } else if (pid > 0) { 
        if (!is_background) {
            foreground_pid = pid;   
            alarm(10);              
            waitpid(pid, NULL, 0);    
            alarm(0);                 
            foreground_pid = -1;      
        }
    } else {
        perror("fork error");
    }
}

void execute_pipe(char *arguments[], int pipe_index) {

    char *cmd1[MAX_COMMAND_LINE_ARGS];
    char *cmd2[MAX_COMMAND_LINE_ARGS];
    
    int i;
    for (i = 0; i < pipe_index; i++) {
        cmd1[i] = arguments[i];
    }
    cmd1[i] = NULL;
    
    int j = 0;
    for (i = pipe_index + 1; arguments[i] != NULL; i++) {
        cmd2[j++] = arguments[i];
    }
    cmd2[j] = NULL;

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return;
    }

    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork failed");
        return;
    }
    
    if (pid1 == 0) {
        close(pipefd[0]);

        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("dup2 failed");
            exit(1);
        }
        close(pipefd[1]);

        if (execvp(cmd1[0], cmd1) == -1) {
            perror("execvp failed for first command");
            exit(1);
        }
    }

    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork failed");
        return;
    }
    
    if (pid2 == 0) {
        close(pipefd[1]);
        
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("dup2 failed");
            exit(1);
        }
        close(pipefd[0]);

        if (execvp(cmd2[0], cmd2) == -1) {
            perror("execvp failed for second command");
            exit(1);
        }
    }

    close(pipefd[0]);
    close(pipefd[1]);

  
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}


void execute_redirection(char *arguments[], int redirect_index) {
    char *file = arguments[redirect_index + 1];
    if (file == NULL) {
        fprintf(stderr, "Error: Missing filename for redirection\n");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) { 
        if (strcmp(arguments[redirect_index], ">") == 0) {
            int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("Failed to open file for output redirection");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        else if (strcmp(arguments[redirect_index], "<") == 0) {
            int fd = open(file, O_RDONLY);
            if (fd == -1) {
                perror("Failed to open file for input redirection");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        arguments[redirect_index] = NULL;
        execvp(arguments[0], arguments);
        perror("execvp error");  
        exit(1);
    } else if (pid > 0) { 
        waitpid(pid, NULL, 0); 
    } else {
        perror("fork error");
    }
}

void handle_timer_alarm(int sig) {
    if (foreground_pid > 0) {  
        printf("\nProcess timed out. Terminating...\n");
        kill(foreground_pid, SIGKILL); 
        foreground_pid = -1;          
        print_prompt();               
        fflush(stdout);
    }
}
void handle_sigint(int sig) {
    printf("\n");
    print_prompt();
    fflush(stdout);
}