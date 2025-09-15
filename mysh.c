#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "builtins.h"
#include "io_helpers.h"
#include "variables.h"
#include <stdbool.h>


#define MAX_CLIENTS 100

struct server_state {
    int sock_fd;
    int port;
    pid_t pid;
    int client_count;
    struct client_info *clients;
};

struct client_info {
    int sock_fd;
    int id;
    char username[MAX_STR_LEN];
    struct client_info *next;
};

struct client_state {
    int sock_fd;
    int id;
    char hostname[MAX_STR_LEN];
    int port;
};

struct server_state active_server = { -1, -1, -1, 0, NULL};
struct client_state active_client = { -1, -1, "", -1 };


#define MAX_BG_PROCS 500
pid_t bg_procs[MAX_BG_PROCS];
char bg_commands[MAX_BG_PROCS][MAX_STR_LEN]; 
char entirebg_commands[MAX_BG_PROCS][MAX_STR_LEN];
size_t bg_count = 0;

void execute_pipes(char **tokens, bool is_background);
void execute_single_cmd(char **tokens, bool is_background);

pid_t child_pid = -1;

// Custom signal handler for SIGINT
void handle_sigint() {
    // Print a new prompt
    display_message("\nmysh$ ");
    return;
}

void handle_sigchld(int sig) {
    (void)sig;  // Suppress unused parameter warning
    int status;
    pid_t pid;

    // Reap all finished background processes
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {

        for (size_t i = 0; i < bg_count; i++) {

            if (bg_procs[i] == pid) {


                char msg[MAX_STR_LEN * 2];
                // Include the command name in the message
                snprintf(msg, sizeof(msg), "[%zu]+  Done %s\n", i + 1, entirebg_commands[i]);
                display_message(msg);

                for (size_t j = i; j < bg_count - 1; j++) {
                    bg_procs[j] = bg_procs[j + 1];
                    strncpy(bg_commands[j], bg_commands[j + 1], MAX_STR_LEN);
                }

                bg_count--;
                break;

            }
        }
    }
}


// You can remove __attribute__((unused)) once argc and argv are used.
int main(__attribute__((unused)) int argc, 
         __attribute__((unused)) char* argv[]) {
      
    
    
    char *prompt = "mysh$ ";
    signal(SIGINT, handle_sigint);   // Ignore Ctrl+C in the shell
    signal(SIGCHLD, handle_sigchld); 

    char input_buf[MAX_STR_LEN + 1];
    input_buf[MAX_STR_LEN] = '\0';
    char *token_arr[MAX_STR_LEN] = {NULL};

    while (1) {

        bool has_pipe = false;
        for (size_t i = 0; token_arr[i]; i++) {
            if (strcmp(token_arr[i], "|") == 0) {
                has_pipe = true;
                break;
            }
        }

        // Handle variable assignments ONLY if there is no pipe
        if (!has_pipe && token_arr[0] != NULL && strchr(token_arr[0], '=')) {
            if (token_arr[0][0] == '=' || strlen(token_arr[0]) > MAX_STR_LEN) {
                display_error("ERROR: Invalid variable assignment: ", token_arr[0]);
                continue;
            }
            char *equal_sign = strchr(token_arr[0], '=');
            *equal_sign = '\0';
            char *name = token_arr[0];
            char *value = equal_sign + 1;

            if (name && value) {
                if (value[0] == '$') {
                    char *expanded = get_envar(value + 1);
                    set_envar(name, expanded);
                } else {
                    set_envar(name, value);
                }
            } else {
                display_error("ERROR: Invalid variable assignment: ", token_arr[0]);
            }
            continue;
        }
        for (size_t i = 0; i < token_count; i++){
            if (strchr(token_arr[i], '$')){
                // Accumulating string
                char final[MAX_STR_LEN + 1] = "";

                char *point_string = token_arr[i];

                while(*point_string){
                    // Checks for any variable expansion
                    if (*point_string == '$'){
                        point_string++;
                        // Ensures the variable has more characters after $, adds to final 
                        if (*point_string == '\0' || *point_string == ' ') {  
                            strncat(final, "$", MAX_STR_LEN - strlen(final));
                            continue;
                        }

                        // Keep iterating until you see another expansion, space, or run out of space (I doubt this can happen)
                        char var[MAX_STR_LEN];
                        size_t j = 0;
                        while(*point_string && *point_string != '$' && *point_string != ' ' && j < MAX_STR_LEN - 1){
                            var[j++] = *point_string++;
                        }
                        // End the var variable and check if the value for this "variable" exists
                        var[j] = '\0';
                        char *var_value = get_envar(var);

                        // If not a real variable, add to final the blank string
                        if (var_value == NULL) {
                            var_value = "";
                        }
                        strncat(final, var_value, MAX_STR_LEN - strlen(final) - 1);

                    } else {
                        // For single character inputs like a$hi, where a is the current char
                        char temp[2] = {*point_string, '\0'};
                        strncat(final, temp, MAX_STR_LEN - strlen(final) - 1);
                        point_string++;
                    }
                }
                // Make a copy since this can get lost, I think
                token_arr[i] = strdup(final);
            }
        }

        // New command execution for pipes and forks
        bool is_background = false;
        // Check for &, and flag/remove
        if (token_count > 0 && strcmp(token_arr[token_count-1], "&") == 0) {
            is_background = true;
            token_arr[token_count-1] = NULL;
            token_count--;
        }
        //Check for pipe and execute

        if (has_pipe) {
            execute_pipes(token_arr, is_background);
        } else {
            execute_single_cmd(token_arr, is_background);
        }

        // Essentially frees every malloc'd token
        for (size_t i = 0; i < token_count; i++) {
                if (token_arr[i] != NULL) {
                    // If any token points from the original input buff, this token was not modified, frees otherwise
                    if (token_arr[i] < input_buf
                        || token_arr[i] >= (input_buf + MAX_STR_LEN + 1)) {
                        free(token_arr[i]);
                    }
                    // To avoid any small error and memory accessing
                    token_arr[i] = NULL;
                }
            }
    }
    return 0;
}

