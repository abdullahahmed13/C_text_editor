CC=gcc

editor: editor.c
	$(CC) editor.c -o editor -Wall -Wextra -pedantic -std=c2x
