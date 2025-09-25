#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>
#include "list_driver.h"
#include "params.h"

#define DEVICE_NAME "pubsub"
#define CLASS_NAME  "pubsub_class"

MODULE_LICENSE("GPL");

int max_msgs = 5;
int max_msg_size = 255;

static int majorNumber;
//static int number_opens = 0;
static struct class *charClass = NULL;
static struct device *charDevice = NULL;

struct list_head topic_list;

static int	dev_open(struct inode *, struct file *);
static int	dev_release(struct inode *, struct file *);
static ssize_t	dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t	dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops =
{
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};

static int pubsub_init(void)
{
	printk(KERN_INFO "PubSub Driver: Initializing the LKM with parameters max_msgs=%d, max_msg_size=%d\n", max_msgs, max_msg_size);

	// Try to dynamically allocate a major number for the device -- more difficult but worth it
	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if (majorNumber < 0) {
		printk(KERN_ALERT "PubSub Driver failed to register a major number\n");
		return majorNumber;
	}

	printk(KERN_INFO "PubSub Driver: registered correctly with major number %d\n", majorNumber);

	// Register the device class
	charClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(charClass)) {		// Check for error and clean up if there is
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "PubSub Driver: failed to register device class\n");
		return PTR_ERR(charClass);	// Correct way to return an error on a pointer
	}

	printk(KERN_INFO "PubSub Driver: device class registered correctly\n");

	// Register the device driver
	charDevice = device_create(charClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if (IS_ERR(charDevice)) {		// Clean up if there is an error
		class_destroy(charClass);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "PubSub Driver: failed to create the device\n");
		return PTR_ERR(charDevice);
	}

	printk(KERN_INFO "PubSub Driver: device class created.\n");

	INIT_LIST_HEAD(&topic_list);

	return 0;
}

static void pubsub_exit(void)
{
	struct topic_s *topic, *tmp_topic;
    	struct process_es *process, *tmp_process;
    	list_for_each_entry_safe(topic, tmp_topic, &topic_list, link) {
        	list_for_each_entry_safe(process, tmp_process, &topic->processes, link) {
			printk(KERN_INFO "freeing messages");
			if (process->messages != NULL) {
           			kfree(process->messages);
			}
			printk(KERN_INFO "freeing topic_to_be_fetched");
			if (process->topic_to_be_fetched != NULL) {
				kfree(process->topic_to_be_fetched);
			}
            		list_del(&process->link);
            		printk(KERN_INFO "freeing process");
			kfree(process);
        	}
        	list_del(&topic->link);
		printk(KERN_INFO "freeing topic");
        	kfree(topic);
    	}

	device_destroy(charClass, MKDEV(majorNumber, 0));
	class_unregister(charClass);
	class_destroy(charClass);
	unregister_chrdev(majorNumber, DEVICE_NAME);
	printk(KERN_INFO "PubSub Driver: goodbye.\n");
}

static int dev_open(struct inode *inodep, struct file *filep)
{
	//number_opens++;
	//printk(KERN_INFO "PubSub Driver: device has been opened %d time(s)\n", number_opens);
	printk("Process %d opened the device\n", (int) task_pid_nr(current));
	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	int pid = (int) task_pid_nr(current);
	char *result = fetch_from_process(pid);

	if (result != NULL) {
		copy_to_user(buffer, result, len);
		return len;
	} else {
		return -1;
	}
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	char cpy_buffer[len];
	memset(cpy_buffer, 0, len);
	copy_from_user(cpy_buffer, buffer, len);
	
	char action[len];
	sscanf(cpy_buffer, "/%s", action);

	char topic_name[len];
	int pid = (int) task_pid_nr(current);
	if (strcmp(action, "subscribe") == 0) {
		//printk("KERNEL: topic_name = %s\n", topic_name);
		sscanf(cpy_buffer, "/subscribe %s", topic_name);
		//printk("KERNEL: topic_name = %s\n", topic_name);
		printk(KERN_INFO "KERNEL: Process %d wants to subscribe to topic %s\n", pid, topic_name);
		add_process_to_topic(pid, topic_name);
	} else if (strcmp(action, "unsubscribe") == 0) {
		sscanf(cpy_buffer, "/unsubscribe %s", topic_name);
		printk(KERN_INFO "KENREL: Process %d wants to unsubscribe to topic %s\n", pid, topic_name);
		rem_process_from_topic(pid, topic_name);
	} else if (strcmp(action, "publish") == 0) {
		char *message;
		sscanf(cpy_buffer, "/publish %s", topic_name);
		message = strchr(cpy_buffer, '"');
		printk(KERN_INFO "KERNEL: Process %d wants to publish %s to %s\n", pid, message, topic_name);
		publish_to_topic(message, topic_name);
	} else if (strcmp(action, "fetch") == 0) {
		sscanf(cpy_buffer, "/fetch %s", topic_name);
		printk(KERN_INFO "KERNEL: Process %d wants to fetch from %s\n", pid, topic_name);

		struct topic_s *topic;
		list_for_each_entry(topic, &topic_list, link) {
			struct process_es *process;
			list_for_each_entry(process, &topic->processes, link) {
				if (process->pid == pid) {
					if (process->topic_to_be_fetched != NULL) {
						kfree(process->topic_to_be_fetched);
					}
                                        process->topic_to_be_fetched = kmalloc(len, GFP_KERNEL);
                                        memcpy(process->topic_to_be_fetched, topic_name, len);
				}
			}
		}
	} else {
		printk(KERN_NOTICE "Device was written with wrong format\nMessage written: %s\n", cpy_buffer);
		return -1;
	}
	return len;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	int pid = (int)task_pid_nr(current);
	struct topic_s *topic;
	list_for_each_entry(topic, &topic_list, link) {
		rem_process_from_topic(pid, topic->title);
        }
	printk(KERN_INFO "Process %d closed the device\n", (int) task_pid_nr(current));
	return 0;
}

module_param(max_msgs, int, 0644);
module_param(max_msg_size, int, 0644);
module_init(pubsub_init);
module_exit(pubsub_exit);
