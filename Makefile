CC=gcc
src=./src/

donkey: $(src)main.c
	$(CC) -o donkey $(src)main.c