#include "params.h"

struct topic {
	struct list_head link;
	char title[64];
	struct process_es processes;
};

struct process_es {
	struct list_head link;
	int pid;
	char *messages;
	int head = 0, tail = 0;
};

void add_process_to_topic(int pid, char topic_title[64]);
void rem_process_from_topic(int pid, char topic_title[64]);
void publish_to_topic(char *message, char topic_title[64]);
