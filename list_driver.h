#include "params.h"

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

void add_process_to_topic(int pid, char topic_title[64]);
void rem_process_from_topic(int pid, char topic_title[64]);
void publish_to_topic(char *message, char topic_title[64]);
void set_topic_to_be_fetched(int pid, char topic_title[64]);
char *fetch_from_process(int pid);
