#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/select.h>

int main(int argc, char *argv[]){
   
   fd_set readset, writeset;
   char buff[255];
   int ret;
   int should_write = 0;
   FD_ZERO(&readset);
   FD_ZERO(&writeset);
   FD_SET(0, &readset);
   FD_SET(1, &writeset);
   
   fcntl(0, F_SETFL, O_NONBLOCK );
   fcntl(1, F_SETFL, O_NONBLOCK );
   while(1){
      FD_SET(0, &readset);
      FD_SET(1, &writeset);
      ret = select(2, &readset, &writeset, NULL, NULL);
      if(FD_ISSET(0, &readset)){
         fprintf(stderr, "Quelque chose à lire\n");
         ret = read(0, buff, 255);
         if(ret>0)
             fprintf(stderr, "%s", buff);
      }
      if(FD_ISSET(1, &writeset)){
          if(should_write){
              fprintf(stderr, "Quelques chose à écrire\n");
              int n = write(1, "Bonjour", 8);
              if (n < 0){
                  fprintf(stderr, "Une erreur\n");
                  //while(1);
             }
          }
      }
      FD_CLR(0, &readset);
      FD_CLR(1, &writeset);
   }
}
