#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include "utils.h"
#include "message.h"

#define BACKLOG_SIZE 10


static int tcp_listen(char *server_ipaddr, char *server_port);


int main(int argc, char *argv[]){
  socklen_t len;
  struct sockaddr  client1, client2;
  char host[MAX_NAME_SIZE+1];
  /*
   * le serveur ecoute sur le port1 pour recevoir les commandes
   * le serveur écoute sur le port2 pour recevoir les donnees*/
  char port1[MAX_PORT_SIZE+1], port2[MAX_PORT_SIZE+1]; 
  
  /* sock1: est le socket sur lequel le serveur attent les connexions clientes
   *        pour les commandes
   * sock2: est le socket sur lequel le serveur attend les connexions clientes
   *        pour les données
   */
  int sock1, sock2;
  /*
   * cmd_sd: socket pour recevoir les commandes du client lorsque la connexion
   *         est établie
   * data_sd: socket pour recevoir les données du client lorsque la connexion
   *         est établi
   */
  int cmd_sd = -1, data_sd = -1;
  /* pid du processus qui sera créé pour gérer les connexions clientes */
  int child;
  /* caractère pour gérer les options en lignes de commande avec opts*/
  int c;
  /* descripteur du fichier à recevoir par la commande GET*/
  int data_fd = -1;
  /* Variable  pour  récupérer le resultat des appels systèmes*/
  ssize_t ret;
  /* structure de données pour gérer les entrées sortie async sur les socket 
   * sock1 et sock2. A l'aide de l'appel système select le serveur ne fait 
   * l'appel à accept que si il ya une connexion entrante
   */
  int fdmax = 0;
  fd_set readfds;
  fd_set writefds;
  
  /* should_send_cmd: (1) le serveur désire envoyer une commande (0) non
   * should_send_data: (1) le serveur désire envoyer des données (0) non
   */
  int should_send_cmd = 0, should_send_data = 0;
  /* mémoire tampon pour l'envoie et la réception*/
  char buffer[BUF_SIZE+1];
  /* message pour récupérer les requêtes et envoyer les réponses */
  struct message m_in, m_out;
  memset(port1, 0, MAX_PORT_SIZE+1);
  memset(port2, 0, MAX_PORT_SIZE+1);
  memset(host, 0, MAX_NAME_SIZE+1);
  /* Gestion des options en ligne de commande */
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
 
  /* le serveur spécifie au système qu'il écoute les ports port1 et port2*/
  if(!port1[0]  || !port2[0] || !host[0]){
     fprintf(stderr, "usage: %s -h servername -p port1 -P port2\n", argv[0]);
     exit(0);
  }
  sock1 = tcp_listen(host, port1);
  sock2 = tcp_listen(host, port2);
  
  /* les sock1 et sock2 sont non bloquants cela signifie qu'il retourne lorsque
   * la ressource demandée lors d'un appel système n'est pas disponible sans
   * bloquer le processus
   */
  fcntl(sock1, F_SETFL, O_NONBLOCK );
  fcntl(sock2, F_SETFL, O_NONBLOCK );
  /* Initialisation des structures  de données readfds et writefds*/
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  fdmax = MAX(sock1, sock2);
  
  /* Le serveur entre dans une boucle en attente des connexions entrantes */
  while(1){
    /* Configuration des descripteurs de fichiers monitorés par le système à 
     * travers l'appel système select 
     */
    FD_SET(sock1, &readfds);
    FD_SET(sock2, &readfds);
    ret = select(fdmax+1, &readfds, NULL,NULL,NULL);
    /*
     * Une client tente de se connecter sur sock1
     */
    if(FD_ISSET(sock1, &readfds)){
      cmd_sd = accept(sock1, (struct sockaddr*)&client1, &len);
      FD_CLR(sock1, &readfds);
      /* Il faut attendre que sock2 soit prêt */
      if(cmd_sd > 0 && data_sd < 0)
        continue;
    }
    /* Un client tente de se connecter sur sock2 */
    if(FD_ISSET(sock2, &readfds)){
      data_sd = accept(sock2, (struct sockaddr*)&client2, &len);
      FD_CLR(sock2, &readfds);
      /* Il faut attendre que sock1 soit prêt */
      if(cmd_sd < 0 && data_sd > 0)
        continue;
    }
    /* On verifie que les connexion de commande et de données sont prêts */
    if(cmd_sd < 0 || data_sd < 0){
      fprintf(stderr, "failed to accept incomming connection (%s)\n", strerror(errno));
      exit(1);
    }
    
    if(cmd_sd>0 && data_sd>0){
       /* Le serveur a un client avec lequel dialogué pour cela il crée un fils*/
       child = fork();
       if(child < 0){
         exit(1);
       }
       if(child == 0){
         static ssize_t data_sent = 0; /**nbre d'octets dejà envoyés pas le serveur**/
         static int failed_send = 0; /** l'appel système send() a echoué **/
         static ssize_t send_length = 0; /** la taille des données à renvoyer suite à l'échec de send() **/
         ssize_t ds; /*nbre d'octets réellement envoyés par l'appel système send() */
         static ssize_t data_read = 0; /*nombre d'octets réellement lus dans  le fichier */
         static ssize_t offset = 0; /* offset dans le buffer d'envoie */
           
         /* Le serveur prépare les descripteur de fichier qui lui permettrons
          * de discuter avec le client 
          */
         fd_set creadfds;
         fd_set cwritefds;
         FD_ZERO(&creadfds);
         FD_ZERO(&cwritefds);
         int cfdmax = MAX(cmd_sd, data_sd);
         /* Il veille à ce que les sockets soient non bloquant*/
         fcntl(cmd_sd, F_SETFL, O_NONBLOCK );
         fcntl(data_sd, F_SETFL, O_NONBLOCK );
         while(1){
           /* Configuration des descripteurs de fichier pour select */
           FD_SET(data_sd, &creadfds);
           FD_SET(data_sd, &cwritefds);
           FD_SET(cmd_sd, &creadfds);
           FD_SET(cmd_sd, &cwritefds);
           ret = select(cfdmax+1, &creadfds, &cwritefds,NULL,NULL);
           /* Le serveur vient de recevoir une commande d'un client */
           if(FD_ISSET(cmd_sd, &creadfds)){
             msg_receive(cmd_sd, &m_in);
             data_fd = handle_msg(&m_in, &m_out);
             should_send_cmd = 1;
             if(data_fd > 0)
               should_send_data = 1;
             FD_CLR(cmd_sd, &creadfds);
           }
           /*Le serveur s'apprête à envoyer une commande au client */
           if(FD_ISSET(cmd_sd, &cwritefds)){
             if(should_send_cmd){
               /* TODO vérifier que m_out est un message valide */
               msg_send(cmd_sd, &m_out);
               should_send_cmd = 0;
             }
             FD_CLR(cmd_sd, &cwritefds);
           }
           /* le serveur a reçu des donnée du client */
           if(FD_ISSET(data_sd, &creadfds)){
             /* TODO */ 
             FD_CLR(data_sd, &creadfds);
           }
           /* le serveur a des données à envoyer*/
           if(FD_ISSET(data_sd, &cwritefds)){
             /* l'appel système send() avait entre temps échoué, il faut renvoyer
              * les données qui ont échouées et qui sont dans le buffer d'envoie
              */
             if(failed_send && send_length>0){
                ds = send(data_sd, buffer+offset, (size_t)send_length,0);
                if(ds < 0){
                   failed_send = 1;
               }
               else{
                 if((ds == send_length) || !send_length ){
                   failed_send =  0;
                   send_length = 0;
                   offset = 0;
                 }else{
                   send_length-=ds;
                   offset+=ds;
                 }
               }
             }
             
             /* Toutes les données ont été envoyées on peut lire des données 
              * du fichier si on est pas à la fin du fichier 
              */
             if(should_send_data && (data_fd > 0) && !failed_send && !send_length){
               while((ret = read(data_fd, buffer, BUF_SIZE)) > 0){
                 data_read += ret;
                 ds = send(data_sd, buffer, (size_t)ret,0);
                 /** Il n'y a plus d'espace dans le buffer d'envoie de TCP **/
                 if((ds < 0) || (ds < ret)){
                   failed_send = 1;
                   send_length = (ds < 0) ? ret : ret - ds;
                   if(ds < ret){
                     data_sent += ds;
                     offset = ds;
                   }else offset = 0;
                   break;
                 }
                 else{
                   /* toutes les données du buffer d'envoie ont pu être envoyées*/
                   data_sent += ds;
                   failed_send = 0;
                 }
               }
               if(ret == 0){
                 /* Etant à la fin du fichier il faut fermer le fichier*/
                 close(data_fd);
                 should_send_data = 0;
                 data_fd = -1;
               }
               if(ret < 0){
                 /** TODO il faut gérer l'erreur**/
               }
             }
             FD_CLR(data_sd, &cwritefds);
           }
         }
       }
    }
  }
/******************************************************************************/
  return 0;
}

static int tcp_listen(char *server_ipaddr, char *server_port){
  int sd = 0;
  struct sockaddr server_addr;
  socklen_t salen;
  sd = socket(AF_INET, SOCK_STREAM,0);
  if(sd == -1){
    perror("socket échec !!");
    exit(1);
  }
  if(resolve_address(&server_addr, &salen, server_ipaddr, server_port, 
      AF_INET, SOCK_STREAM, IPPROTO_TCP)!= 0){
      fprintf(stderr, "Erreur de configuration de sockaddr\n");
      return -1;
  }
  if(bind(sd, &server_addr, salen) < 0){
    perror("Echec de liaison !!");
    return -1;
  }
  listen(sd,BACKLOG_SIZE);
  return sd; 
}
