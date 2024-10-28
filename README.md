# Report on Design choices and Documentation for shell Implementation

The project implements a basic shell in C that can handle shell commands, built-in commands (cd, pwd, exit), environment variable manipulation (echo, env, setenv), and piping two commands. The program is designed to be POSIX- compliant taking advantage of system calls and standard C libraries for providing a minimalistic, efficient interface for command execution, signal handling, and process control.


## Design Choices 

These instructions will give you a copy of the project up and running on
your local machine for development and testing purposes. See deployment
for notes on deploying the project on a live system.

### POSIX standards 

The program defines _POSIX_C_SOURCE ensuring the use of POSIX-compliant functions and avoiding non-standard extensions. It ensures the use of standard system calls and libraries while enhancing portability across UNIX-like operating systems like fork(), execvp(), pipe(), and waitpid(), which are central to the shell's operation.

### Command Parsing and Tokenization 

The user input is tokenized using the standard C library function strtok() splitting the command line into arguments based on specified delimiters. This approach enables the parsing of commands and arguments while handling typical shell input behaviour. 

### Signal Handling 

The shell installs a signal handler to catch the SIGIT signal (triggered by Ctrl+C) ensuring that the shell process is not interrupted. This handler prints the message and returns control to the shell prompt instead of terminating the shell. 

### Built-in commands 

The shell includes built-in commands which are handled without creating child processes. 
  Cd: Changes the current directory
  Pwd: Prints the current working directory 
  Exit: Exits the shell
  Echo: Displays the environment variables 
  Env: Lists all environment variables

### Piping support 

The implementation supports piping by checking for the | character in the command line. If detected, the command is split into two parts, and a pipe is created using the pipe() system call. This allows the output of the first command to be used as the input for the second command, showcasing a fundamental feature of shell environments.

### Error handling 

The implementation includes error handling for system calls such as fork(), execvp(), and pipe(). This is crucial for robustness, as it allows the shell to handle failures gracefully and provide feedback to the user.


## Documentation 

The code is structured into distinct sections, each serving a specific purpose:
  - Definitions: Necessary headers and constants are defined at the top of the file.
  - Signal Handler: The sigint_handler function defines the behavior upon receiving ‘SIGINT’.
  - Command Execution Function: The execute_command function handles the logic for executing commands, including piping.
  - Main Function: The ‘main’ function contains the shell loop, which manages user input, command parsing, and execution


## Conclusion 

This shell implementation emphasizes modularity, usability, and robustness while offering a simple yet effective command-line interface. Compatibility with UNIX-like systems is ensured by the design's adherence to POSIX standards and implementation of crucial features including command execution, built-in commands, environment variable handling, and piping. Both functionality and user experience are improved with the addition of thorough error handling and intuitive prompts. The shell could become much more powerful and adaptable in the future with the addition of advanced capabilities like job control, command history, background processes, and more complex error reporting.
