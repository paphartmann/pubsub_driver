#ifdef UNIT_TEST
#include "tests/test_support.h"
#else
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/slab.h>
#endif
#include "list_driver.h"

extern struct list_head topic_list;

void add_process_to_topic(int pid, const char *topic_title)
{
	struct topic_s *entry;
	struct process_es *process;

	if (max_msgs <= 0 || max_msg_size <= 0)
		return;

	list_for_each_entry(entry, &topic_list, link) {
		if (strcmp(entry->title, topic_title) == 0) {
			list_for_each_entry(process, &entry->processes, link) {
				if (process->pid == pid)
					return;
			}
			goto add_process;
		}
	}

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (entry == NULL)
		return;
	memset(entry, 0, sizeof(*entry));
	strncpy(entry->title, topic_title, sizeof(entry->title) - 1);
	INIT_LIST_HEAD(&entry->link);
	INIT_LIST_HEAD(&entry->processes);
	list_add_tail(&entry->link, &topic_list);

add_process:
	process = kmalloc(sizeof(*process), GFP_KERNEL);
	if (process == NULL)
		return;
	memset(process, 0, sizeof(*process));
	process->messages = kmalloc((size_t)max_msg_size * max_msgs, GFP_KERNEL);
	process->topic_to_be_fetched = kmalloc(64, GFP_KERNEL);
	if (process->messages == NULL || process->topic_to_be_fetched == NULL) {
		kfree(process->messages);
		kfree(process->topic_to_be_fetched);
		kfree(process);
		return;
	}
	process->topic_to_be_fetched[0] = '\0';
	process->pid = pid;
	INIT_LIST_HEAD(&process->link);
	list_add_tail(&process->link, &entry->processes);
}

void rem_process_from_topic(int pid, const char *topic_title)
{
	struct topic_s *topic;
	struct process_es *proc, *tmp;

	list_for_each_entry(topic, &topic_list, link) {
		if (strcmp(topic->title, topic_title) != 0)
			continue;

		list_for_each_entry_safe(proc, tmp, &topic->processes, link) {
			if (proc->pid == pid) {
				list_del(&proc->link);
				kfree(proc->messages);
				kfree(proc->topic_to_be_fetched);
				kfree(proc);
				return;
			}
		}
	}
}

void publish_to_topic(const char *message, const char *topic_title)
{
	struct topic_s *entry = NULL;

	if (message == NULL || max_msgs <= 0 || max_msg_size <= 0)
		return;
	list_for_each_entry(entry, &topic_list, link) {
		if (strcmp(topic_title, entry->title) == 0) {
			struct process_es *process = NULL;
			list_for_each_entry(process, &entry->processes, link) {
				size_t index;
				size_t offset;
				size_t length = strnlen(message, max_msg_size);

				if (length >= (size_t)max_msg_size)
					length = max_msg_size - 1;

				if ((size_t)(process->tail - process->head) >= (size_t)max_msgs)
					process->head++;
				index = (size_t)(process->tail % max_msgs);
				offset = index * max_msg_size;
				memset(process->messages + offset, 0, max_msg_size);
				if (length > 0)
					memcpy(process->messages + offset, message, length);
				process->messages[offset + length] = '\0';
				process->tail++;
			}
			return;
		}
	}
}

void set_topic_to_be_fetched(int pid, const char *topic_title)
{
	struct topic_s *topic;

	list_for_each_entry(topic, &topic_list, link) {
		struct process_es *process;

		list_for_each_entry(process, &topic->processes, link) {
			if (process->pid == pid) {
				strncpy(process->topic_to_be_fetched, topic_title, 63);
				process->topic_to_be_fetched[63] = '\0';
			}
		}
	}
}

char *fetch_from_process(int pid)
{
	struct topic_s *topic;

	if (max_msgs <= 0 || max_msg_size <= 0)
		return NULL;

	list_for_each_entry(topic, &topic_list, link) {
		struct process_es *process;
		list_for_each_entry(process, &topic->processes, link) {
			if (process->pid == pid && process->topic_to_be_fetched != NULL &&
			    strcmp(process->topic_to_be_fetched, topic->title) == 0) {
				size_t head_index;
				size_t offset;

				if (process->head == process->tail)
					return NULL;
				head_index = (size_t)(process->head % max_msgs);
				offset = head_index * max_msg_size;
				process->head++;
				return process->messages + offset;
			}
		}
	}
	return NULL;
}
