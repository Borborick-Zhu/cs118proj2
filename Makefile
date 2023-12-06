CC=gcc
CFLAGS=-Wall -Wextra -Werror

all: clean build

default: build

build: server.c client.cpp
	gcc -Wall -Wextra -o server server.c
	gcc -Wall -Wextra -o client client.cpp

clean:
	rm -f server client output.txt project2.zip

zip: 
	zip project2.zip server.c client.cpp utils.h Makefile README.md report.txt
