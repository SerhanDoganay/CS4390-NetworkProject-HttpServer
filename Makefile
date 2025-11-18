all: main

main: main.c server.c visitors.c
	gcc *.c -o main

clean:
	rm main
