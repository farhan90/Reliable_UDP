#include "common.h"
#include "server.h"
#include "server_conn_list.h"

connection *conn_list;

void on_sigchld(int sig) {
  int pid;
  pid = waitpid(-1, NULL, WNOHANG);
  conn_list = conn_list_remove_pid(conn_list, pid);
}

void read_server_config(server_config* config) {
  FILE *config_file;
  char buff[MAXLINE];

  if ((config_file = fopen(SERVER_CONFIG_FILE, "r")) == NULL) {
    err_sys("Server config file not found");
  }

  if (fgets(buff, MAXLINE, config_file) == NULL) {
    err_sys("Couldn't read server port number");
  }
  config->port = atoi(buff);

  if (fgets(buff, MAXLINE, config_file) == NULL) {
    err_sys("Couldn't read server window size");
  }
  config->window_size = atoi(buff);

  fclose(config_file);
}

void walk_interfaces() {
  struct ip_info *cursor;
  interfaces = make_list();
  cursor = interfaces;
  while (cursor != NULL) {
    print_ip_info(cursor);
    cursor = cursor->next;
    num_listening_interfaces++;
    printf("\n");
  }
  printf("Found %d network interfaces\n", num_listening_interfaces);
}

void bind_listening_socks() {
  struct sockaddr_in listen_addr;
  struct ip_info *interface;
  int fd;
  int i;

  listening_interfaces = malloc(num_listening_interfaces * sizeof(listen_interface));

  i = 0;
  interface = interfaces;
  while (interface != NULL) {
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
      err_sys("Couldn't create a listening socket fd");
    }
    bzero(&listen_addr, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = interface->ip.s_addr;
    listen_addr.sin_port = htons(config.port);

    if (bind(fd, (SA *)&listen_addr, sizeof(listen_addr)) == -1) {
      err_sys("Couldn't bind a listening socket");
    }

    printf(
        "Bound a listening socket at IP addr %s on port %d to fd %d\n", 
        interface->ip_str, 
        config.port, 
        fd);

    listening_interfaces[i].fd = fd;
    listening_interfaces[i].info = interface;
    i++;
    interface = interface->next;
  }
}

void handle_connect(listen_interface interface) {
  struct sockaddr_in client_addr;
  socklen_t client_len;
  char buff[MAXLINE];
  char ip_str[INET_ADDRSTRLEN];
  int num_read;
  int pid;

  client_len = sizeof(client_addr);
  num_read = recvfrom(interface.fd, buff, MAXLINE, 0, (SA *)&client_addr, &client_len);
  buff[num_read] = 0;

  if (!conn_list_contains_addr(conn_list, client_addr.sin_addr, client_addr.sin_port)) {
    pid = fork();
    if (pid == 0) {
      run(interface, client_addr, buff);
      exit(0);
    } else {
      conn_list = conn_list_insert(conn_list, client_addr.sin_addr, client_addr.sin_port, pid);
    }
  } else {
    inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
    printf(
        "Request ignored, a client is already being handled with the IP addr %s port number %d\n", 
        ip_str,
        ntohs(client_addr.sin_port));
  }
}

void select_loop() {
  fd_set listen_set;
  int max_fd;
  int i;

  while (1) {
    FD_ZERO(&listen_set);
    max_fd = 0;
    for (i = 0; i < num_listening_interfaces; i++) {
      FD_SET(listening_interfaces[i].fd, &listen_set);
      max_fd = max(max_fd, listening_interfaces[i].fd);
    }
    if (select(max_fd + 1, &listen_set, NULL, NULL, NULL) > 0) {
      for (i = 0; i < num_listening_interfaces; i++) {
        if (FD_ISSET(listening_interfaces[i].fd, &listen_set)) {
          handle_connect(listening_interfaces[i]);
        }
      }
    }
  }
}

int main(int argc, char **argv) {
  signal(SIGCHLD, &on_sigchld);
  conn_list = NULL;

  printf("Server starting...\n");
  printf("\n");

  printf("Reading config file...\n");
  read_server_config(&config);

  printf("Server config:\n");
  printf("\tPort: %d\n",         config.port);
  printf("\tWindow Size: %d\n",  config.window_size);
  printf("\n");

  printf("Walking network interfaces...\n");
  walk_interfaces();
  printf("\n");

  printf("Binding listening sockets...\n");
  bind_listening_socks();
  printf("\n");

  printf("Entering select loop...\n");
  select_loop();

  return 0;
}
