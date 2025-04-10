#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include "io_helpers.h"

void recursive_ls(char *dir_name, long depth, char *substring);
void list_dir(char *dir_name, char *substring);
void number_to_string(ssize_t num, char *buffer);