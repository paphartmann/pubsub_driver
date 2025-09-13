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

int list_add_entry(const char *data);
void list_show(void);
int list_delete_head(void);
int list_delete_entry(char *data);
