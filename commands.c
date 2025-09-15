#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include "commands.h"
#include "io_helpers.h"

void recursive_ls(char *dir_name, long depth, char *substring){
    if (depth == 0){
        return;
    }

    DIR *dir = opendir(dir_name);
    if (dir == NULL){
        display_error("Error: invalid path ", dir_name);
        return;
    }
    struct dirent *current;

    // Check for every subdir if it includes name
    while ((current = readdir(dir)) != NULL){
        if (!substring || strstr(current->d_name, substring) != NULL){
            display_message(current->d_name);
            display_message("\n");
        }
    }
    rewinddir(dir);
    while ((current = readdir(dir)) != NULL){
        if (strcmp(current->d_name, ".") == 0 || strcmp(current->d_name, "..") == 0){
            continue;
        }
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir_name, current->d_name);
        DIR *subdir = opendir(path);
        if (subdir != NULL) {
            closedir(subdir);
            recursive_ls(path, depth - 1, substring);
            continue;
        }
    }

    closedir(dir);
}

void list_dir(char *dir_name, char *substring){
    DIR *dir = opendir(dir_name);
    if (dir == NULL){
        display_error("Error: invalid path ", dir_name);
        return;
    }
    struct dirent *current;
    while ((current = readdir(dir)) != NULL){
        if (!substring || strstr(current->d_name, substring) != NULL){
            display_message(current->d_name);
            display_message("\n");
        }
    }
    closedir(dir);

}

void number_to_string(ssize_t num, char *buffer) {
    ssize_t i = 0;
    ssize_t is_negative = 0;

    if (num == 0) {
        buffer[i++] = '0';
        buffer[i] = '\0';
        return;
    }

    if (num < 0) {
        is_negative = 1;
        num = -num;
    }

    while (num > 0){
        buffer[i++] = (num % 10) + '0'; 
        num /= 10;
    }

    if (is_negative) {
        buffer[i++] = '-';
    }

    for (ssize_t j = 0; j < i / 2; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }

    buffer[i] = '\0';
}
