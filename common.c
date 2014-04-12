#include "common.h"
#include "unpifiplus.h"

int get_netmask_len(struct in_addr mask){
  char dst [INET_ADDRSTRLEN];
  char *tok;
  int len=0;
  
  inet_ntop(AF_INET,&mask,dst,sizeof(dst));

  if(dst==NULL){
    printf("Inet ntop error\n");
    return -1;
  }

  tok=strtok(dst,".");

  while(tok!=NULL){

    int num=atoi(tok);
    

    if(num==255){
      len=len+8;
    }

    else{
      int c;
      int n;
      for(c=31;c>=0;c--){
  n=num >> c;
  if(n&1)
    len++;
      }
    }
    tok=strtok(NULL,".");
  }
  return len;

}


struct ip_info * insert_into_list(struct ip_info *head,struct ip_info *node){

  struct ip_info *temp=head;
  struct ip_info *a=(struct ip_info*)malloc(sizeof(struct ip_info));
  struct ip_info *prev;

  memcpy(a,node,sizeof(struct ip_info));
  a->next=NULL;

  if(head==NULL){
    head=a;
    head->next=NULL;
  }
  
  else{
    prev=NULL;
    
    while(temp && temp->netmask_len>=a->netmask_len){
      prev=temp;
      temp=temp->next;
    }

    if(!temp){
      prev->next=a;
    }

    else{
      if(prev){
  a->next=prev->next;
  prev->next=a;
      }
      else{
  a->next=head;
  head=a;
  
      }
    }
   
    }
  return head;
}


struct ip_info * make_list(){

  struct ifi_info *ifihead,*ifi;
  struct sockaddr_in *sa,*sa1;
  struct ip_info *root, curr;
  root=NULL;

  for(ifihead=ifi=Get_ifi_info_plus(AF_INET,1);ifi!=NULL;ifi=ifi->ifi_next){

    if((sa=ifi->ifi_addr)!=NULL){
      curr.ip=sa->sin_addr;
      inet_ntop(AF_INET, &sa->sin_addr, curr.ip_str, INET_ADDRSTRLEN);
    }

    if((sa=ifi->ifi_ntmaddr)!=NULL && (sa1=ifi->ifi_addr)!=NULL){

      curr.netmask=sa->sin_addr;
      curr.netmask_len=get_netmask_len(curr.netmask);


      sa1->sin_addr.s_addr=sa->sin_addr.s_addr & sa1->sin_addr.s_addr;
      curr.subnet=sa1->sin_addr;
    }
    curr.next=NULL;
    root=insert_into_list(root,&curr);
  }

  free_ifi_info_plus(ifihead);
  return root;
}




void print_ip_info(struct ip_info*root){

  char netmask[INET_ADDRSTRLEN];
  char subnet[INET_ADDRSTRLEN];

  inet_ntop(AF_INET,&root->netmask,netmask,sizeof(netmask));
  inet_ntop(AF_INET,&root->subnet,subnet,sizeof(subnet));

  printf("The ip address is %s\n",root->ip_str);
  printf("The network mask is %s\n",netmask);
  printf("The netmask length is %d\n",root->netmask_len);
  printf("The subnet address is %s\n",subnet);

}


void free_list(struct ip_info* root){
  struct ip_info *temp, *curr;
  curr=root;
  temp=root;
  while(curr){
    temp=curr;
    curr=temp->next;
    free(temp);
  }

}

int get_host_location(struct ip_info *source, struct in_addr *dest) {
  if (source->ip.s_addr == dest->s_addr) {
    return HOST_LOCAL;
  } else if (
      (source->ip.s_addr & source->netmask.s_addr) == (dest->s_addr & source->netmask.s_addr)) {
    return HOST_SAME_NETWORK;
  }
  // TODO(mark): Make sure you test that foreign actually works here
  return HOST_FOREIGN;
}
