#include "common.h"
#include "unpthread.h"

char *ip_server;
char *ip_client;
int port;
char *filename;
int recv_wndw_size;
int seed_value;
float prob;
int mean;
int sockfd;
struct sockaddr_in addr,serv_addr;
int socklen;
int index_wndw;
int last_read_index;
int last_seq_acked;
datagram *recv_buff;
int out_fd;
int shut_dwn;
int ack_expt;
pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;


void readfile(char *file);
char *check_server_ip(struct ip_info*head);
void get_socket(int opt);
void print_list(struct ip_info *curr);
void bind_socket();
void con_server();
void init_handshake();
void reconnect(int nport);
int gen_rand_prob();
datagram create_datagram(int seq,int ack,char *msg);
void insert_recv_buff(datagram *data);
void init_recv_buff();
void print_recv_buff();
int remove_recv_buff();
int read_recv_buff(int i);
int check_dup_seq(int seq);
int get_ack();
void doit(void *vptr);
void receive_data();
void setup_output_file();

int main(){
  
  struct ip_info *root;
  pthread_t thread;
  socklen=sizeof(struct sockaddr_in);

  readfile("client.in");
  srand(seed_value);
  init_recv_buff();

  root=make_list();

  print_list(root);

  ip_client=check_server_ip(root);

  if(ip_client!=NULL){
    get_socket(1);
  }
  else{
    printf("The server is not on the same network\n");
    ip_client=root->ip_str;
    get_socket(0);
  }

  bind_socket();
  
  con_server();

  setup_output_file();

  Pthread_create(&thread,NULL,&doit,NULL);

  init_handshake();

  receive_data();

  Pthread_join(thread,NULL);

  //free all pointers
  free(ip_server);
  free(filename);
  free(ip_client);
  free_list(root);
  free(recv_buff);
  close(sockfd);
  close(out_fd);
  return 0;
}


char* check_server_ip(struct ip_info *head){
  struct sockaddr_in sa;
  struct in_addr serv;
  int n;
  //char str[INET_ADDRSTRLEN];

  n=inet_pton(AF_INET,ip_server,&(sa.sin_addr));

  if(n<=0){
    printf("Inet pton error, the server ip might not be in ivp4 version\n");
    exit(1);
  }
 
  serv.s_addr=sa.sin_addr.s_addr;

  while(head){
    
    long a= head->ip.s_addr^serv.s_addr; //XOR IP address
    long b= head->netmask.s_addr & serv.s_addr; //BITAND netmask and server ip

    //Client and server on same network and same ip
    if(a==0 && b==head->subnet.s_addr){
      printf("The server has the same ip as client\n");
      printf("The server is on the same network\n");
      ip_server="127.0.0.1";
      return "127.0.0.1";
    } 
    else if(a!=0 && b==head->subnet.s_addr){
      printf("The server has a different ip than client\n");
      printf("The server is on the same network\n");
      return head->ip_str;
    }

    else{
      head=head->next;
    }

  }  
  return NULL;
       
}

void readfile(char *file){

  char line[200];
  int fd;
  int n;
  int count=1;
  fd=open(file,O_RDONLY);

  if(fd<0){
    perror("Error :");
    printf("The error no is %d\n",errno);
    exit(1);
  }
  

  while((n=Readline(fd,line,sizeof(line)))>0){
    
    if(count==1){
      ip_server=(char*)malloc(strlen(line)-1);
      strncpy(ip_server,line,strlen(line)-1);
      ip_server[n]='\0';
      printf("The server ip is %s %d\n",ip_server);      
    }
    
    if(count==2){
      port=atoi(line);
      printf("The port number is %d\n",port);
    }

    if(count==3){
      filename=(char*)malloc(strlen(line)-1);
      strncpy(filename,line,strlen(line)-1);
      printf("The file name to be copied is %s\n",filename);
    }

    if(count==4){
      recv_wndw_size=atoi(line);
      printf("The receiving window size is %d\n",recv_wndw_size);
    }

    if(count==5){
      seed_value=atoi(line);
      printf("The random generator seed value is %d\n",seed_value);
    }

    if(count==6){
      prob=atof(line);
      printf("The probability value is %0.2f\n",prob);
    }

    if(count==7){
      mean=atoi(line);
      printf("The mean is %d\n\n",mean);
    }
    count++;
    
  }

  close(fd);

}

