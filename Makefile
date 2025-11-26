CC=gcc
src=./src/

donkeyd: $(src)donkeyd.c
	$(CC) -o donkeyd $(src)donkeyd.c

donkey: $(src)donkey.c
	$(CC) -o donkey $(src)donkey.c $(src)utils.c