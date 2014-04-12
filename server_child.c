#include "common.h"
#include "server.h"
#include "rto.h"
#include <limits.h>

listen_interface interface;
struct sockaddr_in client_addr;
struct sockaddr_in conn_addr;
int conn_fd;
char client_ip_str[INET_ADDRSTRLEN];
int pid;
int use_dont_route;
char file_name[MAXLINE];
FILE *file;
int next_seq;
int next_ack;
int prev_ack;
int cli_win_size;
int cwnd;
int ssthresh;
int cwnd_acks_left;
int duplicate_acks;
int retransmits;
int client_open;
int timeout_active;
int num_sent;
cbuff send_win;

void on_sigalrm() {
  // noop
}

int time_in_millis() {
  struct timeval current_time;
  gettimeofday(&current_time, NULL);
  return current_time.tv_sec * 1000 + current_time.tv_usec / 1000;
}

void close_inherited_sockets() {
  int i;
  for (i = 0; i < num_listening_interfaces; i++) {
    listen_interface iface = listening_interfaces[i];
    if (iface.fd != interface.fd) {
      printf("Child(%d): Closing fd %d\n", pid, iface.fd);
      close(iface.fd);
    }
  }
}

void analyze_client() {
  int location;

  inet_ntop(AF_INET, &client_addr.sin_addr, client_ip_str, INET_ADDRSTRLEN);
  printf(
      "Child(%d): Client IP addr %s port number %d\n", 
      pid, 
      client_ip_str, 
      ntohs(client_addr.sin_port));

  location = get_host_location(interface.info, &client_addr.sin_addr);
  printf("Child(%d): Client location is ", pid);
  switch (location) {
    case HOST_LOCAL:
      printf("localhost");
      break;
    case HOST_SAME_NETWORK: 
      printf("same network");
      break;
    case HOST_FOREIGN:
      printf("foreign");
      break;
  }
  printf("\n");

  use_dont_route = (location == HOST_LOCAL || location == HOST_SAME_NETWORK);
  if (use_dont_route) {
    printf("Child(%d): Will not use routing for this connection\n", pid);
  }
}

void open_connection() {
  socklen_t conn_len;
  char conn_ip[INET_ADDRSTRLEN];
  int conn_port;
  int on[1] = { 1 };

  bzero(&conn_addr, sizeof(conn_addr));
  conn_addr.sin_family = AF_INET;
  conn_addr.sin_addr = interface.info->ip;
  conn_addr.sin_port = htons(0);

  if ((conn_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    err_sys("Couldn't create a connection socket fd");
  }

  if (bind(conn_fd, (SA *)&conn_addr, sizeof(conn_addr)) == -1) {
    err_sys("Couldn't bind a connection socket");
  }

  if (connect(conn_fd, (SA *)&client_addr, sizeof(client_addr)) == -1) {
    err_sys("Couldn't connect the connection socket");
  }

  if (use_dont_route) {
    printf("Child(%d): Setting SO_DONTROUTE...\n", pid);
    if (setsockopt(conn_fd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on)) == -1) {
      err_sys("Error setting SO_DONTROUTE on the listen socket");
    }
  }

  bzero(&conn_addr, sizeof(conn_addr));
  conn_len = sizeof(conn_addr);
  if (getsockname(conn_fd, (SA *)&conn_addr, &conn_len) == -1) {
    err_sys("Couldn't get connection socket information");
  }
  inet_ntop(AF_INET, &conn_addr.sin_addr, conn_ip, INET_ADDRSTRLEN);
  conn_port = ntohs(conn_addr.sin_port);

  printf(
      "Child(%d): Connection socket opened on IP addr %s and port %d\n", 
      pid, 
      conn_ip, 
      conn_port);
}

