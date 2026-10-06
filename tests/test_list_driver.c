#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "list_driver.h"

int max_msgs = 2;
int max_msg_size = 32;
struct list_head topic_list;

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

int main(void)
{
	INIT_LIST_HEAD(&topic_list);
	test_subscription_and_delivery();
	test_ring_buffer_and_topic_isolation();
	test_duplicate_subscription_and_unsubscribe();
	clear_topics();
	puts("list_driver tests passed");
	return 0;
}
