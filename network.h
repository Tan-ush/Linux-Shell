#ifndef NETWORK_H
#define NETWORK_H

// Network constants
#define MAX_BACKLOG 10       // Maximum pending connections
#define MAX_CLIENTS 100      // Maximum simultaneous clients
#define BUF_SIZE 1024        // Buffer size for network I/O
#define MAX_NAME 32          // Maximum username length
#define MAX_USER_MSG (BUF_SIZE - MAX_NAME - 3) // Max message size after formatting

// Client connection structure
struct client_info {
    int sock_fd;
    int id;
    char username[MAX_NAME];
    struct client_info *next;
};

// Function declarations
int setup_server_socket(int port);
int write_to_socket(int sock_fd, const char *msg);
int read_from_socket(int sock_fd, char *buf, int *inbuf);
void broadcast_message(char *msg);
void remove_client(struct client_info *client);

#endif