#include <linux/list.h>
#include <linux/slab.h>
#include "list_driver.h"

extern struct list_head list;

int list_add_entry(const char *data)
{
	struct message_s *new_node = kmalloc((sizeof(struct message_s)), GFP_KERNEL);
	
	if (!new_node) {
		printk(KERN_INFO "Memory allocation failed, this should never fail due to GFP_KERNEL flag\n");
		
		return 1;
	}
	strcpy(new_node->message, data);
	new_node->size = strlen(data);
	list_add_tail(&(new_node->link), &list);
	
	return 0;
}

void list_show(void)
{
	struct message_s *entry = NULL;
	int i = 0;
	
	list_for_each_entry(entry, &list, link) {
		printk(KERN_INFO "Message #%d: %s\n", i++, entry->message);
	}
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
			kfree(entry);
			
			return 0;
		}
	}
	
	printk(KERN_INFO "Could not find data.");
	
	return 1;
}
