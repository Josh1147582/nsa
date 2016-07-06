nsa : nsa.c
	gcc -std=c99 -Wall -ggdb -Wextra -pedantic -o nsa nsa.c

clean:
	rm -f nsa
