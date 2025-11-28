CFLAGS = -Wall -Wextra -O2 -Iinclude

all: client server tester

client: build
	gcc $(CFLAGS) src/client/*.c -o build/client

server: build
	gcc $(CFLAGS) src/server/*.c -o build/server

tester: build
	gcc $(CFLAGS) src/tester/*.c -o build/tester

build:
	mkdir -p build

clean:
	rm -rf build
