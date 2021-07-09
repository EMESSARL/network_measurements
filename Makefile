all: utils.o client.o server.o server client

utils.o: utils.c
	gcc -c  -Wall utils.c -o utils.o

message.o: message.c
	gcc -c  -Wall message.c -o message.o

client.o: client.c
	gcc -c  -Wall client.c -o client.o

server.o: server.c
	gcc -c  -Wall server.c -o server.o

server: utils.o message.o server.o
	gcc -g utils.o message.o server.o -o server 

client: utils.o client.o
	gcc -g utils.o message.o client.o -o client -lreadline
	

clean:
	rm *.o
	rm client
	rm server
