#include <string.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include "commands.h"
#include "builtins.h"
#include "io_helpers.h"
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <netinet/in.h>
#include "network.h"
#include <sys/select.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <limits.h>


#define MAX_CLIENTS 100

struct server_state {
    int sock_fd;
    int port;
    pid_t pid;
    int client_count;
    struct client_info *clients;
};

struct client_state {
    int sock_fd;
    int id;
    char hostname[MAX_STR_LEN];
    int port;
};

extern pid_t bg_procs[];
extern size_t bg_count;
extern char bg_commands[100][MAX_STR_LEN];
extern struct server_state active_server;
extern struct client_state active_client;




// ====== Command execution =====

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error ... but there are no errors on echo. 
 */
ssize_t bn_echo(char **tokens) {
    ssize_t index = 1;
    ssize_t len = 0;

    if (tokens[index] != NULL) {
        // TODO:
        // Implement the echo command
        size_t len_token = strlen(tokens[index]);
        if (len + len_token >= MAX_STR_LEN){
            size_t remaining = MAX_STR_LEN - len;
            write(STDOUT_FILENO, tokens[index], remaining);
            len = MAX_STR_LEN;
        } else {
            display_message(tokens[index]);
            len += len_token;
        }
        index++;

    }
    while (tokens[index] != NULL) {
        // TODO:
        // Implement the echo commande
        size_t len_token = strlen(tokens[index]);
        if (len + 1 + len_token >= MAX_STR_LEN){
            size_t remaining = MAX_STR_LEN - len - 1;
            write(STDOUT_FILENO, tokens[index], remaining);
            len = MAX_STR_LEN;
        } else {
            display_message(" ");
            display_message(tokens[index]);
            len += len_token;
        }
        index++;
    }
    display_message("\n");

    return 0;
}
ssize_t bn_ls(char **tokens) {
    ssize_t recursion_flag = 0, substring_flag = 0, depth_flag = 0, dir_count = 0;
    long depth_amount = 1;
    char *substring = NULL;
    char *directory = ".";

    for (int i = 1; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "--rec") == 0) {
            recursion_flag = 1;
        } 
        else if (strcmp(tokens[i], "--d") == 0) {
            if (!tokens[++i]) {
                display_error("ERROR: --d requires a value", tokens[i-1]);
                goto error;
            }
            char *endptr;
            depth_amount = strtol(tokens[i], &endptr, 10);
            if (*endptr != '\0' || depth_amount <= 0) {
                display_error("ERROR: --d requires a positive integer", tokens[i]);
                goto error;
            }
            depth_flag = 1;
        } 
        else if (strcmp(tokens[i], "--f") == 0) {
            if (!tokens[++i] || tokens[i][0] == '-') {
                display_error("ERROR: --f requires a substring", tokens[i-1]);
                goto error;
            }
            if (substring_flag) free(substring);
            substring = strdup(tokens[i]);
            if (!substring) {
                display_error("ERROR: Memory allocation failed", tokens[0]);
                return -1;
            }
            substring_flag = 1;
        }
        else if (tokens[i][0] == '-') {
            display_error("ERROR: Invalid flag", tokens[i]);
            goto error;
        }
        else {
            if (dir_count++ >= 1) {
                display_error("ERROR: Only one directory allowed", tokens[i]);
                goto error;
            }
            DIR *dir = opendir(tokens[i]);
            if (!dir) {
                display_error("ERROR: Invalid path", tokens[i]);
                display_error("ERROR: Builtin failed: ls", tokens[0]);
                goto error;
            }
            closedir(dir);
            directory = tokens[i];
        }
    }

    if (depth_flag && !recursion_flag) {
        display_error("ERROR: --d requires --rec", tokens[0]);
        goto error;
    }

    if (recursion_flag) {
        recursive_ls(directory, depth_flag ? depth_amount : LONG_MAX, substring);
    } else {
        list_dir(directory, substring);
    }

    if (substring_flag) free(substring);
    return 0;

error:
    if (substring_flag) free(substring);
    return -1;
}


