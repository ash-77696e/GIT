all: server.c client.c IO.o server client
	gcc -o server/WTFserver server.c IO.o -lssl -lcrypto -lpthread
	gcc -o client/WTF client.c IO.o -lssl -lcrypto

test: server.c client.c IO.o server client
	gcc -o server/WTFserver server.c IO.o -lssl -lcrypto -lpthread
	gcc -o client/WTF client.c IO.o -lssl -lcrypto
	gcc -o WTFtest WTFtest.c

IO.o: IO.c
	gcc -c IO.c

server:
	mkdir server

client:
	mkdir client

clean:
	rm -r client
	rm -r server
	rm IO.o
	rm WTFtest