void send_connection_info() {
  fd_set listen_set;
  char buff[MAXLINE];
  struct timeval timeout;
  int num_read;
  int ready;
  int parent_fd_flags = 0;

  if (use_dont_route) {
    parent_fd_flags |= MSG_DONTROUTE;
    printf("Child(%d): Sending messages to server parent's socket as MSG_DONTROUTE\n", pid);
  }

  // Send the info only to the listening socket at first
  sprintf(buff, "%d", ntohs(conn_addr.sin_port));
  sendto(interface.fd, buff, strlen(buff), parent_fd_flags, (SA *)&client_addr, sizeof(client_addr));

  printf("Child(%d): Waiting for ack from client\n", pid);
  while (1) {
    FD_ZERO(&listen_set);
    FD_SET(conn_fd, &listen_set);
    timeout.tv_sec = HANDSHAKE_TIMEOUT;
    ready = select(conn_fd + 1, &listen_set, NULL, NULL, &timeout);
    if (ready > 0) {
      // Received an ack
      if (FD_ISSET(conn_fd, &listen_set)) {
        num_read = recvfrom(conn_fd, buff, MAXLINE, 0, NULL, NULL);
        buff[num_read] = 0;
        cli_win_size = atoi(buff);
        ssthresh = cli_win_size;
        printf(
            "Child(%d): Received ack from client, initial window size is %d\n", 
            pid, 
            cli_win_size);
        break;
      }
    } else if (ready == 0) {
      // If timed out, send to both listening socket and connection socket
      printf("Child(%d): Timed out, sending info again...\n", pid);
      sendto(interface.fd, buff, strlen(buff), 0, (SA *)&client_addr, sizeof(client_addr));      
      sendto(conn_fd, buff, strlen(buff), 0, (SA *)&client_addr, sizeof(client_addr));
    } else if (ready == -1 && errno != EINTR) {
      err_sys("Select error");
    }
  }

  // Close the listen fd, no longer needed
  close(interface.fd);
}

void send_new_segments() {
  send_win_entry *entry;
  datagram *dg;
  int in_flight;
  int nread;
  int i;

  in_flight = send_win.length;
  num_sent = min(cwnd, min(send_win.capacity, cli_win_size)) - in_flight;

  // Already reached EOF or can't currently send anything
  if (feof(file) || num_sent <= 0) {
    num_sent = 0;
    return;
  }

  for (i = 0; i < num_sent; i++) {
    entry = malloc(sizeof(send_win_entry));
    entry->was_retransmitted = 0;

    dg = &entry->dg;
    memset(dg->message, 0, DATAGRAM_MESSAGE_SIZE);
    nread = fread(dg->message, 1, DATAGRAM_MESSAGE_SIZE, file);

    dg->header.seq_number = next_seq;
    dg->header.ack_number = feof(file) ? -1 : 0;
    dg->header.win_size = 1;
    dg->header.length = nread;

    cbuff_insert(&send_win, entry);
    next_seq++;

    entry->sent_at = time_in_millis();

    dg_send_conn(conn_fd, dg);

    // Reached EOF, this is the last segment
    if (feof(file)) {
      num_sent = i + 1;
      break;
    }
  }

  printf(
      "Child(%d): %d segment(s) numbered %d through %d were put in sending window and sent out\n",
      pid,
      num_sent,
      next_seq - num_sent, 
      next_seq - 1);
}

void set_timeout() {
  sigset_t sigalrm;
  struct itimerval duration;
  int millis;

  millis = get_rto();

  printf(
      "Child(%d): Setting a timeout for %dms, uncapped rto is %dms\n", 
      pid, 
      millis, 
      rto);

  sigemptyset(&sigalrm);
  sigaddset(&sigalrm, SIGALRM);
  sigprocmask(SIG_BLOCK, &sigalrm, NULL);

  duration.it_interval.tv_sec = 0;
  duration.it_interval.tv_usec = 0;
  duration.it_value.tv_sec = millis / 1000;
  duration.it_value.tv_usec = (millis % 1000) * 1000;

  setitimer(ITIMER_REAL, &duration, NULL);

  timeout_active = 1;
}

