all: server.c client.c IO.o server
	gcc -o server/WTFserver server.c IO.o -lssl -lcrypto -lpthread
	gcc -o WTF client.c IO.o -lssl -lcrypto

IO.o: IO.c
	gcc -c IO.c

server:
	mkdir server