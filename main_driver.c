#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "list_driver.h"
#include "params.h"

#define DEVICE_NAME "pubsub"
#define CLASS_NAME  "pubsub_class"
#define MAX_COMMAND_SIZE 4096

MODULE_LICENSE("GPL");

int max_msgs = 5;
int max_msg_size = 255;

static int majorNumber;
static struct class *charClass = NULL;
static struct device *charDevice = NULL;

struct list_head topic_list;
DEFINE_MUTEX(pubsub_lock);

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
	device_destroy(charClass, MKDEV(majorNumber, 0));
	class_unregister(charClass);
	class_destroy(charClass);
	unregister_chrdev(majorNumber, DEVICE_NAME);
	printk(KERN_INFO "PubSub Driver: goodbye.\n");
}

static int dev_open(struct inode *inodep, struct file *filep)
{
	printk("Process %d opened the device\n", (int) task_pid_nr(current));
	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	int pid = (int) task_pid_nr(current);
	char *result = fetch_from_process(pid);

	if (result != NULL) {
		size_t to_copy = min(len, strlen(result));
		if (copy_to_user(buffer, result, to_copy))
			return -EFAULT;
		return to_copy;
	}
	return 0;
}

static int is_command_whitespace(char character)
{
	return character == ' ' || character == '\t' ||
	       character == '\n' || character == '\r';
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	char *command;
	char *cursor;
	char *action;
	char *topic_name;
	char *message;
	char *message_end;
	size_t topic_len;
	int pid = (int) task_pid_nr(current);
	ssize_t result = -EINVAL;

	if (len == 0)
		return 0;
	if (len > MAX_COMMAND_SIZE)
		return -E2BIG;

	command = kmalloc(len + 1, GFP_KERNEL);
	if (command == NULL)
		return -ENOMEM;
	if (copy_from_user(command, buffer, len)) {
		kfree(command);
		return -EFAULT;
	}
	command[len] = '\0';
	/* Reject malformed input before parsing tokens. */
	if (memchr(command, '\0', len) != NULL || command[0] != '/')
		goto out;

	/* Tokenize: /<action> <topic> [payload]. */
	action = command + 1;
	cursor = action;
	/* Walk past the action name until the first whitespace or the end of the string. */
	while (*cursor != '\0' && !is_command_whitespace(*cursor))
		cursor++;
	if (*cursor != '\0') {
		/* Terminate the action string and skip spacing before the topic. */
		*cursor++ = '\0';
		while (is_command_whitespace(*cursor))
			cursor++;
	}
	/* Reject empty verbs and commands with no topic after the action. */
	if (*action == '\0' || *cursor == '\0')
		goto out;

	/* Topic is the next token, and it must fit in the topic title field. */
	topic_name = cursor;
	/* Advance until the next separator to isolate the topic string. */
	while (*cursor != '\0' && !is_command_whitespace(*cursor))
		cursor++;
	topic_len = cursor - topic_name;
	if (topic_len == 0 || topic_len >= sizeof(((struct topic_s *)0)->title))
		goto out;
	if (*cursor != '\0') {
		/* Close the topic token and move to whatever optional payload remains. */
		*cursor++ = '\0';
		while (is_command_whitespace(*cursor))
			cursor++;
	}

	if (strcmp(action, "subscribe") == 0) {
		if (*cursor != '\0')
			goto out;
		printk(KERN_INFO "KERNEL: Process %d wants to subscribe to topic %s\n", pid, topic_name);
		add_process_to_topic(pid, topic_name);
	} else if (strcmp(action, "unsubscribe") == 0) {
		if (*cursor != '\0')
			goto out;
		printk(KERN_INFO "KENREL: Process %d wants to unsubscribe to topic %s\n", pid, topic_name);
		rem_process_from_topic(pid, topic_name);
	} else if (strcmp(action, "publish") == 0) {
		if (*cursor != '"')
			goto out;
		message = cursor + 1;
		message_end = strchr(message, '"');
		if (message_end == NULL)
			goto out;
		cursor = message_end + 1;
		while (is_command_whitespace(*cursor))
			cursor++;
		if (*cursor != '\0')
			goto out;

		*message_end = '\0';
		printk(KERN_INFO "KERNEL: Process %d wants to publish %s to %s\n",
		       pid, message, topic_name);
		publish_to_topic(message, topic_name);
	} else if (strcmp(action, "fetch") == 0) {
		if (*cursor != '\0')
			goto out;
		printk(KERN_INFO "KERNEL: Process %d wants to fetch from %s\n", pid, topic_name);
		set_topic_to_be_fetched(pid, topic_name);
	} else {
		goto out;
	}

	result = len;
out:
	kfree(command);
	return result;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	int pid = (int)task_pid_nr(current);
	struct topic_s *topic;
	rem_process_from_all_topics(pid);
	printk(KERN_INFO "Process %d closed the device\n", (int) task_pid_nr(current));
	return 0;
}

module_param(max_msgs, int, 0644);
module_param(max_msg_size, int, 0644);
module_init(pubsub_init);
module_exit(pubsub_exit);
