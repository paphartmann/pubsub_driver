#ifndef PUBSUB_TEST_SUPPORT_H
#define PUBSUB_TEST_SUPPORT_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

#define INIT_LIST_HEAD(head) do { \
	(head)->next = (head); \
	(head)->prev = (head); \
} while (0)

#define list_add_tail(node, head) do { \
	(node)->prev = (head)->prev; \
	(node)->next = (head); \
	(head)->prev->next = (node); \
	(head)->prev = (node); \
} while (0)

#define list_del(node) do { \
	(node)->prev->next = (node)->next; \
	(node)->next->prev = (node)->prev; \
} while (0)

#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))

#define list_for_each_entry(pos, head, member) \
	for (pos = container_of((head)->next, __typeof__(*pos), member); \
	     &(pos)->member != (head); \
	     pos = container_of((pos)->member.next, __typeof__(*pos), member))

#define list_for_each_entry_safe(pos, tmp, head, member) \
	for (pos = container_of((head)->next, __typeof__(*pos), member), \
	     tmp = container_of((pos)->member.next, __typeof__(*pos), member); \
	     &(pos)->member != (head); \
	     pos = tmp, tmp = container_of((tmp)->member.next, __typeof__(*tmp), member))

#define GFP_KERNEL 0
#define KERN_ALERT ""
#define KERN_INFO ""
#define kmalloc(size, flags) ((void)(flags), malloc(size))
#define kfree(pointer) free(pointer)
#define printk(...) ((void)0)
#define mdelay(milliseconds) ((void)(milliseconds))

extern int max_msgs;
extern int max_msg_size;

#endif
