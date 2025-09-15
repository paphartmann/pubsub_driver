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
		if (strcmp(entry->title, topic_title) == 0) {
			struct process_es *new_process = kmalloc(sizeof(struct process_es), GFP_KERNEL);
			new_process->pid = pid;
			new_process->messages = kmalloc(max_msg_size * max_msgs, GFP_KERNEL);
			new_process->head = 0;
			new_process->tail = 0;
			INIT_LIST_HEAD(&new_process->link);
			list_add_tail(&new_process->link, &entry->processes);
			return;
		}
	}

	entry = kmalloc(sizeof(struct topic_s), GFP_KERNEL);
	strcpy(entry->title, topic_title);
	INIT_LIST_HEAD(&entry->link);
	INIT_LIST_HEAD(&entry->processes);
	list_add_tail(&entry->link, &topic_list);

	struct process_es *new_process = kmalloc(sizeof(struct process_es), GFP_KERNEL);
	new_process->messages = kmalloc(max_msgs * max_msg_size, GFP_KERNEL);
	new_process->head = 0;
	new_process->tail = 0;
	new_process->pid = pid;
	INIT_LIST_HEAD(&new_process->link);
	list_add_tail(&new_process->link, &entry->processes);
}

void rem_process_from_topic(int pid, char topic_title[64])
{
    struct topic_s *topic;
    struct process_es *proc, *tmp;

    list_for_each_entry(topic, &topic_list, link) {
        if (strcmp(topic->title, topic_title) == 0) {

            list_for_each_entry_safe(proc, tmp, &topic->processes, link) {
                if (proc->pid == pid) {
                    list_del(&proc->link);
                    kfree(proc->messages);
                    kfree(proc);
                    return;
                }
            }

            return;
        }
    }
}

void publish_to_topic(char *message, char topic_title[64])
{
	struct topic_s *entry = NULL;

	list_for_each_entry(entry, &topic_list, link) {
		if (strcmp(topic_title, entry->title) == 0) {
			struct process_es *process = NULL;
			list_for_each_entry(process, &entry->processes, link) {
				size_t offset = (process->tail++ % max_msgs) * max_msg_size;
				memcpy(process->messages + offset, message, max_msg_size);
			}
			return;
		}
	}
}