void setup_output_file(){
  char *temp=(char*)malloc(strlen(filename)+1);
  strncpy(temp,filename,strlen(filename));
  char *ptr=strtok(temp,".");
  char *type=strtok(NULL,".");
  char *buff= (char*)malloc(strlen(ptr)+strlen(type)+4);
  strcpy(buff,ptr);
  strcat(buff,"cp.");
  strcat(buff,type);
  
  FILE *fp=fopen(buff,"w+");
  out_fd=fileno(fp);

  if(fp==NULL || out_fd<0){
    err_sys("Output file open error");
    }
  free(temp);
}


void get_socket(int opt){

  int optval=1;
  
  if((sockfd=socket(AF_INET,SOCK_DGRAM,0))<0){
    perror("Error:");
    printf("The error no is %d\n",errno);
    exit(1);
  }

  memset(&addr,0,sizeof(struct sockaddr_in));
  addr.sin_family=AF_INET;
  addr.sin_port=0;
  char *temp=ip_client;
  if(inet_pton(AF_INET,temp,&addr.sin_addr)<=0){
    printf("Error with inet_pton");
  }
 

  //Setup msg dont route sock option
  if(opt==1){
    int n=setsockopt(sockfd,SOL_SOCKET,SO_DONTROUTE,&optval,sizeof(optval));
    if(n<0){
      perror("Error :");
      printf("The error no is %d\n",errno);
    }
    printf("Setting the SO_DONTROUTE option\n");
  }

}

void print_list(struct ip_info *curr){

  printf("The sorted list\n\n");
  while(curr){
    print_ip_info(curr);
    printf("\n");
    curr=curr->next;
  }

}

void bind_socket(){
  if(bind(sockfd,(struct sockaddr*)&addr,sizeof (struct sockaddr_in))<0){
    printf("The errno is %d\n",errno);
    err_sys("Bind error");
  }

  if( getsockname(sockfd,(struct sockaddr*)&addr,&socklen)<0){
    printf("The error no is %d\n",errno);
    err_sys("getsockname error\n");
  }
  printf("\n\nClient ip address is %s, and port number bound to is %d\n",ip_client, addr.sin_port);


}


void con_server(){
  char str[INET_ADDRSTRLEN];
  struct sockaddr_in temp;

  memset(&serv_addr,0,socklen);

  serv_addr.sin_family=AF_INET;
  serv_addr.sin_port=htons(port);
  
  printf("\n\nAttempting to connect to server at IP %s and port %d\n",ip_server,port);

  if(inet_pton(AF_INET,ip_server,&serv_addr.sin_addr)<=0){
    err_sys("inet_pton error");
  }

  if(connect(sockfd,(struct sockaddr*)&serv_addr,socklen)<0){
    printf("The errno is %d\n",errno);
    err_sys("Connection error\n");
  }

  if(getpeername(sockfd,(struct sockaddr*) &temp,&socklen)<0){
    printf("The error no is %d\n",errno);
    err_sys("Get peer name error\n");
  }

  
  printf("Connection completed\n");
  printf("The peer name is %s\n",inet_ntop(AF_INET,&temp.sin_addr,str,socklen));
  printf("The peer port number is %d\n",(int)ntohs(temp.sin_port));
}


