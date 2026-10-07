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

	mutex_lock(&pubsub_lock);
	if (max_msgs <= 0 || max_msg_size <= 0) {
		mutex_unlock(&pubsub_lock);
		return;
	}

	/* Each topic owns its own subscriber list, so the loop below checks whether
	 * this pid is already attached before appending another process entry.
	 */
	list_for_each_entry(entry, &topic_list, link) {
		if (strcmp(entry->title, topic_title) == 0) {
			list_for_each_entry(process, &entry->processes, link) {
				if (process->pid == pid) {
					mutex_unlock(&pubsub_lock);
					return;
				}
			}
			goto add_process;
		}
	}

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (entry == NULL) {
		mutex_unlock(&pubsub_lock);
		return;
	}
	memset(entry, 0, sizeof(*entry));
	strncpy(entry->title, topic_title, sizeof(entry->title) - 1);
	INIT_LIST_HEAD(&entry->link);
	INIT_LIST_HEAD(&entry->processes);
	list_add_tail(&entry->link, &topic_list);

/* The topic lookup above either matched an existing node or created a new one;
 * this shared label continues the common logic for attaching the process.
 */
add_process:
	process = kmalloc(sizeof(*process), GFP_KERNEL);
	if (process == NULL) {
		mutex_unlock(&pubsub_lock);
		return;
	}
	memset(process, 0, sizeof(*process));
	process->messages = kmalloc((size_t)max_msg_size * max_msgs, GFP_KERNEL);
	process->topic_to_be_fetched = kmalloc(64, GFP_KERNEL);
	if (process->messages == NULL || process->topic_to_be_fetched == NULL) {
		kfree(process->messages);
		kfree(process->topic_to_be_fetched);
		kfree(process);
		mutex_unlock(&pubsub_lock);
		return;
	}
	/* The fetch target is tracked separately from the topic membership itself:
	 * a process can be subscribed to a topic, but only one topic is selected for
	 * the next read operation.
	 */
	process->topic_to_be_fetched[0] = '\0';
	process->pid = pid;
	INIT_LIST_HEAD(&process->link);
	list_add_tail(&process->link, &entry->processes);
	mutex_unlock(&pubsub_lock);
}

void rem_process_from_topic(int pid, const char *topic_title)
{
	struct topic_s *topic;
	struct process_es *proc, *tmp;

	mutex_lock(&pubsub_lock);
	list_for_each_entry(topic, &topic_list, link) {
		if (strcmp(topic->title, topic_title) != 0)
			continue;

		list_for_each_entry_safe(proc, tmp, &topic->processes, link) {
			if (proc->pid == pid) {
				if (proc->head != proc->tail)
					printk(KERN_WARNING "PubSub: removing process %d from topic %s with unread messages\n",
					       pid, topic_title);
				list_del(&proc->link);
				kfree(proc->messages);
				kfree(proc->topic_to_be_fetched);
				kfree(proc);
				if (list_empty(&topic->processes)) {
					list_del(&topic->link);
					kfree(topic);
				}
				mutex_unlock(&pubsub_lock);
				return;
			}
		}
	}
	mutex_unlock(&pubsub_lock);
}

void rem_process_from_all_topics(int pid)
{
	struct topic_s *topic, *topic_tmp;
	struct process_es *proc, *proc_tmp;

	mutex_lock(&pubsub_lock);
	list_for_each_entry_safe(topic, topic_tmp, &topic_list, link) {
		list_for_each_entry_safe(proc, proc_tmp, &topic->processes, link) {
			if (proc->pid == pid) {
				if (proc->head != proc->tail)
					printk(KERN_WARNING "PubSub: removing process %d from topic %s with unread messages\n",
					       pid, topic->title);
				/* Removing the pid from every topic requires cleaning the
				 * subscriber record and then dropping the topic node if it is now empty.
				 */
				list_del(&proc->link);
				kfree(proc->messages);
				kfree(proc->topic_to_be_fetched);
				kfree(proc);
			}
		}
		if (list_empty(&topic->processes)) {
			list_del(&topic->link);
			kfree(topic);
		}
	}
	mutex_unlock(&pubsub_lock);
}

