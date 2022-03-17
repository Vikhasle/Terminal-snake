CFLAGS = -Wall -Wextra

snake : snake.c
	gcc snake.c -o snake $(CFLAGS)
