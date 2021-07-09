#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "utils.h"
#include "message.h"


#define MAXLINE 200
#define MAX_PORT_SIZE 6
#define MAX_NAME_SIZE 255

static int TCP_Connect(int af, char *server_ipaddr, char *server_port);


int main(int argc, char *argv[]){
  int cmd_sd, data_sd, ret;
  char port1[MAX_PORT_SIZE], port2[MAX_PORT_SIZE];
  char host[MAX_NAME_SIZE];
  char cmd[MAX_NAME_SIZE];
  char params [MAX_NAME_SIZE];
  char c;
  struct message m_out, m_in; 
  fd_set readfds;
  fd_set writefds;
  int fdmax;
  char *input = NULL;
  char buffer[BUF_SIZE];
  int should_send_cmd = 0, data_fd;
  /* Recupération des paramètres de configuration du client et du serveur*/
  while((c = getopt(argc, argv, "p:P:h:")) != -1) {
    switch(c){
      case 'p':
         strncpy(port1, optarg, MAX_PORT_SIZE);
        break;
      case 'P':
        strncpy(port2, optarg, MAX_PORT_SIZE);
        break;
      case 'h':
        strncpy(host, optarg, MAX_NAME_SIZE);
        break;
      default:
        break;  
    }
  }
  /* 
   * Vérification des paramètres de configuration du client et du serveur
   * Connexion au serveur
   * Negociation des paramètres de configuration
   */
  cmd_sd =  TCP_Connect(AF_INET, host, port1);
  data_sd = TCP_Connect(AF_INET, host, port2);
  if(cmd_sd < 0 || data_sd < 0){
    fprintf(stderr, "Echec de la tentative de connection au serveur\n");
    fprintf(stderr, "%s\n", strerror(errno));
  }
  fdmax = MAX(cmd_sd, data_sd);
  /*
   * Echange avec le serveur et l'utilisateur de façon interactive
   */
  fcntl(cmd_sd, F_SETFL, O_NONBLOCK );
  fcntl(data_sd, F_SETFL, O_NONBLOCK );
  while(1){
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(cmd_sd, &readfds);
    FD_SET(cmd_sd, &writefds);
    FD_SET(data_sd, &readfds);
    FD_SET(data_sd, &writefds);
    FD_SET(0, &readfds);
    ret = select(fdmax+1, &readfds, &writefds,NULL,NULL);
    if(FD_ISSET(0, &readfds)){
      input = readline(">");
      if(input!=NULL){
        add_history(input);
        sscanf(input, "%s %s\n", cmd, params);
        
        if(!strcmp(cmd, "get") || !strcmp(cmd, "GET")){
          /* Commande GET */
          if(!strcmp(params, "")){
            fprintf(stderr, "parametre obligatoire\n");
          }
          else{
            data_fd = open(params, O_CREAT | O_WRONLY);
            if(data_fd < 0){
              fprintf(stderr, "Erreur I/O: %s\n", strerror(errno));
              FD_CLR(0, &readfds);
              continue;
            }
            m_out.opcode = GET;
            m_out.params_len = strlen(params);
            m_out.result_str_len = 0;
            strcpy(m_out.params, params);
            should_send_cmd = 1;
            
          }
        } else{
          /* TODO  Commande PUT, etc*/
          fprintf(stderr, "commande inconnue\n");
        }
        memset(cmd, 0, MAX_NAME_SIZE);
        memset(params, 0, MAX_NAME_SIZE);
        free(input);
      }
    }
    if(FD_ISSET(cmd_sd, &readfds)){
      msg_receive(cmd_sd, &m_in);
      /* TODO il faut vérifier que m_in est valide */
      fprintf(stderr, "%s\n", m_in.result_str);
      FD_CLR(cmd_sd, &readfds);
    }
    if(FD_ISSET(cmd_sd, &writefds)){
      if(should_send_cmd){
        /* TODO il faut verifier que m_out est valide */
        msg_send(cmd_sd, &m_out);
        should_send_cmd = 0;
      }
      FD_CLR(cmd_sd, &writefds);
    }
    int val = 0;
    if(FD_ISSET(data_sd, &readfds)){
       do{
         while((ret=recv(data_sd, buffer, BUF_SIZE,0)) < 0 && (errno==EINTR) );
         if(ret < 0){
           /* TODO il faut gérer */
         }
         if(ret > 0)
           val = write(data_fd, buffer, ret);
         if(ret == 0){
           fprintf(stderr, "Aucune donnée réçu %s (%d:%d)\n", strerror(errno), errno, val);
           /* TODO il faut gérer */
         }
       }while(ret>0);
       FD_CLR(data_sd, &readfds);
    }
    
    if(FD_ISSET(data_sd, &writefds)){
       /* TODO il faut gérer */
       FD_CLR(data_sd, &writefds);
    }    
  }
}



static int TCP_Connect(int af, char *server_ipaddr, char *server_port)
{
  int ret, sd;
  struct sockaddr server_addr;
  socklen_t salen;
  if ((sd = socket(af, SOCK_STREAM, 0)) < 0){
    return sd;
  }
  if(resolve_address(&server_addr, &salen, server_ipaddr, server_port, 
      AF_INET, SOCK_STREAM, IPPROTO_TCP)!= 0){
      fprintf(stderr, "Erreur de configuration de sockaddr\n");
      return -1;
  }
  if((ret = connect(sd, &server_addr, salen) ) < 0){
    fprintf(stderr, "Ici haha (%d) (%s:%s)\n", ret, strerror(errno), server_port);
    return ret;
  }
  fprintf(stderr, "con ret %d:%s:%d\n", ret, server_port,sd);
  return sd;
}
