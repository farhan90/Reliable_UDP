#define DATAGRAM_SIZE 512
#define DATAGRAM_HEADER_SIZE sizeof(datagram_header)
#define DATAGRAM_MESSAGE_SIZE DATAGRAM_SIZE - DATAGRAM_HEADER_SIZE

typedef struct datagram_header {
  int seq_number;
  int ack_number;
  int win_size;
  int length;
} datagram_header;

typedef struct datagram {
  datagram_header header;
  char message[DATAGRAM_MESSAGE_SIZE];
} datagram;

int dg_send(int fd, datagram *dg, struct sockaddr_in* dest_addr, socklen_t dest_addr_len);
int dg_send_conn(int fd, datagram *dg);
int dg_recv(int fd, datagram *dg, struct sockaddr_in* src_addr, socklen_t *src_addr_len);
int dg_recv_conn(int fd, datagram *dg);
void dg_print(datagram *dg);
