#include "params.h"

#ifdef UNIT_TEST
#include "tests/test_support.h"
#else
#include <linux/list.h>
#endif

struct process_es {
	struct list_head link;
	int pid;
	char *messages;
	int head, tail;
	char *topic_to_be_fetched;
};

struct topic_s {
	struct list_head link;
	char title[64];
	struct list_head processes;
};

void add_process_to_topic(int pid, const char *topic_title);
void rem_process_from_topic(int pid, const char *topic_title);
void publish_to_topic(const char *message, const char *topic_title);
void set_topic_to_be_fetched(int pid, const char *topic_title);
char *fetch_from_process(int pid);
