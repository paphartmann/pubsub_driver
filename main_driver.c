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

int max_msgs;
int max_msg_size;

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

	INIT_LIST_HEAD(&list);

	return 0;
}

static void pubsub_exit(void)
{
	struct topic_s *topic;
	list_for_each_entry(topic, topic_list, link) {
		struct process_es *process;
		list_for_each_entry(process, topic->processes, link) {
			kfree(process->messages);
		}
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
	printk("Process id %d opened the device\n", (int) task_pid_nr(current));

	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	int error = 0;
	struct message_s *entry = list_first_entry(&list, struct message_s, link);

	if (list_empty(&list)) {
		printk(KERN_INFO "PubSub Driver: no data.\n");

		return 0;
	}

	// copy_to_user has the format ( * to, *from, size) and returns 0 on success
	error = copy_to_user(buffer, entry->message, max_msg_size);

	if (!error) {				// if true then have success
		printk(KERN_INFO "PubSub Driver: sent %d characters to the user\n", strlen(entry->message));
		list_delete_head();

		return 0;
	} else {
		printk(KERN_INFO "PubSub Driver: failed to send %d characters to the user\n", error);

		return -EFAULT;			// Failed -- return a bad address message (i.e. -14)
	}
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	char cpy_buffer[len];
	copy_from_user(cpy_buffer, buffer, len);
	
	char action[len];
	sscanf(cpy_buffer, "/%s", action);

	char topic_name[len];
	int pid = (int) task_pid_nr(current);
	if (strcmp(action, "subscribe") == 0) {
		sscanf(cpy_buffer, "/subscribe %s", topic_name);
		printk(KERN_INFO "Process %d wants to subscribe to topic %s\n", pid, topic_name);
		add_process_to_topic(pid, topic_name);
	} else if (strcmp(action, "unsubscribe") == 0) {
		printk(KERN_INFO "Process %d wants to unsubscribe to topic %s\n", pid, topic_name);
		sscanf(cpy_buffer, "/unsubscribe %s", topic_name);
		rem_process_from_topic(pid, topic_name);
	} else if (strcmp(action, "publish") == 0) {
		char message[len];
		sscanf(cpy_buffer, "/publish %s \"%s\"", topic_name, message);
		printk(KERN_INFO "Process %d wants to publish %s to %s\n", pid, message, topic_name);
		publish_to_topic(message, topic_name);
	} else if (strcmp(action, "fetch") == 0) {

	} else {
		printk(KERN_NOTICE "Device was written with wrong format\nMessage written: %s\n", cpy_buffer);
		return -1;
	}
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_INFO "Process id %d closed the device\n", (int) task_pid_nr(current));

	return 0;
}

module_param(max_msgs, int, 0);
module_param(max_msg_size, int, 0);
module_init(pubsub_init);
module_exit(pubsub_exit);
