all: server.c IO.o
	gcc server.c IO.o -lssl -lcrypto

IO.o: ../IO.c
	gcc -c ../IO.c

clean:
	rm a.out; rm IO.o;