all: server.c IO.o
	gcc server.c IO.o -lssl -lcrypto -lpthread

IO.o: ../IO.c
	gcc -c ../IO.c

clean:
	rm a.out; rm IO.o;