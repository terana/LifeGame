CFLAGS=  -Wall
CC=mpicc

server: life_server.o
	$(CC) $(CFLAGS)  $^ -o $@

worker: worker.o
	$(CC) $(CFLAGS)   $^ -o $@

client: client.o
	$(CC) $(CFLAGS)  $^ -o $@

clean:
	rm -f *.o server client worker a.out server_port.txt