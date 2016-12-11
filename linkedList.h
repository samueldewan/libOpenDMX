//
//  LinkedList.h
//  OpenDMX
//
//  Created by Samuel Dewan on 2016-12-09.
//  Copyright © 2016 Samuel Dewan. All rights reserved.
//

#ifndef LinkedList_h
#define LinkedList_h

#include <stdlib.h>


struct list {
    struct list_node    *first;
    int                 length;
};

struct list_node {
    struct list_node    *prev;
    char                *value;
    struct list_node    *next;
};

/**
 *  Gets an item from a list.
 *  @param list The list from witch to get the item.
 *  @param index The index of the item to get.
 *  @returns The item at the specified index.
 */
extern char *list_get (const struct list *list, const int index);

/**
 *  Append and item to a list
 *  @param list The list to which the item should be appended.
 *  @param length The length of the cahr array to be appended to the list.
 *  @returns The char array which has been appended to the list.
 */
extern char *list_append (struct list *list, int length);

/**
 *  Pop an item from a list.
 *  @note It is the responsability of the calling function to free the returned pointer. It will no longer be managed by the list.
 *  @param list The list from which the item should be poped.
 *  @param index The index which should be poped from the list.
 *  @returns The value which was poped from the list
 */
extern char *list_pop (struct list *list, const int index);

/**
 *  Remove an item from a list (and free it).
 *  @param list The list from which the item should be removed.
 *  @param index The index which should be removed from the list.
 */
extern void list_remove (struct list *list, const int index);

/**
 *  Generate an array with the contents of a list.
 *  @param list The list from which the array should be created.
 *  @param buffer The array which should contain the items in the list.
 *  @param size The maximum number of items to add to the buffer.
 */
extern void list_array (const struct list *list, char **buffer, int size);

/**
 *  Frees all nodes of a list
 *  @param list The list for which all nodes should be freed
 *  @returns 0 if the list was freed sucesfully.
 */
extern int list_free (struct list *list);

/**
 *  Generate an array with the contents of a list and free the original list. Pops items from list as they are added to the array.
 *  @param list The list from which the array should be created.
 *  @param buffer The array which should contain the items in the list.
 *  @param size The maximum number of items to add to the buffer.
 */
extern void list_array_freeing (struct list *list, char **buffer, int size);


#endif /* LinkedList_h */
