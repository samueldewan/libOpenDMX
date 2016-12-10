//
//  linkedList.c
//  libOpenDMX
//
//  Created by Samuel Dewan on 2016-12-09.
//  Copyright © 2016 Samuel Dewan. All rights reserved.
//

#include "linkedList.h"

static struct list_node *list_get_node (const struct list *list, const int index) {
    if (index >= list->length) {
        return NULL;
    }
    
    struct list_node *node = list->first;
    for (int i = 0; i < index; i++) {
        node = node->next;
    }
    return node;
}

char *list_get (const struct list *list, const int index) {
    struct list_node *node = list_get_node(list, index);
    return node->value;
}

char *list_append (struct list *list, int length) {
    struct list_node *node = (struct list_node*)malloc(sizeof(struct list_node));
    char *value = (char*)malloc(length * sizeof(char*));
    node->value = value;
    if (list->first == NULL) {
        // The list is empty, all pointers are to this node
        list->first = node;
        node->prev = node;
        node->next = node;
    } else {
        // The list already has elements, pointers of existing elements need to be updated
        node->prev = list->first->prev;
        node->next = list->first;
        list->first->prev->next = node;
        list->first->prev = node;
    }
    list->length++;
    return value;
}

static char *list_unchain_node (struct list_node *node) {
    char *value = node->value;
    node->next->prev = node->prev;
    node->prev->next = node->next;
    free(node);
    return value;
}


char *list_pop (struct list *list, const int index) {
    struct list_node *node = list_get_node(list, index);
    if (node == list->first) {
        list->first = node->next;
    }
    list->length--;
    return list_unchain_node(node);
}

void list_remove (struct list *list, const int index) {
    free(list_pop(list, index));
}

void list_array (const struct list *list, char **buffer, int size) {
    struct list_node *node = list->first;
    if (node == NULL) { return; }
    
    buffer[0] = node->value;
    node = node->next;
    for (int i = 1; (i < size) && (node != list->first); i++, node = node->next) {
        buffer[i] = node->value;
    }
}

int list_free (struct list *list) {
    while (list->length > 0) {
        list_remove(list, 0);
    }
    return 0;
}

void list_array_freeing (struct list *list, char **buffer, int size) {
    int length = list->length;
    if (length == 0) { return; }
    
    for (int i = 0; (i < size) && (list->length > 0); i++) {
        buffer[i] = list_pop(list, 0);
    }
}