ssize_t bn_cd(char **tokens){
    if (tokens[1] == NULL){
        // No directory
        display_error("Error: Invalid path:", "NULL");
        return -1;
    } else if (tokens[2] != NULL){
        display_error("Error: Invalid path:", "");
        return -1;
    } else if (strcmp(tokens[1], ".") == 0){
        // Current Directory
        return 0;
    } else if (strcmp(tokens[1], "..") == 0){
        // Parent Directory
        if (chdir("..") == -1){
            display_error("Error: Invalid path:", tokens[1]);
            return -1;
        }
    } else if (strcmp(tokens[1], "...") == 0){
        // Parent Directory
        if (chdir("../..") == -1){
            display_error("Error: Invalid path:", tokens[1]);
            return -1;
        }
    } else if (strcmp(tokens[1], "....") == 0){
        // Parent Directory
        if (chdir("../../..") == -1){
            display_error("Error: Invalid path:", tokens[1]);
            return -1;
        }
    } else {
        if (chdir(tokens[1]) == -1){
            display_error("Error: Invalid path:", tokens[1]);
            return -1;
        }
    }
    return 0;
    
}

ssize_t bn_cat(char **tokens){
    FILE *file = stdin;
    ssize_t error;
    char line[81];

    // Check if file provided in second token, if token[1] is not NULL, open or return
    if (tokens[1] != NULL){
        file = fopen(tokens[1], "r");
        if (file == NULL){
            display_error("ERROR: Cannot open file", "");
            return -1;
        }
    }

    if (tokens[1] != NULL && tokens[2] != NULL){
        FILE *newfile = fopen(tokens[2], "r");
        if (newfile != NULL){
            display_error("ERROR: Too many paths", "");
            return -1;
        }
    }

    // Display File
    while (fgets(line, 81, file) != NULL){
        display_message(line);
    }

    // Close if there is a file
    if (file != stdin){
        error = fclose(file);
        if (error != 0){
            display_error("File could not close", tokens[1]);
            return -1;
        }
    }

    return 0;

}


ssize_t bn_wc(char **tokens){
    FILE *file = stdin;
    ssize_t error;
    char line[81];
    ssize_t word_count = 0;
    ssize_t char_count = 0;
    ssize_t newline_count = 0;
    ssize_t word_flag = 0;

    if (tokens[1] != NULL && strchr(tokens[1], '-') == NULL){
        file = fopen(tokens[1], "r");
        if (file == NULL){
            display_error("ERROR: Cannot open file", "");
            return -1;
        }
    }
    if (tokens[1] != NULL && tokens[2] != NULL){
        FILE *newfile = fopen(tokens[2], "r");
        if (newfile != NULL){
            display_error("ERROR: Too many paths", "");
            return -1;
        }
    }

    while (fgets(line, 81, file) != NULL) {
        for (int i = 0; line[i] != '\0'; i++) {
            if (line[i] == '\n') {
                newline_count++;
            }
            if (line[i] == '\n' || line[i] == ' ' || line[i] == '\t' || line[i] == '\r') {
                if (word_flag == 1) {
                    word_count++;
                    word_flag = 0;
                }
            } else {
                word_flag = 1;
            }
            char_count++;
        }
    }
    if (word_flag == 1){
        word_count++;
    }

    if (file != stdin){
        error = fclose(file);
        if (error != 0){
            display_error("File could not close", tokens[1]);
            return -1;
        }
    }
    
    char buff[MAX_STR_LEN];
    display_message("word count ");
    number_to_string(word_count, buff);
    display_message(buff);
    display_message("\n");
    display_message("character count ");
    number_to_string(char_count, buff);
    display_message(buff);
    display_message("\n");
    display_message("newline count ");
    number_to_string(newline_count, buff);
    display_message(buff);
    display_message("\n");

    return 0;
}

ssize_t bn_kill(char **tokens) {
    if (!tokens[1]) {
        display_error("ERROR: Missing PID", "");
        return -1;
    }

    pid_t pid = atoi(tokens[1]);

    if (kill(pid, 0) == -1) {
        display_error("ERROR: The process does not exist", "");
        return -1;
    }

    int sig = SIGTERM;

    if (tokens[2]) {
        sig = atoi(tokens[2]);

        if (sig < 1 || sig > 64) {
            display_error("ERROR: Invalid signal specified", "");
            return -1;
        }
    }

    if (kill(pid, sig) == -1) {
        display_error("ERROR: Failed to send signal to the process", "");
        return -1;
    }

    return 0;
}

