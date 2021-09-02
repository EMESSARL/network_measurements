CFLAGS=-Wall -O2 -Werror -Wall -Wextra -Wpedantic -Wformat=2 -Wformat-overflow=2 -Wformat-truncation=2 -Wformat-security -Wnull-dereference -Wstack-protector -Wtrampolines -Walloca -Wvla -Warray-bounds=2 -Wimplicit-fallthrough=3 -Wtraditional-conversion -Wshift-overflow=2 -Wcast-qual -Wstringop-overflow=4 -Wconversion -Warith-conversion -Wlogical-op -Wduplicated-cond -Wduplicated-branches -Wformat-signedness -Wshadow -Wstrict-overflow=4 -Wundef -Wstrict-prototypes -Wswitch-default -Wswitch-enum -Wstack-usage=1000000 -Wcast-align=strict -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fstack-clash-protection -fPIE -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -Wl,-z,separate-code

all: utils.o client.o server.o server client

utils.o: utils.c
	gcc -c  $(CFLAGS) utils.c -o utils.o

message.o: message.c
	gcc -c  $(CFLAGS) message.c -o message.o

client.o: client.c
	gcc -c  $(CFLAGS) client.c -o client.o

server.o: server.c
	gcc -c  $(CFLAGS) server.c -o server.o

server: utils.o message.o server.o
	gcc -g utils.o message.o server.o -o server 

client: utils.o client.o
	gcc -g utils.o message.o client.o -o client -lreadline
	

clean:
	rm *.o
	rm client
	rm server
