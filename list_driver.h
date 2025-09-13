#include "params.h"

struct message_s {
	struct list_head link;
	char *message;
	short size;
};

int list_add_entry(const char *data);
void list_show(void);
int list_delete_head(void);
int list_delete_entry(char *data);
