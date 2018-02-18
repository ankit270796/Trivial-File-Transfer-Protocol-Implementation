CC=g++

all: tftps

tftps: server.o tftp.o
	$(CC) server.o tftp.o -o tftps

server.o: server.cpp
	$(CC) -c server.cpp

tftp.o: tftp.cpp
	$(CC) -c tftp.cpp

.PHONY: clean

clean:
	rm -f tftps *.o