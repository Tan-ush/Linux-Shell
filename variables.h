#include <stdlib.h>
#include <string.h>
#include <stdio.h>


typedef struct Envar {
    char *name;
    char *value;
    struct Envar *next;
} Envar; 
extern Envar *env_head;

void set_envar(char *name, char *value);
char *get_envar(char *name);
void free_vars();