all: cshell

cshell: cshell.c
	gcc -o cshell cshell.c

clean:
	rm -f cshell *.o