void reconnect(int nport){

  struct sockaddr_in temp;
  int len=sizeof(temp);
  char str[INET_ADDRSTRLEN];
  memset(&temp,0,len);

  temp.sin_family=AF_UNSPEC;

  if(connect(sockfd,(struct sockaddr*)&temp,len)<0){
    printf("The errno is %d\n",errno);
    err_sys("Connect error:");
  }

  serv_addr.sin_port=htons(nport);

  if(connect(sockfd,(struct sockaddr*)&serv_addr,socklen)<0){
    err_sys("Reconnect error:");
  }

  printf("\n\nReconnect Connection completed\n");
  printf("The peer name is %s\n",inet_ntop(AF_INET,&serv_addr.sin_addr,str,socklen));
  printf("The peer port number is %d\n\n",(int)ntohs(serv_addr.sin_port));

}

int gen_rand_prob(){
  double num= (double)rand()/(double)RAND_MAX;
  int ret;
  printf("The random probability generated is %0.2f\n",num);

  if(num>prob){
    //printf("The packet will not be dropped\n\n");
    ret=1;
  }
    
  if(num<prob){
    //printf("The packet will be dropped\n\n");
    ret=0;
  }
  return ret;
}


datagram create_datagram(int seq,int ack,char *buff){

  datagram data;
  int temp=recv_wndw_size;

  memset(&data,0,sizeof(DATAGRAM_SIZE));
  data.header.seq_number=seq;
  data.header.ack_number=ack;

  // We need to get the window size from the last acked datagram
  //and make sure the buffer is not full
  if(index_wndw<recv_wndw_size){
    temp=ack%recv_wndw_size;
    if(temp==last_read_index){
      temp=0;
      }
    else if(temp<last_read_index){
      temp=recv_wndw_size-last_read_index+temp;
    }
    else if(temp>last_read_index && last_read_index!=0){
      temp=temp-last_read_index;
    }
  }
 
  
  data.header.win_size=recv_wndw_size-temp; 

  strncpy(data.message,buff,DATAGRAM_MESSAGE_SIZE);
  
  return data;
}



void init_handshake(){

  fd_set fset;
  struct sockaddr_in temp;
  struct timeval timeout;
  timeout.tv_sec=HANDSHAKE_TIMEOUT;
  FD_ZERO(&fset);
  int ready;
  char buff[DATAGRAM_MESSAGE_SIZE];

  printf("The file name is %s\n",filename);
  if(send(sockfd,filename,strlen(filename),0)<0)
    err_sys("send error");

  while(1){
    FD_SET(sockfd,&fset);
    ready=select(sockfd+1,&fset,NULL,NULL,&timeout);

    if(ready>0){
      if(FD_ISSET(sockfd,&fset)){
	printf("\n\nReceived an initial handshake from server\n");
        if(recvfrom(sockfd,buff,sizeof(buff),0,(struct sockaddr*)&temp,&socklen)>0){
	  if(gen_rand_prob()>0){
	    printf("The sever sent  %s\n",buff);
	    int nport=atoi(buff);
	    reconnect(nport);
	    char win_size[MAXLINE];
	    sprintf(win_size,"%d",recv_wndw_size);
	    if(send(sockfd,win_size,strlen(win_size),0)<0){
	      err_sys("re send error:");
	    }
	    break;
	  }
	}
	else{
	  err_sys("Recv Error:");
	}
      }

    }//end of fd_isset
      
    else if(ready==0){
      printf("The message times out\n");
     if( send(sockfd,filename,strlen(filename),0)<0)
       err_sys("send error");
    }

    else if(ready==-1){
      err_sys("Select error");
    }
  }


}


