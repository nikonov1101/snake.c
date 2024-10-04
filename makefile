.PHONY: snake 
snake:
	$(CC) -g3 -Wall -Wextra -Wconversion -Wdouble-promotion -Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion -fsanitize=undefined snake.c screenbuf.c -o ./snake

