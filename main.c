#include "common.h"
#include <netinet/in.h>
#include <pthread.h>
#include "server.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "visitors.h"

char *uriPrefix = "http://localhost";
char baseURI[100] = {0};

int main(int argc, char **argv)
{
  // Extract port number
  if (argc != 2)
  {
    printf("Usage: %s <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // Setup URI info with port
  int port = atoi(argv[1]);
  sprintf(baseURI, "%s:%i", uriPrefix, port);

  // Load visitors database
  LoadVisitors();

  // We want to save the database upon server shutdown
  signal(SIGINT, SaveVisitors);
  signal(SIGTERM, SaveVisitors);

  // Setup tcp socket
  int socketfd = socket(AF_INET, SOCK_STREAM, 0);
  if (socketfd == -1)
  {
    perror("Failed to initialize socket");
    exit(EXIT_FAILURE);
  }

  // Initialize server address struct
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  // Assign socket to address
  if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)))
  {
    perror("Failed to bind socket to address");
    exit(EXIT_FAILURE);
  }

  // Wait for incoming connections
  while (1)
  {
    if (listen(socketfd, 5) == -1)
    {
      perror("Failed to listen to socket");
      exit(EXIT_FAILURE);
    }

    // Obtain client info
    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);

    int *clientfd = malloc(sizeof(int)); // We don't want to lose the fd
    if (!clientfd)
    {
      puts("Failed to allocate clientfd");
      exit(EXIT_FAILURE);
    }

    *clientfd = accept(socketfd, (struct sockaddr *)&client_addr, &client_len);
    if (*clientfd == -1)
    {
      perror("Failed to accept client");
      exit(EXIT_FAILURE);
    }

    // Create new thread to handle client
    pthread_t thread;
    pthread_create(&thread, NULL, httpserver, (void *)clientfd);
  }

  return 0;
}