void receive_data(){
  
  struct sockaddr_in temp;
  datagram dg,ack_info;
  char *str="Thank you";
  fd_set  fset;
  FD_ZERO(&fset);
  struct timeval timeout;
  timeout.tv_sec=3;
  int counter=0;
  while(1){

    FD_SET(sockfd,&fset);
    int n=select(sockfd+1,&fset,NULL,NULL,&timeout);

    if(shut_dwn==1)
      break;

    if(FD_ISSET(sockfd,&fset)&& n>0){
      if(dg_recv(sockfd,&dg,&temp,&socklen)>0){	
	if(gen_rand_prob()>0){
	  printf("Received datagram with seq %d\n",dg.header.seq_number);
	  Pthread_mutex_lock(&lock);
	  if(dg.header.seq_number>last_seq_acked && check_dup_seq(dg.header.seq_number)>0 ){
	    insert_recv_buff(&dg);
	  }
	  int k=get_ack();
	  Pthread_mutex_unlock(&lock);
	  if(gen_rand_prob()>0){
	    ack_info=create_datagram(dg.header.seq_number,k,str);
	    printf("Ack sent %d and win size %d\n\n",k,ack_info.header.win_size);
	    // printf("The last index read %d and last seq acked %d and index_wndw %d\n\n",last_read_index,last_seq_acked,index_wndw);
	    dg_send_conn(sockfd,&ack_info);
	  }
	  
	  else
	  printf("The ack was dropped\n\n");
	}//end of if for error prob condition
	else
	  printf("The packet was dropped\n\n");
      }//end of read from dg_recv
    }

    if(n==0){
      printf("The client timed out and did not read anything\n");
      printf("Sending a dummy ack with ack number %d\n",ack_expt);
      datagram dg=create_datagram(0,ack_expt,str);
      dg_send_conn(sockfd,&dg);
      counter++;
      if(counter==12 && ack_expt==0){
	printf("Nothing send from the server, the client is going to close\n");
	exit(0);
      }
    }

    if(n<0){
      err_sys("Select error");
    }

  }

}

int check_dup_seq(int seq){
  int i=seq%recv_wndw_size;
  if((recv_buff+i)->header.seq_number==seq && seq!=0){
    return 0;
  }
  else 
    return 1;
}


void insert_recv_buff(datagram *data){
  int i=data->header.seq_number% recv_wndw_size;
  //Pthread_mutex_lock(&lock);
  if(index_wndw<recv_wndw_size && (recv_buff+i)->header.seq_number==0 && (recv_buff+i)->header.ack_number==0 
     && (recv_buff+i)->header.win_size==0){
    memcpy((recv_buff+i),data,DATAGRAM_SIZE);
    index_wndw++;
  }
  //Pthread_mutex_unlock(&lock);
}


void init_recv_buff(){
  int i;
  recv_buff=(datagram*)calloc(recv_wndw_size, DATAGRAM_SIZE);

  if(recv_buff==NULL){
    err_sys("Malloc error:");
  }

  
  for(i=0;i<recv_wndw_size;i++){
    (recv_buff+i)->header.seq_number=0;
    (recv_buff+i)->header.ack_number=0;
    (recv_buff+i)->header.win_size=0;
    }

  index_wndw=0;
  last_read_index=0;
  last_seq_acked=-1;
  shut_dwn=0;
}

void print_recv_buff(){

  int i=0;
  for(i=0;i<recv_wndw_size;i++){
    datagram data=*(recv_buff+i);
    dg_print(&data);
  }

}

int get_ack(){
  
  int i=0;
  int ret=0;
  //Pthread_mutex_lock(&lock);
  for(i=last_read_index;i<recv_wndw_size;i++){
    if((recv_buff+i)->header.seq_number==0 && (recv_buff+i)->header.ack_number==0 && 
       (recv_buff+i)->header.win_size==0)
      break;
  }
  if(i==0 && last_seq_acked==0){
    ret=0;
  }
  
  if(i==0 && last_seq_acked!=0){
    ret=last_seq_acked+1;
  }

  if(i!=0 && i==last_read_index){
    ret=ack_expt;
  }

  if(i==recv_wndw_size){
    int j=0;
    if(((recv_buff+j)->header.seq_number==0 && (recv_buff+j)->header.ack_number==0 &&
       (recv_buff+j)->header.win_size==0) || last_read_index==0){
      i--;
      datagram data=*(recv_buff+i);
      ret=data.header.seq_number;
      ret++;
    }
    else{
      for(j=1;j<last_read_index;j++){
	if((recv_buff+j)->header.seq_number==0 && (recv_buff+j)->header.ack_number==0 &&
	   (recv_buff+j)->header.win_size==0){
	  break;
	}
      }
      j--;
      ret=(recv_buff+j)->header.seq_number;
      ret++;
    }
  }
  
  else if(i!=recv_wndw_size && i!=0 && i!=last_read_index){
    i--;
    if((recv_buff+i)->header.ack_number==0 && (recv_buff+i)->header.seq_number==0 &&
       (recv_buff+i)->header.win_size==0){
      
      ret=last_seq_acked+1;
    }
      
    else{  
      datagram data=*(recv_buff+i);
      ret=data.header.seq_number;
      ret++;
    }
   }
  ack_expt=ret;
  return ret;
  //Pthread_mutex_unlock(&lock);

 
}


