//
//  LinkedList.c
//  OpenDMX
//
//  Created by Samuel Dewan on 2016-12-09.
//  Copyright © 2016 Samuel Dewan. All rights reserved.
//

#include "LinkedList.h"

static struct list_node *list_get_node (const struct list *list, const int index) {
    if ((list->length == 0) || (index >= list->length)) {
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
    char *value;
    if ((value = (char*)malloc(length * sizeof(char))) == NULL) {
        goto error_value_malloc;
    }
    struct list_node *node;
    if ((node = (struct list_node*)malloc(sizeof(struct list_node))) == NULL) {
        goto error_node_malloc;
    }
    
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
    
error_node_malloc:
    free(node);
error_value_malloc:
    free(value);
    return NULL;
}

char *list_add (struct list *list, int index, int length) {
    if (index == list->length) {
        return list_append(list, length); // Trying to add to the end of the list
    }
    struct list_node *node_at_index;
    if ((node_at_index = list_get_node(list, index)) == NULL) {
        goto error; // Trying to add to an index which is not in the list
    }
    char *value;
    if ((value = (char*)malloc(length * sizeof(char))) == NULL) {
        goto error_value_malloc;
    }
    struct list_node *node;
    if ((node = (struct list_node*)malloc(sizeof(struct list_node))) == NULL) {
        goto error_node_malloc;
    }
    
    node->value = value;
    
    node_at_index->prev->next = node;
    node->prev = node_at_index->prev;
    node_at_index->prev = node;
    node->next = node_at_index;
    
    if (index == 0) {
        list->first = node;
    }
    
    list->length++;
    return value;
    
error_node_malloc:
    free(node);
error_value_malloc:
    free(value);
error:
    return NULL;
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

int list_array (const struct list *list, char **buffer, int size) {
    struct list_node *node = list->first;
    if (node == NULL) { return 0; }
    
    buffer[0] = node->value;
    node = node->next;
    int i = 1;
    for (; (i < size) && (node != list->first); i++, node = node->next) {
        buffer[i] = node->value;
    }
    return i;
}

int list_free (struct list *list) {
    while (list->length > 0) {
        list_remove(list, 0);
    }
    return 0;
}

int list_array_freeing (struct list *list, char **buffer, int size) {
    int length = list->length;
    if (length == 0) { return 0; }
    
    int i = 0;
    for (; (i < size) && (list->length > 0); i++) {
        buffer[i] = list_pop(list, 0);
    }
    return i;
}


// MARK: Iterator
struct list_iterator *list_iterator (const struct list *list) {
    struct list_iterator *itt = (struct list_iterator *)malloc(sizeof(struct list_iterator));
    itt->next = list->first;
    itt->first = list->first;
    return itt;
}

char *list_iterator_next (struct list_iterator *iter) {
    if (iter->next == NULL) return NULL;
    char *value = iter->next->value;
    iter->next = (iter->next->next != iter->first) ? iter->next->next : NULL;
    return value;
}

int list_iterator_has_next (const struct list_iterator *iter) {
    return iter->next != NULL;
}
