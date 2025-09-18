#include <linux/list.h>
#include <linux/slab.h>
#include "list_driver.h"

extern struct list_head topic_list;

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
			if (proc->head < proc->tail) {
				printk(KERN_ALERT "Process %d unsubscribed from %s with messages to be read\n", pid, topic_title);
			}
			if (proc->pid == pid) {
			    list_del(&proc->link);
			    kfree(proc->messages);
			    kfree(proc->topic_to_be_fetched);
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

	if (strlen(message) > max_msg_size) {
		printk(KERN_ALERT "Message is larger than maximum message size\n");
	}
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

char *fetch_from_process(int pid)
{
	struct topic_s *topic;
	list_for_each_entry(topic, &topic_list, link) {
		struct process_es *process;
		list_for_each_entry(process, &topic->processes, link) {
			if (process->pid == pid && strcmp(process->topic_to_be_fetched, topic->title) == 0) {
				size_t offset = (process->head++ % max_msgs) * max_msg_size;
				return process->messages + offset;
			}
		}
	}
	return NULL;
}
