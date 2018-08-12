CC=gcc

typefast: main.c typefast.h
	$(CC) -g -pthread  main.c -o typefast -lncurses

