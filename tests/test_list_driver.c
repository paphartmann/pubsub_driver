#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "list_driver.h"

int max_msgs = 2;
int max_msg_size = 32;
struct list_head topic_list;
struct mutex pubsub_lock = { PTHREAD_MUTEX_INITIALIZER };

static int topic_count(void)
{
	int count = 0;
	struct topic_s *topic;

	list_for_each_entry(topic, &topic_list, link)
		count++;
	return count;
}

static int subscriber_count(const char *topic_title)
{
	int count = 0;
	struct topic_s *topic;

	list_for_each_entry(topic, &topic_list, link) {
		if (strcmp(topic->title, topic_title) == 0) {
			struct process_es *process;
			list_for_each_entry(process, &topic->processes, link)
				count++;
		}
	}
	return count;
}

static void clear_topics(void)
{
	struct topic_s *topic, *topic_tmp;

	list_for_each_entry_safe(topic, topic_tmp, &topic_list, link) {
		struct process_es *process, *process_tmp;

		list_for_each_entry_safe(process, process_tmp, &topic->processes, link) {
			list_del(&process->link);
			free(process->messages);
			free(process->topic_to_be_fetched);
			free(process);
		}
		list_del(&topic->link);
		free(topic);
	}
}

static void test_subscription_and_delivery(void)
{
	char topic[] = "news";
	char message[] = "hello subscribers";
	char bounded_message[] = {'x', 'y', 'z', '\0'};

	add_process_to_topic(101, topic);
	add_process_to_topic(202, topic);
	assert(topic_count() == 1);
	assert(subscriber_count(topic) == 2);

	set_topic_to_be_fetched(101, topic);
	set_topic_to_be_fetched(202, topic);
	assert(fetch_from_process(101) == NULL);
	publish_to_topic(message, topic);
	assert(strcmp(fetch_from_process(101), message) == 0);
	assert(strcmp(fetch_from_process(202), message) == 0);
	assert(fetch_from_process(101) == NULL);
	assert(fetch_from_process(999) == NULL);

	publish_to_topic(bounded_message, topic);
	assert(strcmp(fetch_from_process(101), "xyz") == 0);
	assert(strcmp(fetch_from_process(202), "xyz") == 0);
}

static void test_ring_buffer_and_topic_isolation(void)
{
	char first_topic[] = "first";
	char second_topic[] = "second";
	char first[] = "one";
	char second[] = "two";
	char third[] = "three";
	char other[] = "unrelated";

	add_process_to_topic(303, first_topic);
	add_process_to_topic(303, second_topic);
	assert(topic_count() == 3);

	publish_to_topic(first, first_topic);
	publish_to_topic(second, first_topic);
	publish_to_topic(third, first_topic);
	publish_to_topic(other, second_topic);

	set_topic_to_be_fetched(303, first_topic);
	assert(strcmp(fetch_from_process(303), second) == 0);
	assert(strcmp(fetch_from_process(303), third) == 0);
	assert(fetch_from_process(303) == NULL);
	set_topic_to_be_fetched(303, second_topic);
	assert(strcmp(fetch_from_process(303), other) == 0);
}

static void test_duplicate_subscription_and_unsubscribe(void)
{
	char topic[] = "cleanup";
	char message[] = "still subscribed once";

	add_process_to_topic(404, topic);
	add_process_to_topic(404, topic);
	assert(subscriber_count(topic) == 1);

	rem_process_from_topic(404, topic);
	assert(subscriber_count(topic) == 0);
	assert(fetch_from_process(404) == NULL);

	publish_to_topic(message, topic);
	assert(subscriber_count(topic) == 0);
}

static void test_remove_process_from_all_topics(void)
{
	const char *first_topic = "cleanup-first";
	const char *second_topic = "cleanup-second";
	const char *preserved_topic = "cleanup-preserved";

	add_process_to_topic(505, first_topic);
	add_process_to_topic(606, first_topic);
	add_process_to_topic(505, second_topic);
	add_process_to_topic(505, preserved_topic);

	rem_process_from_all_topics(505);

	assert(subscriber_count(first_topic) == 1);
	assert(subscriber_count(second_topic) == 0);
	assert(subscriber_count(preserved_topic) == 0);
	set_topic_to_be_fetched(505, first_topic);
	assert(fetch_from_process(505) == NULL);
	set_topic_to_be_fetched(606, first_topic);
	publish_to_topic("still subscribed", first_topic);
	assert(strcmp(fetch_from_process(606), "still subscribed") == 0);

	rem_process_from_all_topics(505);
	assert(subscriber_count(first_topic) == 1);
}

