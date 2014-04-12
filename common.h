#include "unp.h"
#include "datagram.h"
#include "cbuff.h"

#define HOST_LOCAL 0
#define HOST_SAME_NETWORK 1
#define HOST_FOREIGN 2

#define HANDSHAKE_TIMEOUT 3

struct ip_info{
  struct in_addr ip;
  struct in_addr netmask;
  struct in_addr subnet;
  int netmask_len;
  struct ip_info *next;
  char ip_str[INET_ADDRSTRLEN];
};

int get_netmask_len(struct in_addr mask);
struct ip_info* insert_into_list(struct ip_info *head,struct ip_info *node);
struct ip_info* make_list();
void free_list(struct ip_info*root);
void print_ip_info(struct ip_info* root);

int get_host_location(struct ip_info *source, struct in_addr *dest);
