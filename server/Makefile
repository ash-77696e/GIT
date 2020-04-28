all: server.c IO.o
	gcc server.c IO.o

IO.o: ../IO.c
	gcc -c ../IO.c

clean:
	rm a.out; rm IO.o;