void execute_single_cmd(char **tokens, bool is_background){
    if (strcmp(tokens[0], "cd") == 0 || strcmp(tokens[0], "start-server") == 0 || strcmp(tokens[0], "close-server") == 0 || strcmp(tokens[0], "start-client") == 0) {
        // Execute 'cd' directly in the parent process
        bn_ptr builtin = check_builtin(tokens[0]);
        if (builtin) {
            ssize_t err = builtin(tokens); 
            if (err == -1) {
                display_error("ERROR: Builtin failed: ", tokens[0]); 
            }
        }
        return;
    }
    

    pid_t pid = fork();
    //child
    if (pid == 0){
        
        bn_ptr builtin = check_builtin(tokens[0]);
        if (builtin) {
            builtin(tokens);
            exit(0);
        }
        execvp(tokens[0], tokens);
        display_error("ERROR: Unknown command: ", tokens[0]);
        exit(1);
    } else if (pid > 0){
        if (is_background) {
            if (bg_count < MAX_BG_PROCS) {
                
                bg_procs[bg_count] = pid;
                // Store the command name
                snprintf(bg_commands[bg_count], MAX_STR_LEN, "%s", tokens[0]);
                entirebg_commands[bg_count][0] = '\0';
                for (char **t = tokens; *t != NULL; t++) {
                    strncat(entirebg_commands[bg_count], *t, MAX_STR_LEN - strlen(entirebg_commands[bg_count]) - 1);
                    strncat(entirebg_commands[bg_count], " ", MAX_STR_LEN - strlen(entirebg_commands[bg_count]) - 1);
                    
                    if (*(t + 1) != NULL && strcmp(*(t + 1), "|") == 0) {
                        strncat(entirebg_commands[bg_count], "| ", MAX_STR_LEN - strlen(entirebg_commands[bg_count]) - 1);
                    }
                }
                bg_count++;
                char msg[MAX_STR_LEN];
                snprintf(msg, sizeof(msg), "[%zu] %d\n", bg_count, pid);
                display_message(msg);
            }
        } else {
            waitpid(pid, NULL, 0);
        }
    } else{
        display_error("ERROR: Fork failed", "");
    }

}

