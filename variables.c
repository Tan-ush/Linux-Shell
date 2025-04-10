#include "variables.h" 
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Create a head for the beginning of the linked list
Envar *env_head = NULL;

void set_envar(char *name, char *value){
    // Checks if the name or value is not NULL, redundant but precautionary
    if (name == NULL || value == NULL) {
        return;
    }
    // Iterate through the stuct until a name is found
    struct Envar *cur = env_head;
    while (cur != NULL){
        if (strcmp(name, cur->name) == 0){
            free(cur->value);
            cur->value = strdup(value);
            return;
        }
        cur = cur->next;
    }
    if (cur == NULL){
        // When none is found, we add the new name and value to the front of the list
        struct Envar *new_var = malloc(sizeof(struct Envar));
        new_var->name = strdup(name);
        new_var->value = strdup(value);
        new_var->next = env_head;
        env_head = new_var;
    }
}

// Retrieve an environment variable
char * get_envar(char *name){
    // Redundant but good to keep I guess
    if (name == NULL) return "";

    // Iterate until name is found
    struct Envar *cur = env_head;
    while(cur != NULL){
        if(strcmp(cur->name, name) == 0){
            return cur->value;
        }
        cur = cur->next;
    }
    // For no defined envar
    return "";
}

void free_vars(){
    struct Envar *cur = env_head;
    while (cur != NULL){
        struct Envar *temp = cur;
        cur = cur->next;
        free(temp->name);
        free(temp->value);
        free(temp);
    }
    env_head = NULL;

}