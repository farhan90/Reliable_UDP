typedef struct cbuff {
  int length;
  int capacity;
  int element_size;
  void **start;
  void **front;
  void **back;
  void **end;
} cbuff;

void cbuff_init(cbuff *cb, int element_size, int capacity);
void cbuff_free(cbuff *cb);

int cbuff_is_full(cbuff *cb);
void cbuff_insert(cbuff *cb, void *element);
void *cbuff_remove(cbuff *cb);
void *cbuff_access(cbuff *cb, int i);
void *cbuff_next_index(cbuff *cb, void *index);
