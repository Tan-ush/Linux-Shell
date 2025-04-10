# Linux-Shell

## Overview
`mysh` is a simple, fully functional shell program written in C, implementing a wide range of commands and features typically found in Linux shells. This project simulates key shell functionalities, including process management, environment variables, file system operations, piping, background processes, and network communication.

## Features
- **Built-in Commands**:
  - `echo [string]` - Outputs the given string to stdout.
  - `exit` - Terminates the shell.
  - `ls [path]` - Lists directory contents with optional recursion and depth options.
  - `cd [path]` - Changes the current working directory.
  - `cat [file]` - Displays the contents of a file.
  - `wc [file]` - Counts the number of lines, words, and characters in a file.
  - `kill [pid] [signum]` - Sends a signal to a process.
  - `ps` - Lists processes launched by the shell.
  
- **Process Management**:
  - **Background Processes** (`&`) - Run processes in the background.
  - **Pipes** (`|`) - Connect the output of one command to the input of another.

- **Environment Variables**:
  - Support for setting and expanding environment variables in shell commands.

- **Network Support**:
  - **start-server [port]** - Starts a server on a specified port.
  - **send [hostname] [port] [message]** - Sends a message to a server.
  - **start-client [hostname] [port]** - Starts a client that sends messages to the server.
