CC=gcc
src=./src/

all:
	$(CC) -o donkeyd $(src)donkeyd.c
	$(CC) -o donkey $(src)donkey.c $(src)utils.c
	
donkeyd: $(src)donkeyd.c
	$(CC) -o donkeyd $(src)donkeyd.c

donkey: $(src)donkey.c
	$(CC) -o donkey $(src)donkey.c $(src)utils.c