void execute_pipes(char **tokens, bool is_background) {
    int fd[2];
    int prev_read = -1;
    pid_t pid;
    char **cmd = tokens;
    char last_command[MAX_STR_LEN] = "";

    char full_command[MAX_STR_LEN] = "";
    for (char **t = tokens; *t != NULL; t++) {
        strncat(full_command, *t, MAX_STR_LEN - strlen(full_command) - 1);
        strncat(full_command, " ", MAX_STR_LEN - strlen(full_command) - 1);
    }
    // Remove the trailing space (if any)
    if (strlen(full_command)) {
        full_command[strlen(full_command) - 1] = '\0';
    }

    

    while (1){
        // To find next command
        char **next = cmd;
        while (*next && strcmp(*next, "|") != 0){
            next++;
        } 
        bool last = !*next;
        // Make | into NULL
        *next = NULL;


        // Store the last command name in the pipeline
        if (last) {
            strncpy(last_command, cmd[0], MAX_STR_LEN);
        }

        if (strcmp(cmd[0], "cd") == 0) {
            // If 'cd' is part of a pipe, execute it in a child process
            pid = fork();
            if (pid == 0) {
                // Child process
                bn_ptr builtin = check_builtin(cmd[0]);
                if (builtin) {
                    builtin(cmd); // Execute 'cd' in the child process
                }
                exit(0); // Exit the child process
            } else if (pid > 0) {
                // Parent process
                if (!is_background) {
                    waitpid(pid, NULL, 0); // Wait for the child process to finish
                }
            } else {
                display_error("ERROR: Fork failed", "");
            }

            // Skip forking for 'cd' since it must not affect the parent shell
            if (last) {
                break; // Exit the loop if this is the last command
            }
            cmd = next + 1; // Move to the next command in the pipeline
            continue; // Skip the rest of the loop for 'cd'
        }

        // Initialize pipe
        if (pipe(fd) == -1) {
            display_error("ERROR: Pipe creation failed", "");
            return;
        }

        pid = fork();

        if (pid == 0){
            // If there was a command before, make the input of the pipe the command
            if (prev_read != -1){
                dup2(prev_read, STDIN_FILENO);
            }
            // If this is NOT the last command, change the write to the pipe
            if (!last) {
                dup2(fd[1], STDOUT_FILENO);
            }

            close(fd[0]);
            close(fd[1]);

            if (strchr(cmd[0], '=') != NULL) {
                char *equal_sign = strchr(cmd[0], '=');
                *equal_sign = '\0';
                char *name = cmd[0];
                char *value = equal_sign + 1;

                if (name && value) {
                    if (value[0] == '$') {
                        char *expanded = get_envar(value + 1);
                        set_envar(name, expanded);
                    } else {
                        set_envar(name, value);
                    }
                }
                exit(0); // Child exits after setting the variable
            }
            
            // Command execution 
            bn_ptr builtin = check_builtin(cmd[0]);
            if (is_background && last && builtin){
                display_message("\n");
            }
            if (builtin) {
                ssize_t err = builtin(cmd);
                if (err == -1) {
                    // Builtin failed
                    display_error("ERROR: Builtin failed: ", cmd[0]);
                    exit(1); // Exit with non-zero status
                }
                if (is_background && last){
                    display_message("mysh$ ");
                }
                exit(0);  // Exit child process
            } else {
                execvp(cmd[0], cmd);  // Execute external command
                display_error("ERROR: Unknown command: ", cmd[0]);
                exit(1);  // Exit child process on error
            }

        }
        //Parent
        close(fd[1]);
        if (prev_read != -1){
            close(prev_read);
        } 
        prev_read = fd[0]; // Saving the read for the next command

        if (last){
            break;
        }
        cmd = next + 1; // move to next command
    }
    if (prev_read != -1) {
        close(prev_read);  // Close the last pipe's read end
    }

    if (!is_background) {
        while (wait(NULL) > 0);  // Wait for all child processes
    } else {
        // Track the last process in the pipeline as a background job
        if (bg_count < MAX_BG_PROCS) {
            bg_procs[bg_count] = pid;
            // Store the last command name in the pipeline
            strncpy(bg_commands[bg_count], last_command, MAX_STR_LEN);
            strncpy(entirebg_commands[bg_count], full_command, MAX_STR_LEN);
            bg_count++;
            char msg[MAX_STR_LEN];
            snprintf(msg, sizeof(msg), "[%zu] %d\n", bg_count, pid);
            display_message(msg);
        }
    }





}


