#define SERVER_CONFIG_FILE "server.in"
#define DUPLICATE_ACKS_BEFORE_RTO 3

struct ip_info;

typedef struct server_config {
  int port;
  int window_size;
} server_config;

typedef struct listen_interface {
  int fd;
  struct ip_info *info;
} listen_interface;

typedef struct send_win_entry {
  datagram dg;
  int sent_at;
  int was_retransmitted;
} send_win_entry;

int run(listen_interface creator_interface, struct sockaddr_in cli_addr, char *connect_msg);

server_config config;
struct ip_info *interfaces;
listen_interface *listening_interfaces;
int num_listening_interfaces;
