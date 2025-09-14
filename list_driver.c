#include <linux/list.h>
#include <linux/slab.h>
#include "list_driver.h"

extern struct list_head topic_list;

/*
int list_add_entry(const char *data)
{
	struct message_s *new_node = kmalloc((sizeof(struct message_s)), GFP_KERNEL);
	new_node->message = kmalloc(max_msg_size, GFP_KERNEL);

	if (!new_node || !new_node->message) {
		printk(KERN_INFO "Memory allocation failed, this should never fail due to GFP_KERNEL flag\n");

		return 1;
	}
	strcpy(new_node->message, data);
	list_add_tail(&(new_node->link), &list);

	return 0;
}

int list_delete_head(void)
{
	struct message_s *entry = NULL;

	if (list_empty(&list)) {
		printk(KERN_INFO "Empty list.\n");

		return 1;
	}

	entry = list_first_entry(&list, struct message_s, link);

	list_del(&entry->link);
	kfree(entry);

	return 0;
}

int list_delete_entry(char *data)
{
	struct message_s *entry = NULL;

	list_for_each_entry(entry, &list, link) {
		if (strcmp(entry->message, data) == 0) {
			list_del(&(entry->link));
			kfree(entry->message);
			kfree(entry);

			return 0;
		}
	}

	printk(KERN_INFO "Could not find data.");

	return 1;
}*/

void add_process_to_topic(int pid, char topic_title[64])
{
	struct topic_s *entry = NULL;

	list_for_each_entry(entry, &topic_list, link) {
		if (strcmp(entry->title, topic_title)) {
			struct process_es *new_process = kmalloc(sizeof(struct process_es), GFP_KERNEL);
			new_process->pid = pid;
			new_process->messages = kmalloc(max_msg_size * max_msgs, GFP_KERNEL);
			list_add_tail(&new_process->link, &topic_list);
			return;
		}
	}

	entry = kmalloc(sizeof(struct topic_s), GFP_KERNEL);
	entry->head = 0;
	entry->tail = 0;
	strcpy(entry->title, topic_title);
	INIT_LIST_HEAD(&entry->link);
}
