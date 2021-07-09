#ifndef _MESSAGE_H
#define _MESSAGE_H

#include <sys/types.h>          
#include <sys/socket.h>
#include <stdbool.h>
/*Definitions needed by clients and servers*/
#define TRUE            1
#define MAX_PATH      255 /*Maximum length of file name*/
#define BUF_SIZE     8192 /* How much data to transfer at once*/
#define E_BAD_OPCODE -1  /* Unknown operation requested*/
#define E_BAD_PARAM -2   /* error in a parameter*/
#define E_IO        -3   /* disk error or other I/O error*/

/* definitions of the allowed operations*/
enum{
  OK,             /*Operation performed correctly*/
  CREATE,         /*Create a new file*/
  READ,          /* read date from a file and return it*/
  WRITE,          /* write data to a file*/
  DELETE,         /* delete an existing file*/
  PUT,
  GET,
  PWD,
  LPWD,
  LS,
  LLS,
  CD,
  LCD          
};

/*definition of the message format*/
struct message{
  long opcode;     /* requested operation */
  long result;     /* result of the operation */
  long params_len;   /* name len */
  long result_str_len;
  char params[MAX_PATH];  /*name of the file being operated on */
  char result_str[MAX_PATH];
};

int msg_receive(int from, struct message *m);
int msg_send(int to, struct message *m);
int resolve_address(struct sockaddr *sa, socklen_t *salen, const char *host,
   const char *port, int family, int type, int proto);
double gettime_ms(void);
void init_params(int recv_log_, int sent_log_);
int set_recv_data(int recv_data);
int print_recv_log(void);
int print_sent_log(void);
int handle_msg(struct message *m_in, struct message *m_out, int data_sd);
#endif
