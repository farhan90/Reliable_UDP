typedef struct connection {
  struct in_addr ip;
  int port;
  int pid;
  struct connection *next;
} connection;

connection *conn_list_insert(connection *head, struct in_addr ip, int port, int pid) {
  connection *conn; 

  conn = malloc(sizeof(connection));
  conn->ip = ip;
  conn->port = port;
  conn->pid = pid;

  if (head != NULL) {
    conn->next = head;
  }

  return conn;
}

connection *conn_list_remove_pid(connection *head, int pid) {
  connection *prev;
  connection *cursor;

  if (head == NULL) {
    return head;
  }

  if (head->pid == pid) {
    cursor = head->next;
    free(head);
    return cursor;
  }

  prev = head;
  cursor = head->next;
  while (cursor != NULL) {
    if (cursor->pid == pid) {
      prev->next = cursor->next;
      free(cursor);
      return head;
    }
    prev = prev->next;
    cursor = cursor->next;
  }

  return head;
}

int conn_list_contains_addr(connection *head, struct in_addr ip, int port) {
  connection *cursor;

  cursor = head;
  while (cursor != NULL) {
    if (cursor->ip.s_addr == ip.s_addr && cursor->port == port) {
      return 1;
    }
    cursor = cursor->next;
  }

  return 0;
}
