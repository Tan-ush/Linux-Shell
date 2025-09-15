#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/select.h>
#include "network.h"
#include "builtins.h"
#include "io_helpers.h"

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

extern struct server_state active_server;


// Sets up a listening socket
int setup_server_socket(int port) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        display_error("ERROR: socket failed", "");
        return -1;
    }

    // Allow port reuse
    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        display_error("ERROR: setsockopt failed", "");
        close(sock_fd);
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr))) {
        display_error("ERROR: bind failed", "");
        close(sock_fd);
        return -1;
    }

    if (listen(sock_fd, 10)) {
        display_error("ERROR: listen failed", "");
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

// Writes to socket with network newline
int write_to_socket(int sock_fd, const char *msg) {
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "%s\r\n", msg);
    return write(sock_fd, buf, strlen(buf)) == -1 ? -1 : 0;
}

// Reads from socket into buffer
int read_from_socket(int sock_fd, char *buf, int *inbuf) {
    // Read exactly BUF_SIZE bytes (including null-terminator space)
    int bytes = read(sock_fd, buf + *inbuf, BUF_SIZE - 1);
    
    if (bytes <= 0) {
        return -1;  // Error or disconnect
    }

    *inbuf += bytes;
    
    // If we filled the buffer, check if the last byte is non-zero (message too long)
    if (*inbuf == BUF_SIZE && buf[BUF_SIZE-1] == '\n') {
        return -1;  // Message too long (disconnect client)
    }

    // Ensure null-termination (if buffer wasn't full)
    buf[*inbuf] = '\0';
    
    return 0;  // Success (or partial read)
}

void broadcast_message(char *msg) {
    struct client_info *curr = active_server.clients;
    while (curr) {
        write_to_socket(curr->sock_fd, msg);
        curr = curr->next;
    }
    // Also display in server console
    display_message(msg);
    display_message("\n");
}

// Remove client from list
void remove_client(struct client_info *client) {
    if (!active_server.clients) return;
    
    if (active_server.clients == client) {
        active_server.clients = client->next;
    } else {
        struct client_info *curr = active_server.clients;
        while (curr->next && curr->next != client) {
            curr = curr->next;
        }
        if (curr->next) {
            curr->next = client->next;
        }
    }
    
    free(client);
    active_server.client_count--;
}
