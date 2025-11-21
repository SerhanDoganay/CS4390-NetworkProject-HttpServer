CFLAGS = -Wall -Wextra -O2 -Iinclude

all: client server

client: build
	gcc $(CFLAGS) src/client/*.c -o build/client

server: build
	gcc $(CFLAGS) src/server/*.c -o build/server

build:
	mkdir -p build

clean:
	rm -rf build
