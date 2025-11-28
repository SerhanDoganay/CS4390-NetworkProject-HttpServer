# Overview

This project implements an HTTP/1.0 web server, client and a tester program that guides the user through client and server interactions.

# System Requirements

Linux is preferred. Although this project can be built and run on Windows, thorough testing has not been conducted unlike in Linux.

If you’re testing the server using a standard web browser, **make sure caching is enabled.** 

Otherwise, the browser will send a request for favicon.ico immediately after requesting the resource you want, and **you might accidentally trigger the server’s anti-DoS feature.**

The server offers some grace initially, so the browser won’t be banned after the first request.

# Compilation

## Linux

Make sure gcc is installed.

Run “make”.

Binary files will be created in the build directory.

## Windows

Make sure Cygwin is installed with the following packages: cygwin-devel, gcc-core.

Make sure the \<cygwin directory\>/bin directory is added to the PATH environmental variable.

Within the Cygwin terminal, “cd” to this project’s root directory.

Run “make” (If Cygwin complains about forked processes, keep rerunning “make”).

Binary files will be created in the build directory.

# How to Run

After building, three executable files will be created in the build directory: “server”, “client” and “tester”.

**Server**: “./server \<port\>”

For example: “./server 5335”

**Client**: “./client \<hostname\> \<port\> \<file\> \<HTTP method\> \[-d \<num rapid requests\>\]

For example: “./client 127.0.0.1 5335 index.html GET \-d 100”

For example: “./client 127.0.0.1 5335 sample1 POST”

**Tester**: “./tester”

# Codebase Structure

## Server

* src/server/main.c \- Entry point. Responsible for listening for incoming connections.  
* src/server/server.c \- Reads an HTTP request and transmits an HTTP response. This is where the brains of the server reside.  
* src/server/visitors.c \- Keeps track of users interacting with the server, generates cookies, and handles DoS protection.

## Client

* src/client/main.c \- Entry point. Entire client code resides here. 

## Tester

* src/tester/main.c \- Entry point. Entire tester code resides here.

# Frameworks Used

UNIX syscalls, POSIX threads API, POSIX sockets API, and the C Standard Library.

# Implementation Details

## Server

On startup:

* Locate server files  
* Read visitor data from visitors.csv if it exists  
* Create TCP socket  
* Wait for incoming connections

On connection:

* Extract client’s User-Agent and IP address  
* Update/add visitor entry, logging number of visits and visit time  
* Keep a running average of the number of requests per second each visitor generates. If this surpasses an average of 100 requests per minute, then close the socket and stop here  
* Generate a cookie for the user  
* Scan the HTTP request for the method (HEAD, GET, POST, PUT), and whether it has file contents  
* If it’s a HEAD or GET request, infer the file type and fetch file data if it exists, or throw status code 404 if it doesn’t  
* If it’s a POST request, save the HTTP payload to a file if it doesn’t exist, or throw status code 403 if it does  
* If it’s a PUT request, rewrite the file only if the file exists, returning 404 otherwise, and if the file being modified was created by a client (indicated by different permission flags), returning 403 otherwise  
* Compose and transmit the HTTP response  
* Close the connection

On shutdown:

* Export in-memory visitors list to visitors.csv

## Client

* Locate Download directory  
* If the user wants a POST or PUT request, then read the corresponding file  
* Compose an HTTP request with a User-Agent field, as well as Content-Length and content fields if the request was POST or PUT  
* Connect to the server’s TCP socket  
* Transmit the HTTP request  
* Print the HTTP response to the terminal as it comes in  
* Scan the HTTP response for the status code, Content-Length, and two new lines which indicate the start of file contents  
* If the user made a GET request, then write the content to a file  
* Check whether the client received as many bytes of data as was promised in Content-Length  
* Close the connection and close the file  
* If the user specified a rapid request argument (flag “-d”), then decrement that counter and repeat starting from the “Connect to the server’s TCP socket” step until the counter reaches 0, skipping any file write operations this time

## Tester

On startup:

* Locate server and client files and executables  
* Startup the server

Main loop:

* Display options menu and await user input  
* IF RETRIEVING, display files in Upload directory  
* IF UPLOADING, display files in Download directory  
* Await target filename from user  
* Display DoS menu and await user input  
* Start the client with appropriate command line arguments  
* IF RESTARTING, kill the child server process and make a new one

On shutdown:

* Kill the child server process