void retransmit_oldest_segment() {
  send_win_entry *entry;

  if (retransmits++ >= MAX_RTO_COUNT) {
    printf("Child(%d): Maximum number of retransmits reached\n", pid);
    return;
  }

  if (send_win.length == 0) {
    if (!feof(file)) {
      // This isn't really a retransmit, its just many requests for something that hasn't been
      // added to the send window yet
      send_new_segments();

      printf(
        "Child(%d): Retransmit caused segment %d to be loaded\n", 
        pid,
        prev_ack);

      retransmits = 0;
    }
  } else {
    // Resend the first item in the cbuff window
    entry = cbuff_access(&send_win, 0);
    entry->was_retransmitted = 1;
    dg_send_conn(conn_fd, &entry->dg);

    // Reset the timeout
    set_timeout();

    printf(
      "Child(%d): Retransmitting segment %d retransmit number %d\n", 
      pid, 
      prev_ack, 
      retransmits);
  }
}

void send_probe_dg() {
  datagram dg;
  dg.header.seq_number = 0;
  dg.header.ack_number = 1;
  dg.header.win_size = 1;
  dg.header.length = 0;
  dg_send_conn(conn_fd, &dg);
}

void handle_timeout() {
  timeout_active = 0;
  rto *= 2;

  if (cli_win_size != 0) {
    ssthresh = (cwnd + 1) / 2;
    cwnd = 1;
    cwnd_acks_left = 0;

    printf(
        "Child(%d): Timed out, doubling rto, cwnd is now %d and ssthresh is now %d\n", 
        pid, 
        cwnd, 
        ssthresh);
    retransmit_oldest_segment();

    duplicate_acks = 0;
  } else {
    printf("Child(%d): Probing client for window size\n", pid);
    send_probe_dg();
    set_timeout();
  }
}

void update_cwnd() {
  if (cwnd <= ssthresh) {
    cwnd++;
    printf(
        "Child(%d): Ack processed in slow start phase, cwnd was increased to %d and ssthresh is %d\n", 
        pid, 
        cwnd, 
        ssthresh);
  } else {
    if (cwnd_acks_left == 0) {
      cwnd_acks_left = cwnd;  
    }
    cwnd_acks_left--;
    if (cwnd_acks_left == 0) {
      cwnd++;
      printf(
        "Child(%d): Ack processed in congestion avoidance phase, cwnd was increased to %d and ssthresh is %d\n",
        pid, 
        cwnd,
        ssthresh);
    } else {
      printf(
        "Child(%d): Ack processed in congestion avoidance phase, %d acks left until cwnd increases\n",
        pid, 
        cwnd_acks_left);
    }
  }
}

void handle_ack() {
  datagram ack_dg;
  send_win_entry *entry;
  int ack_number;
  int now;
  int rtt;
  int i;

  if (dg_recv_conn(conn_fd, &ack_dg) <= 0) {
    client_open = 0;
    printf("Child(%d): Client has closed the connection\n", pid);
    return;
  }

  ack_number = ack_dg.header.ack_number;
  cli_win_size = ack_dg.header.win_size;
  printf(
      "Child(%d): Ack received with next expected segment %d and advertised window size %d\n", 
      pid,
      ack_number,
      cli_win_size);

  if (ack_number >= next_ack) {
    now = time_in_millis();
    for (i = next_ack; i <= ack_number; i++) {
      entry = cbuff_remove(&send_win);
      if (!entry->was_retransmitted) {
        rtt = now - entry->sent_at;
        rto_update(rtt);
        printf(
           "Child(%d): Updated RTO with a RTT of %dms for segment %d, new uncapped RTO is %dms\n",
           pid,
           rtt,
           i,
           rto);
      }
      free(entry);
      update_cwnd();
    }
    printf(
        "Child(%d): Removed %d segment(s) numbered %d through %d from the sending window\n",
        pid,
        ack_number - next_ack + 1,
        next_ack - 1,
        ack_number - 1);
    printf("Child(%d): There are now %d segments in the sending window\n", pid, send_win.length);

    // Reset the timeout
    set_timeout();

    // Reset duplicate ack counter
    prev_ack = ack_number;
    duplicate_acks = 0;

    // Reset retransmit counter
    retransmits = 0;

    // Update the next expected ack
    next_ack = ack_number + 1;
  } else if (ack_number == prev_ack) {
    duplicate_acks++;
    retransmits = 0; 
    if (duplicate_acks >= DUPLICATE_ACKS_BEFORE_RTO) {
      ssthresh = (cwnd + 1) / 2;
      cwnd = ssthresh;
      cwnd_acks_left = 0;

      printf(
          "Child(%d): Received %d duplicate acks for segment %d, cwnd is %d and ssthresh is %d\n", 
          pid,
          duplicate_acks, 
          ack_number,
          cwnd,
          ssthresh);

      retransmit_oldest_segment();

      duplicate_acks = 0;
    } else {
      printf(
        "Child(%d): Duplicate ack for segment %d, now have %d duplicate acks\n", 
        pid, 
        ack_number, 
        duplicate_acks);
    }
  }
}

