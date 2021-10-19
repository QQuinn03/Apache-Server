/*
* CS 656 / Fall 2019 / Term Project
* Group: M3 / Leader: Lee (lrz2)
* Group Members: Qiong (qq24), Sanyamee (smp239), Kevin (kh388), Jessica (jlc39)
*
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <net/if.h>
#include <stdarg.h>
#include<time.h>
#include <sys/timeb.h>
#include <sys/time.h>
/*--------------- end include/s --------------*/

char HOST[1024]; 
int  PORT ;     
struct in_addr PREFERRED; 
struct addrinfo *addrinfostructs;

void printErr(){
  perror("Error :");
  exit(0);
}

int parse(int connfd, char cliMsg[],int msgLen, char reqMsg[]){
  int i;
  if(msgLen <= 0){
    printErr();
    return -1; }
  for(i=0; i<msgLen-6; i++)
    reqMsg[i]=cliMsg[i+4];
  send(connfd, "\nREQ: ", strlen("\nREQ: "), 0);
  send(connfd, reqMsg, strlen(reqMsg), 0);
  return 1;
}

//FUNCTION TO CREATE A SOCKET AND BIND IT TO THE CLIENT 
int socket_listenfd(int port){
  int listenfd, optval=1;
  struct sockaddr_in servAddr;
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd <0)
    return -1; 
  if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0)
   return -1;  
  bzero((char*) &servAddr, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons((unsigned short) port);
  if( bind(listenfd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
    return -1;
  if(listen(listenfd,10)<0)
    return -1;
  return listenfd;
}

int dns(char* req, int connfd)  /* you can define the signature for dns() */{
  int dnsErr = 0, preferredIP=0, ipCtr=0, ressockfd, loopCtr=0;
  struct addrinfo *temp;
  char siteIP[100][1024];
  int least = 9999999, pingTime;
  struct timeval begin, end;
  struct addrinfo info;
  void *addrPtr;
  info.ai_family = AF_UNSPEC;
  info.ai_socktype = SOCK_STREAM;
  info.ai_protocol = 0;
  info.ai_addrlen = 0;
  info.ai_addr = NULL;
  info.ai_canonname = NULL;
  info.ai_next = NULL;
  if((dnsErr = getaddrinfo(req, "80", &info, &addrinfostructs)) !=0){
    printf("\nREQ: %s / RESP: ERROR",req);
    send(connfd, "\nIP : ERROR (Could not find IP)\n", strlen("\nIP : ERROR (Could not find IP)\n"), 0);
  }else{
    temp=addrinfostructs;
    ipCtr=0;
    while(temp){
 	    inet_ntop (temp->ai_family,temp->ai_addr->sa_data,siteIP[ipCtr],1024);
      if(temp->ai_family == AF_INET)
        addrPtr = &((struct sockaddr_in*)temp->ai_addr)->sin_addr;
      else
        addrPtr = &((struct sockaddr_in6*)temp->ai_addr)->sin6_addr;
      strncpy(siteIP[ipCtr],"",sizeof(siteIP[ipCtr]));
      inet_ntop(temp->ai_family, addrPtr, siteIP[ipCtr], 1024);
      ressockfd = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol);
      gettimeofday(&begin, NULL);
      connect(ressockfd,temp->ai_addr, temp->ai_addrlen);  //CREATE A STRUCTURE SiMILAR TO THAT OF ACCEPT
      gettimeofday(&end, NULL);
      pingTime = (int)(end.tv_usec - begin.tv_usec);
      //printf("\nBegin time: %d , End time: %d :: Pingtime= %d", (int)begin.tv_usec, (int)end.tv_usec, pingTime);
      if(pingTime<least){
          least = pingTime;
          preferredIP = ipCtr;
          PREFERRED = ((struct sockaddr_in *)temp->ai_addr)->sin_addr;
      }
      ipCtr++;
      temp= temp->ai_next;
    }//end while to set PREFERRED and save all IPs
    loopCtr=0;
    while((siteIP[loopCtr][0])!= '\0'){
      send(connfd, "\nIP: ", strlen("\nIP: "), 0); 
      send(connfd, siteIP[loopCtr], strlen(siteIP[loopCtr]), 0);
      if(loopCtr==preferredIP){
        send(connfd, " (PREFERRED)", strlen(" (PREFERRED)"), 0);
        printf("\nREQ: %s / RESP: %s", req, siteIP[loopCtr] );}
      strncpy(siteIP[loopCtr], "", sizeof(siteIP[loopCtr]));
      loopCtr++;
    }///end while to print IPs at client
    least=9999999;
  }//end if-else dns found
  send(connfd, "\n", strlen("\n"), 0);
  close(connfd);
  return 1;
}

int main(int argc, char **argv)
{
  int listenfd, connfd, ctr=1, msgLen, cliPORT, parseErr=0, dnsErr =0;
  struct sockaddr_in cliAddr, servAddr;
  socklen_t  cliLen = sizeof(cliAddr);
  socklen_t servLen = sizeof(servAddr);
  char cliMsg[1024], reqMsg[1024];
  char *CLIENT;
  
  setbuf(stdout, NULL);
  if (argc > 1){ //checking if the user put an argument
    PORT = atoi(argv[1]); 
    //creating socket to listen to client
    listenfd=socket_listenfd(PORT);
    if(listenfd == -1)
      printErr();
    else
      printf("Apache Listening on socket %d\n", PORT);
  }
  else
    printErr();

  //WHILE LISTENING TO CLIENT
  while(1){
    cliLen = sizeof(cliAddr);
    if ((connfd = accept(listenfd, (struct sockaddr *)&cliAddr, &cliLen)) < 0)
      printErr();
    //Getting IP of incoming client
    getsockname(connfd , (struct sockaddr *)&servAddr, &servLen);
    inet_ntop(AF_INET, &(servAddr.sin_addr), HOST, 1024);
    CLIENT = inet_ntoa(cliAddr.sin_addr);
    cliPORT = (int)(cliAddr.sin_port);
    printf("\n(%d) Incoming client connection from %s:%d to me %s:%d", ctr, CLIENT, cliPORT, HOST, atoi(argv[1]));  ////CHECK 
    ctr++;
    strncpy(cliMsg, "", sizeof(cliMsg));
    msgLen = recv(connfd, cliMsg, 1024, 0);
    strncpy(reqMsg, "", sizeof(reqMsg));  
    if((parseErr = parse(connfd, cliMsg, msgLen, reqMsg)!=1))
      printErr();
    if((dnsErr = dns(reqMsg, connfd))!=1)
      printErr();
  }
  close(listenfd);
  return 0;
}