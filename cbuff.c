#include <stdlib.h>
#include "cbuff.h"

void cbuff_init(cbuff *cb, int element_size, int capacity) {
  cb->length = 0;
  cb->capacity = capacity;
  cb->element_size = element_size;

  cb->start = malloc(capacity * element_size);
  cb->end = cb->start + cb->element_size * cb->capacity;
  cb->front = cb->start;
  cb->back = cb->start;
}

void cbuff_free(cbuff *cb) {
  free(cb->start);
}

int cbuff_is_full(cbuff *cb) {
  return cb->length == cb->capacity;
}

void cbuff_insert(cbuff *cb, void *element) {
  *(cb->back) = element;
  cb->length++;
  cb->back = cbuff_next_index(cb, cb->back);
}

void *cbuff_remove(cbuff *cb) {
  void *element = *cb->front;
  cb->length--;
  cb->front = cbuff_next_index(cb, cb->front);
  return element;
}

void *cbuff_access(cbuff *cb, int i) {
  void **cursor = cb->front;
  while (i > 0) {
    cursor = cbuff_next_index(cb, cursor);
    i--;
  }
  return *cursor;
}

void *cbuff_next_index(cbuff *cb, void *index) {
  if (index + cb->element_size == cb->end) {
    return cb->start;
  }
  return index + cb->element_size;
}