ssize_t bn_ps(){
    for (size_t i = 0; i < bg_count; i++){
        char line[MAX_STR_LEN * 2];
        snprintf(line, sizeof(line), "%s %d\n", bg_commands[i], bg_procs[i]);
        display_message(line);
    }
    return 0;
}
ssize_t bn_start_server(char **tokens) {
    if (!tokens[1]) {
        display_error("ERROR: No port provided", "");
        return -1;
    }

    if (active_server.sock_fd != -1) {
        display_error("ERROR: Server already running", "");
        return -1;
    }
    active_server.client_count = 0;

    int port = atoi(tokens[1]);
    if (port <= 0 || port > 65535) {
        display_error("ERROR: Invalid port", "");
        return -1;
    }

    pid_t pid = fork();
    if (pid == 0) { // Server process
        active_server.sock_fd = setup_server_socket(port);
        if (active_server.sock_fd == -1) {
            display_error("ERROR: Builtin failed: start-server", "");
            exit(1);
        }

        fd_set read_fds, active_fds;
        FD_ZERO(&active_fds);
        FD_SET(active_server.sock_fd, &active_fds);

        while (1) {
            read_fds = active_fds;
            if (select(FD_SETSIZE, &read_fds, NULL, NULL, NULL) == -1) {
                if (errno == EINTR) continue;
                break;
            }

            // New connection
            if (FD_ISSET(active_server.sock_fd, &read_fds)) {
                struct sockaddr_in addr;
                socklen_t len = sizeof(addr);
                int client_fd = accept(active_server.sock_fd, (struct sockaddr *)&addr, &len);
                
                if (client_fd != -1) {
                    
                    active_server.client_count++;
                    // Set socket to non-blocking mode
                    int flags = fcntl(client_fd, F_GETFL, 0);
                    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
            
                    // Try to read initial message (won't block)
                    char init_msg[BUF_SIZE];
                    int bytes_read = read(client_fd, init_msg, sizeof(init_msg) - 1);
                    
                    if (bytes_read > 0) {
                        // Handle send command case
                        init_msg[bytes_read] = '\0';
                        broadcast_message(init_msg);
                    } else if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        // Real error occurred
                        close(client_fd);
                        continue;
                    }
                    // For start-client (bytes_read == 0 or EAGAIN), proceed without message
            
                    // Add client to list
                    struct client_info *new = malloc(sizeof(struct client_info));
                    new->sock_fd = client_fd;
                    new->id = active_server.client_count;
                    snprintf(new->username, sizeof(new->username), "client%d", new->id);
                    new->next = active_server.clients;
                    active_server.clients = new;
                    FD_SET(client_fd, &active_fds);

                    char welcome_msg[BUF_SIZE];
                    snprintf(welcome_msg, sizeof(welcome_msg), "client%d", new->id);
                    write(client_fd, welcome_msg, strlen(welcome_msg));
                }
            }

            // Client messages
            struct client_info *curr = active_server.clients;
            while (curr) {
                if (FD_ISSET(curr->sock_fd, &read_fds)) {
                    char buf[BUF_SIZE];
                    int inbuf = 0;
                    int read_result = read_from_socket(curr->sock_fd, buf, &inbuf) == -1;
                    if (read_result == -1) {
                        // Client disconnected
                        close(curr->sock_fd);
                        FD_CLR(curr->sock_fd, &active_fds);
                        struct client_info *to_remove = curr;
                        curr = curr->next;
                        remove_client(to_remove);
                        continue;
                    }
                    
                    // Process message
                    char *msg = strtok(buf, "\r\n");
                    if (msg) {
                        if (strcmp(msg, "\\connected") == 0) {
                            char response[BUF_SIZE];
                            snprintf(response, sizeof(response), 
                                "client%d: %d clients connected", 
                                curr->id, active_server.client_count);
                            write_to_socket(curr->sock_fd, response);
                            continue;  // Skip broadcast for this command
                        }


                        char full_msg[BUF_SIZE];
                        snprintf(full_msg, sizeof(full_msg), "%s", msg);
                        broadcast_message(full_msg);
                    }
                }
                curr = curr->next;
            }
        }
        exit(0);
    } else if (pid > 0) {
        active_server.pid = pid;
        active_server.port = port;
        return 0;
    } else {
        display_error("ERROR: fork failed", "");
        return -1;
    }
}

