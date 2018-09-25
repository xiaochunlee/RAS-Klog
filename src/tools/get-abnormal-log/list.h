#ifndef LIB_LIST_H
#define LIB_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

    /****************
     * list functions
     ****************/

    /*
     * A list consists of a list head plus elements.
     * Each element has 'next' and 'previous' pointers.
     * The list head's pointers point to the first and the last element.
     */

    struct list_head {
        struct list_head *next, *prev;
    };

    /*
     * Initialise a list before use.
     * The list head's next and previous pointers point back to itself.
     */
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

    static void INIT_LIST_HEAD(struct list_head *list)
    {
        list->next = list;
        list->prev = list;
    }
    /*
     * Insert a new entry between two known consecutive entries.
     *
     * This is only for internal list manipulation where we know
     * the prev/next entries already!
     */
    static void __list_add(struct list_head *new,
            struct list_head *prev,
            struct list_head *next)
    {
        next->prev = new;
        new->next = next;
        new->prev = prev;
        prev->next = new;
    }
    /**
     * list_add - add a new entry
     * @new: new entry to be added
     * @head: list head to add it after
     *
     * Insert a new entry after the specified head.
     * This is good for implementing stacks.
     */
    static void list_add(struct list_head *new, struct list_head *head)
    {
        __list_add(new, head, head->next);
    }

    /**
     * list_add_tail - add a new entry
     * @new: new entry to be added
     * @head: list head to add it before
     *
     * Insert a new entry before the specified head.
     * This is useful for implementing queues.
     */
    static void list_add_tail(struct list_head *new, struct list_head *head)
    {
        __list_add(new, head->prev, head);
    }


    /*
     * Delete a list entry by making the prev/next entries
     * point to each other.
     *
     * This is only for internal list manipulation where we know
     * the prev/next entries already!
     */
    static void __list_del(struct list_head * prev, struct list_head * next)
    {
        next->prev = prev;
        prev->next = next;
    }

    static inline void list_del(struct list_head *entry)
    {
        __list_del(entry->prev, entry->next);
        entry->next = NULL;
        entry->prev = NULL;
    }
    /**
     * list_del_init - deletes entry from list and reinitialize it.
     * @entry: the element to delete from the list.
     */
    static void list_del_init(struct list_head *entry)
    {
        __list_del(entry->prev, entry->next);
        INIT_LIST_HEAD(entry);
    }

    /**
     * list_move - delete from one list and add as another's head
     * @list: the entry to move
     * @head: the head that will precede our entry
     */
    static void list_move(struct list_head *list, struct list_head *head)
    {
        __list_del(list->prev, list->next);
        list_add(list, head);
    }
    /**
     * list_move_tail - delete from one list and add as another's tail
     * @list: the entry to move
     * @head: the head that will follow our entry
     */
    static void list_move_tail(struct list_head *list,
            struct list_head *head)
    {
        __list_del(list->prev, list->next);
        list_add_tail(list, head);
    }
    /**
     * list_empty - tests whether a list is empty
     * @head: the list to test.
     */
    static int list_empty(const struct list_head *head)
    {
        return head->next == head;
    }

    static inline int list_is_last(const struct list_head *list,
            const struct list_head *head)
    {
        return list->next == head;
    }
    static void __list_splice(const struct list_head *list,
            struct list_head *prev,
            struct list_head *next)
    {
        struct list_head *first = list->next;
        struct list_head *last = list->prev;

        first->prev = prev;
        prev->next = first;

        last->next = next;
        next->prev = last;
    }
    /**
     * list_splice - join two lists, this is designed for stacks
     * @list: the new list to add.
     * @head: the place to add it in the first list.
     */
    static void list_splice(const struct list_head *list,
            struct list_head *head)
    {
        if (!list_empty(list))
            __list_splice(list, head, head->next);
    }

    /**
     * list_entry - get the struct for this entry
     * @ptr:    the &struct list_head pointer.
     * @type:   the type of the struct this is embedded in.
     * @member: the name of the list_struct within the struct.
     */
#define list_entry(ptr, type, member) \
    ((type *)((const char *)(ptr) - (const char *)&((type *) 0)->member))

    /**
     * list_first_entry - get the first element from a list
     * @ptr:    the list head to take the element from.
     * @type:   the type of the struct this is embedded in.
     * @member: the name of the list_struct within the struct.
     *
     * Note, that list is expected to be not empty.
     */
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

    /**
     * list_for_each    -   iterate over a list
     * @pos:    the &struct list_head to use as a loop cursor.
     * @head:   the head for your list.
     */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); \
            pos = pos->next)

    /**
     * __list_for_each  -   iterate over a list
     * @pos:    the &struct list_head to use as a loop cursor.
     * @head:   the head for your list.
     *
     * This variant differs from list_for_each() in that it's the
     * simplest possible list iteration code, no prefetching is done.
     * Use this for code that knows the list to be very short (empty
     * or 1 entry) most of the time.
     */
#define __list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)


    /**
     * list_for_each_safe - iterate over a list safe against removal of list entry
     * @pos:    the &struct list_head to use as a loop cursor.
     * @n:      another &struct list_head to use as temporary storage
     * @head:   the head for your list.
     */
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
            pos = n, n = pos->next)

    /**
     * list_for_each_entry  -   iterate over list of given type
     * @pos:    the type * to use as a loop cursor.
     * @head:   the head for your list.
     * @member: the name of the list_struct within the struct.
     */
#define list_for_each_entry(pos, head, member)              \
    for (pos = list_entry((head)->next, typeof(*pos), member);  \
            &pos->member != (head); \
            pos = list_entry(pos->member.next, typeof(*pos), member))

    /**
     * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
     * @pos:    the type * to use as a loop cursor.
     * @n:      another type * to use as temporary storage
     * @head:   the head for your list.
     * @member: the name of the list_struct within the struct.
     */
#define list_for_each_entry_safe(pos, n, head, member)          \
    for (pos = list_entry((head)->next, typeof(*pos), member),  \
            n = list_entry(pos->member.next, typeof(*pos), member); \
            &pos->member != (head);                 \
            pos = n, n = list_entry(n->member.next, typeof(*n), member))



#ifdef __cplusplus
}
#endif
#endif