int remove_recv_buff(){

  int i=0;
  int seq_read_num=-1;
  int seq;
  Pthread_mutex_lock(&lock);
 
 //Means buffer is not empty
  if(index_wndw>0){
    for(i=last_read_index;i<recv_wndw_size;i++){
      if((recv_buff+i)->header.seq_number==0 && (recv_buff+i)->header.ack_number==0 &&
	(recv_buff+i)->header.win_size==0){
	break;
      }
      else{
	seq=(recv_buff+i)->header.seq_number;
	if(read_recv_buff(i)<0){
	  if(seq_read_num==-1)
	    seq_read_num++;
	  printf("\n\nDatagram read from sequence number %d to %d\n\n",(seq-seq_read_num),seq);
	  return -1;
	}
	seq_read_num++;
      }
	
    }
  
    if(i!=recv_wndw_size)
      last_read_index=i;
    if(i==recv_wndw_size){
      int j=0;
      for(j=0;j<last_read_index;j++){
	if((recv_buff+j)->header.seq_number==0 && (recv_buff+j)->header.ack_number==0 && 
	   (recv_buff+j)->header.win_size==0){
	  break;
	}
	else{
	  seq=(recv_buff+j)->header.seq_number;
	  if(read_recv_buff(j)<0){
	    if(seq_read_num==-1)
	      seq_read_num++;
	    printf("\n\nDatagram read from sequence number %d to %d\n\n",(seq-seq_read_num),seq);
	    return -1;
	  }
	  seq_read_num++;
	}
      }
      
      last_read_index=j;
    } 
  }
  if(seq_read_num>-1)
    printf("\n\nDatagram read from sequence number %d to %d\n\n",(seq-seq_read_num),seq);
  Pthread_mutex_unlock(&lock);
  return 0;
}


int read_recv_buff(int i){
  char message[DATAGRAM_MESSAGE_SIZE + 1];
  datagram dg=*(recv_buff+i);
  write(out_fd,dg.message,dg.header.length);
  dg_print(&dg);
  strncpy(message, dg.message, DATAGRAM_MESSAGE_SIZE);
  message[DATAGRAM_MESSAGE_SIZE] = 0;
  printf("\n%s\n\n",message);
  if(dg.header.ack_number==-1){
    if(dg.header.length==-1)
      printf("\nThe file does not exists\n");
    return -1;
  }
  last_seq_acked=(recv_buff+i)->header.seq_number;
  (recv_buff+i)->header.seq_number=0;
  (recv_buff+i)->header.ack_number=0;
  (recv_buff+i)->header.win_size=0;
  memset(&(recv_buff+i)->message,0,DATAGRAM_MESSAGE_SIZE);
  index_wndw--;

  return 0;
}


void doit(void *vptr){

  while(1){
    double rand=drand48();
    double time=-1*mean*(log(rand));
    time=time*1000;
    int x=0;
    struct timeval timeout;
    timeout.tv_usec=time;
    select(0,NULL,NULL,NULL,&timeout);
    if(index_wndw==recv_wndw_size)
      x=1;
 
    if(remove_recv_buff()<0){
      shut_dwn=1;
      break;
    }
    if(x==1){
      datagram dg;
      dg.header.ack_number=last_seq_acked+1;
      dg.header.seq_number=last_seq_acked;
      dg.header.win_size=recv_wndw_size;
      dg_send_conn(sockfd,&dg);
    }
  }
}
