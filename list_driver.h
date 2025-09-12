#define MSG_SIZE	128

struct message_s {
	struct list_head link;
	char message[MSG_SIZE];
	short size;
};

int list_add_entry(const char *data);
void list_show(void);
int list_delete_head(void);
int list_delete_entry(char *data);