void wait_for_ack() {
  fd_set listen_set;
  sigset_t empty_set;
  int nready;

  FD_ZERO(&listen_set);
  FD_SET(conn_fd, &listen_set);

  sigemptyset(&empty_set);

  nready = pselect(conn_fd + 1, &listen_set, NULL, NULL, NULL, &empty_set);
  if (nready > 0 && FD_ISSET(conn_fd, &listen_set)) {
    if (client_open) {
      handle_ack();
    }
  } else if (nready == -1 && errno == EINTR) {
    handle_timeout();
  } else {
    err_sys("pselect error");
  }
}

void transmit() {
  do {
    send_new_segments();
    if (!timeout_active) {
      set_timeout();
    }
    wait_for_ack();
  } while (!(num_sent == 0 && send_win.length == 0 && feof(file)) 
      && retransmits <= MAX_RTO_COUNT
      && client_open);
}

void send_termination_dg() {
  datagram dg;
  dg.header.ack_number = -1;
  dg.header.win_size = 1;
  dg.header.seq_number = 0;
  dg.header.length = -1;
  dg_send_conn(conn_fd, &dg);
}

int run(listen_interface creator_interface, struct sockaddr_in cli_addr, char *connect_msg) {
  interface = creator_interface;
  client_addr = cli_addr;
  strcpy(file_name, connect_msg);
  pid = getpid();

  cbuff_init(&send_win, sizeof(send_win_entry *), config.window_size);
  next_seq = 0;
  next_ack = next_seq + 1;
  cli_win_size = INT_MAX;
  cwnd = 1;
  ssthresh = cli_win_size;
  cwnd_acks_left = 0;
  prev_ack = 0;
  duplicate_acks = 0;
  retransmits = 0;
  client_open = 1;
  timeout_active = 0;

  file = fopen(file_name, "r");

  signal(SIGALRM, on_sigalrm);

  printf(
      "Child(%d): Connection opened on IP addr %s fd %d asking for file named %s\n", 
      pid, 
      interface.info->ip_str, 
      interface.fd,
      file_name);
  printf("\n");

  printf("Child(%d): Closing inherited listening sockets...\n", pid);
  close_inherited_sockets();
  printf("\n");

  printf("Child(%d): Analyzing client...\n", pid);
  analyze_client();
  printf("\n");

  printf("Child(%d): Opening connection socket...\n", pid);
  open_connection();
  printf("\n");

  printf("Child(%d): Sending connection socket info to client...\n", pid);
  send_connection_info();
  printf("\n");

  if (file != NULL) {
    printf("Child(%d): Connection established, transmitting data...\n", pid);
    transmit();
    printf("\n");
  } else {
    printf("Child(%d): File not found, sending client termination packet...\n", pid);
    send_termination_dg();
    printf("\n");
  }

  printf("Child(%d): Closing...\n", pid);

  return 0;
}
