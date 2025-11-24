#include <arpa/inet.h>
#include <libgen.h>
#include <netinet/in.h>
#include <pthread.h>
#include "server/common.h"
#include "server/server.h"
#include "server/visitors.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

char *uriPrefix = "http://localhost";
char baseURI[100] = {0};
char uploadDir[256] = {0};
char buildDir[200] = {0};

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

  // Determine the Upload directory
  char p[200];
  int len = readlink("/proc/self/exe", p, 199);
  if (len)
  {
    strcpy(buildDir, dirname(p));
    sprintf(uploadDir, "%s/../Upload", buildDir);
  }
  else
    exit(EXIT_FAILURE);

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
    socklen_t client_len = sizeof(client_addr);

    struct ServerArg *clientInfo = malloc(sizeof(struct ServerArg)); // We don't want to lose the fd
    if (!clientInfo)
    {
      puts("Failed to allocate clientInfo");
      exit(EXIT_FAILURE);
    }

    clientInfo->clientfd = accept(socketfd, (struct sockaddr *)&client_addr, &client_len);
    if (clientInfo->clientfd == -1)
    {
      perror("Failed to accept client");
      exit(EXIT_FAILURE);
    }

    // Get IP address of client
    inet_ntop(AF_INET, &(client_addr.sin_addr), clientInfo->ipaddr, 16);

    // Create new thread to handle client
    pthread_t thread;
    pthread_create(&thread, NULL, httpserver, (void *)clientInfo);
  }

  return 0;
}