// Client implementation
ssize_t bn_start_client(char **tokens) {
    if (!tokens[1] || !tokens[2]) {
        display_error("ERROR: Usage: start-client port hostname", "");
        return -1;
    }

    int port = atoi(tokens[1]);
    if (port <= 0 || port > 65535) {
        display_error("ERROR: Invalid port", "");
        return -1;
    }

    active_client.sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (active_client.sock_fd < 0) {
        display_error("ERROR: socket failed", "");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, tokens[2], &addr.sin_addr) <= 0) {
        display_error("ERROR: Invalid hostname", "");
        close(active_client.sock_fd);
        return -1;
    }

    if (connect(active_client.sock_fd, (struct sockaddr *)&addr, sizeof(addr))) {
        display_error("ERROR: connect failed", "");
        close(active_client.sock_fd);
        return -1;
    }
    
    char welcome_msg[BUF_SIZE];
    if (read(active_client.sock_fd, welcome_msg, sizeof(welcome_msg)) > 0) {
        // Parse server's welcome message containing the assigned ID
        sscanf(welcome_msg, "client%d", &active_client.id);
    }
    strncpy(active_client.hostname, tokens[2], MAX_STR_LEN);
    active_client.port = port;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(active_client.sock_fd, &read_fds);

    while (1) {
        fd_set ready = read_fds;
        if (select(active_client.sock_fd + 1, &ready, NULL, NULL, NULL) == -1) {
            if (errno == EINTR) continue;
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &ready)) {
            char input[BUF_SIZE];
            if (!fgets(input, sizeof(input), stdin)) {
                if (feof(stdin)) break;
                display_error("ERROR: read failed", "");
                break;
            }

            input[strcspn(input, "\n")] = '\0'; // Remove newline
            
            if (strcmp(input, "\\connected") == 0) {
                // Send raw command to server (no client ID prepending)
                if (write_to_socket(active_client.sock_fd, "\\connected") == -1) {
                    display_error("ERROR: Failed to send status request", "");
                }
                continue;  // Skip normal message processing
            }

            char msg[BUF_SIZE];
            snprintf(msg, sizeof(msg), "client%d: %.*s", active_client.id, (int)(sizeof(msg) - 10), input);

            if (write_to_socket(active_client.sock_fd, msg) == -1) {
                display_error("ERROR: write failed", "");
                break;
            }
        }

        if (FD_ISSET(active_client.sock_fd, &ready)) {
            char buf[BUF_SIZE];
            int inbuf = 0;
            if (read_from_socket(active_client.sock_fd, buf, &inbuf) == -1) {
                display_error("Server disconnected", "");
                break;
            }
            
            char *msg = strtok(buf, "\r\n");
            if (msg) {
                display_message(msg);
                display_message("\n");
            }
        }
    }

    close(active_client.sock_fd);
    active_client.sock_fd = -1;
    return 0;
}
ssize_t bn_close_server() {

    if (active_server.pid == -1) {
        display_error("ERROR: No server running", "");
        return -1;
    }
    
    if (kill(active_server.pid, SIGTERM) == -1) {
        display_error("ERROR: Failed to stop server", "");
        return -1;
    }
   

    // Clean up clients
    while (active_server.clients) {
        close(active_server.clients->sock_fd);
        struct client_info *next = active_server.clients->next;
        free(active_server.clients);
        active_server.clients = next;
    }
    

    active_server.sock_fd = -1;
    active_server.port = -1;
    active_server.pid = -1;
    active_server.client_count = 0;

    display_message("Server stopped\n");
    return 0;
}

// Send message command
ssize_t bn_send(char **tokens) {
    if (!tokens[1] || !tokens[2] || !tokens[3]) {
        display_error("ERROR: Usage: send port hostname message", "");
        return -1;
    }

    int port = atoi(tokens[1]);
    if (port <= 0 || port > 65535) {
        display_error("ERROR: Invalid port", "");
        return -1;
    }

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        display_error("ERROR: socket failed", "");
        return -1;
    }

    // Set up address (non-blocking for quick send)
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };
    if (inet_pton(AF_INET, tokens[2], &addr.sin_addr) <= 0) {
        display_error("ERROR: Invalid hostname", "");
        close(sock_fd);
        return -1;
    }

    // Connect, send, and close immediately
    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        char msg[BUF_SIZE] = {0};  // Initialize to zero
        for (int i = 3; tokens[i] != NULL; i++) {
            // Check if we have space left (avoid buffer overflow)
            if (strlen(msg) + strlen(tokens[i]) + 1 >= BUF_SIZE) {
                display_error("ERROR: Message too long", "");
                close(sock_fd);
                return -1;
            }
            // Add a space between words (except the first one)
            if (i > 3) {
                strcat(msg, " ");
            }
            strcat(msg, tokens[i]);
        }

        // Send the message (ensure newline termination)
        if (write(sock_fd, msg, strlen(msg)) == -1) {
            display_error("ERROR: Failed to send message", "");
            close(sock_fd);
            return -1;
        }
    } else {
        display_error("ERROR: Failed to connect", "");
    }

    close(sock_fd);
    return 0;
}
