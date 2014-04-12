#include "unp.h"
#include "datagram.h"

int dg_send(int fd, datagram *dg, struct sockaddr_in* dest_addr, socklen_t dest_addr_len) {
  return sendto(fd, (char *)dg, DATAGRAM_SIZE, 0, (SA *)dest_addr, dest_addr_len);
}

int dg_send_conn(int fd, datagram *dg) {
  return send(fd, (char *)dg, DATAGRAM_SIZE, 0);
}

int dg_recv(int fd, datagram *dg, struct sockaddr_in* src_addr, socklen_t *src_addr_len) {
  char buff[DATAGRAM_SIZE];
  int rv;

  if ((rv = recvfrom(fd, buff, DATAGRAM_SIZE, 0, (SA *)src_addr, src_addr)) == -1) {
    return -1;
  }

  buff[rv] = 0;
  *dg = *((datagram *)buff);

  return rv;
}

int dg_recv_conn(int fd, datagram *dg) {
  char buff[DATAGRAM_SIZE];
  int rv; 

  if ((rv = recv(fd, buff, DATAGRAM_SIZE, 0)) == -1) {
    return -1;
  }

  buff[rv] = 0;
  *dg = *((datagram *)buff);

  return rv;
}

void dg_print(datagram *dg) {
  char message[DATAGRAM_MESSAGE_SIZE + 1];

  // Do this to null terminate the string being printed
  memset(message, 0, sizeof(DATAGRAM_MESSAGE_SIZE + 1));
  strcpy(message, dg->message);
  message[DATAGRAM_MESSAGE_SIZE] = 0;

  printf("Datagram Header:\n");
  printf("\tseq: %d\n", dg->header.seq_number);
  printf("\tack: %d\n", dg->header.ack_number);
  printf("\twin: %d\n", dg->header.win_size);
  printf("\tlen: %d\n", dg->header.length);
  printf("Datagram Body:\n");
  printf("\tmessage: %s\n", message);
}