void publish_to_topic(const char *message, const char *topic_title)
{
	struct topic_s *entry = NULL;

	mutex_lock(&pubsub_lock);
	if (message == NULL || max_msgs <= 0 || max_msg_size <= 0) {
		mutex_unlock(&pubsub_lock);
		return;
	}
	list_for_each_entry(entry, &topic_list, link) {
		if (strcmp(topic_title, entry->title) == 0) {
			struct process_es *process = NULL;
			size_t message_len = strnlen(message, (size_t)max_msg_size + 1);

			if (message_len > (size_t)max_msg_size - 1) {
				printk(KERN_WARNING "PubSub: dropping message for topic %s because it exceeds %d bytes\n",
				       topic_title, max_msg_size);
				mutex_unlock(&pubsub_lock);
				return;
			}
			list_for_each_entry(process, &entry->processes, link) {
				size_t index;
				size_t offset;

				/* The process queue is a ring buffer: once it fills up, the oldest
				 * unread item is discarded so the newest message can be stored in the
				 * next slot without growing the allocation.
				 */
				if ((size_t)(process->tail - process->head) >= (size_t)max_msgs) {
					printk(KERN_WARNING "PubSub: queue for pid %d on topic %s is full; dropping oldest message\n",
					       process->pid, topic_title);
					process->head++;
				}
				index = (size_t)(process->tail % max_msgs);
				offset = index * max_msg_size;
				memset(process->messages + offset, 0, max_msg_size);
				memcpy(process->messages + offset, message, message_len);
				process->messages[offset + message_len] = '\0';
				process->tail++;
			}
			mutex_unlock(&pubsub_lock);
			return;
		}
	}
	mutex_unlock(&pubsub_lock);
}

void set_topic_to_be_fetched(int pid, const char *topic_title)
{
	struct topic_s *topic;

	mutex_lock(&pubsub_lock);
	list_for_each_entry(topic, &topic_list, link) {
		struct process_es *process;

		/* A process can only consume from one topic at a time, so this stores the
		 * currently selected topic for later fetches without changing membership.
		 */
		list_for_each_entry(process, &topic->processes, link) {
			if (process->pid == pid) {
				strncpy(process->topic_to_be_fetched, topic_title, 63);
				process->topic_to_be_fetched[63] = '\0';
			}
		}
	}
	mutex_unlock(&pubsub_lock);
}

char *fetch_from_process(int pid)
{
	struct topic_s *topic;
	char *result = NULL;

	mutex_lock(&pubsub_lock);
	if (max_msgs <= 0 || max_msg_size <= 0) {
		mutex_unlock(&pubsub_lock);
		return NULL;
	}

	list_for_each_entry(topic, &topic_list, link) {
		struct process_es *process;
		list_for_each_entry(process, &topic->processes, link) {
			/* The fetch is gated by the topic that this process selected earlier; this
			 * prevents a process from reading from a different subscriber queue.
			 */
			if (process->pid == pid && process->topic_to_be_fetched != NULL &&
			    strcmp(process->topic_to_be_fetched, topic->title) == 0) {
				size_t head_index;
				size_t offset;

				/* head and tail are the queue's read and write cursors. When they are
				 * equal, the buffer is empty even though the underlying storage still
				 * exists.
				 */
				if (process->head == process->tail) {
					mutex_unlock(&pubsub_lock);
					return NULL;
				}
				head_index = (size_t)(process->head % max_msgs);
				offset = head_index * max_msg_size;
				result = kmalloc((size_t)max_msg_size + 1, GFP_KERNEL);
				if (result == NULL) {
					mutex_unlock(&pubsub_lock);
					return NULL;
				}
				memcpy(result, process->messages + offset, (size_t)max_msg_size);
				result[max_msg_size] = '\0';
				memset(process->messages + offset, 0, (size_t)max_msg_size);
				process->head++;
				break;
			}
		}
		if (result != NULL)
			break;
	}
	mutex_unlock(&pubsub_lock);
	return result;
}
