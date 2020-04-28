all: client.c IO.o
	gcc client.c IO.o -lssl -lcrypto

IO.o: ../IO.c
	gcc -c ../IO.c

clean:
	rm a.out; rm IO.o;