static void test_empty_topic_cleanup(void)
{
	const char *topic = "cleanup-topic";

	clear_topics();
	add_process_to_topic(909, topic);
	assert(subscriber_count(topic) == 1);
	assert(topic_count() == 1);

	rem_process_from_topic(909, topic);
	assert(subscriber_count(topic) == 0);
	assert(topic_count() == 0);

	add_process_to_topic(909, topic);
	add_process_to_topic(910, topic);
	assert(subscriber_count(topic) == 2);
	rem_process_from_all_topics(909);
	assert(subscriber_count(topic) == 1);
	assert(topic_count() == 1);
	rem_process_from_all_topics(910);
	assert(subscriber_count(topic) == 0);
	assert(topic_count() == 0);
}

struct concurrent_worker_args {
	int pid;
};

static void *concurrent_worker(void *arg)
{
	struct concurrent_worker_args *worker = arg;
	int i;

	for (i = 0; i < 1000; i++) {
		add_process_to_topic(worker->pid, "concurrent");
		set_topic_to_be_fetched(worker->pid, "concurrent");
		publish_to_topic("concurrent message", "concurrent");
		(void)fetch_from_process(worker->pid);
		if (i % 3 == 0)
			rem_process_from_topic(worker->pid, "concurrent");
	}

	rem_process_from_all_topics(worker->pid);
	return NULL;
}

static void test_concurrent_topic_operations(void)
{
	enum { WORKER_COUNT = 4 };
	pthread_t threads[WORKER_COUNT];
	struct concurrent_worker_args args[WORKER_COUNT];
	int i;

	for (i = 0; i < WORKER_COUNT; i++) {
		args[i].pid = 700 + i;
		assert(pthread_create(&threads[i], NULL, concurrent_worker, &args[i]) == 0);
	}
	for (i = 0; i < WORKER_COUNT; i++)
		assert(pthread_join(threads[i], NULL) == 0);

	assert(subscriber_count("concurrent") == 0);
}

static void test_message_size_rejection(void)
{
	int saved_max_msgs = max_msgs;
	int saved_max_msg_size = max_msg_size;
	char oversized[64];

	max_msgs = 3;
	max_msg_size = 8;
	memset(oversized, 'x', sizeof(oversized));
	oversized[sizeof(oversized) - 1] = '\0';

	add_process_to_topic(811, "oversized");
	publish_to_topic(oversized, "oversized");
	set_topic_to_be_fetched(811, "oversized");
	assert(fetch_from_process(811) == NULL);
	assert(subscriber_count("oversized") == 1);

	max_msgs = saved_max_msgs;
	max_msg_size = saved_max_msg_size;
	rem_process_from_topic(811, "oversized");
}

static void test_invalid_queue_configuration(void)
{
	int saved_max_msgs = max_msgs;
	int saved_max_msg_size = max_msg_size;

	max_msgs = 0;
	add_process_to_topic(808, "invalid-msg-count");
	publish_to_topic("ignored", "invalid-msg-count");
	set_topic_to_be_fetched(808, "invalid-msg-count");
	assert(fetch_from_process(808) == NULL);
	assert(subscriber_count("invalid-msg-count") == 0);

	max_msgs = saved_max_msgs;
	max_msg_size = 0;
	add_process_to_topic(808, "invalid-msg-size");
	publish_to_topic("ignored", "invalid-msg-size");
	assert(fetch_from_process(808) == NULL);
	assert(subscriber_count("invalid-msg-size") == 0);

	max_msg_size = saved_max_msg_size;
}

int main(void)
{
	INIT_LIST_HEAD(&topic_list);
	test_subscription_and_delivery();
	test_ring_buffer_and_topic_isolation();
	test_duplicate_subscription_and_unsubscribe();
	test_remove_process_from_all_topics();
	test_empty_topic_cleanup();
	test_concurrent_topic_operations();
	test_message_size_rejection();
	test_invalid_queue_configuration();
	clear_topics();
	puts("list_driver tests passed");
	return 0;
}
