#include <arpa/inet.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

char downloadDir[256];

int main(int argc, char **argv)
{
  if (argc < 5)
  {
    printf("Usage: %s <server host> <server port> <filename> <command> [options]\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  else if (argc == 6)
  {
    printf("Expected an argument with the option\n");
    exit(EXIT_FAILURE);
  }

  // Determine the Download directory
  char p[200];
  int len = readlink("/proc/self/exe", p, 199);
  if (len)
    sprintf(downloadDir, "%s/../Download", dirname(p));
  else
    exit(EXIT_FAILURE);

  // Extract command line arguments
  char *host = argv[1];
  int port = atoi(argv[2]);
  char *filename = argv[3];
  char *method = argv[4];
  char *option = NULL;
  int optionArg = 1;

  if (argc > 5)
  {
    option = argv[5];
    optionArg = atoi(argv[6]);

    if (strcmp(option, "-d"))
    {
      printf("Unexpected option \"%s\"\n", argv[5]);
      exit(EXIT_FAILURE);
    }
  }

  // Read the method
  char sendFile = 0;
  char isHead = 0;
  if (!strcmp(method, "GET"))
    sendFile = 0;
  else if (!strcmp(method, "POST") || !strcmp(method, "PUT"))
    sendFile = 1;
  else if (!strcmp(method, "HEAD"))
    isHead = 1;
  else
  {
    printf("Unknown method \"%s\"\n", method);
    exit(EXIT_FAILURE);
  }

  // Open file
  char filePath[300];
  char transmissionPath[40];
  if (filename[0] == '/')
    sprintf(transmissionPath, "%s", filename);
  else
    sprintf(transmissionPath, "/%s", filename);
  sprintf(filePath, "%s%s", downloadDir, transmissionPath);

  int fd = -1;
  if (sendFile)
    fd = open(filePath, O_RDONLY); // Read the file contents so I can send this over the HTTP request
  // ...If this is a GET request, wait until after we determine whether the file actually exists in the server
  
  if (sendFile && fd == -1)
  {
    perror("Could not open file");
    exit(EXIT_FAILURE);
  }

  // Read local file contents (for POST or PUT)
  char fileContents[5000];
  int fileLength = -1;
  if (sendFile && (fileLength = read(fd, fileContents, 5000)) == -1)
  {
    perror("Could not read file");
    exit(EXIT_FAILURE);
  }

  // Compose HTTP request
  // Necessary components: Header, User-Agent, (if sending: Content-Length, content)
  char http1[300];
  sprintf(http1, "%s %s HTTP/1.0\nUser-Agent: client4390\n", method, transmissionPath);

  char http2[5100] = {0}; // If sending a POST or PUT
  if (sendFile)
    sprintf(http2, "Content-Length: %i\n\n%s", fileLength, fileContents);

  char httpRequest[6000];
  sprintf(httpRequest, "%s%s", http1, http2);

  // Transmission loop
  char hasWritten = isHead;
  char serverResponse[1000] = {0};
  while (optionArg--)
  {
    // Establish connection
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == -1)
    {
      perror("Failed to initialize socket");
      exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) < 1)
    {
      perror("Host argument was not in a proper format");
      exit(EXIT_FAILURE);
    }

    if (connect(socketfd, (struct sockaddr *)&addr, sizeof(addr)))
    {
      perror("Could not connect to host");
      exit(EXIT_FAILURE);
    }
    
    // Connection is established!
    // Transmit request
    if (send(socketfd, httpRequest, strlen(httpRequest), 0) == -1)
      perror("Could not transmit HTTP request");

    // Listen for response
    memset(serverResponse, 0, 1000);
    int recvStatus = 0;
    char anticipateFile = 0;
    int promisedLength = -1;
    int bytesRead = 0;
    int httpStatus = -1;
    while ((recvStatus = recv(socketfd, serverResponse, 1000, 0)) > 0)
    {
      // Print server response
      printf("%s", serverResponse);

      // What's the HTTP status?
      if (httpStatus == -1)
      {
        char *statusField = serverResponse + 9; // Skip "HTTP/1.0 "
        httpStatus = atoi(statusField);
        if (httpStatus < 200)
          printf("[CLIENT] Invalid status code received: %i\n", httpStatus);

        // GET requests will return 200 if successful
        if (httpStatus != 200)
          sendFile = 1; // If a different status code was obtained, don't save this file
      }

      // Am I getting Content-Length?
      if (!sendFile && promisedLength == -1)
      {
        char *lengthField = strstr(serverResponse, "Content-Length: ");
        if (lengthField)
        {
          // Extract file length
          char splicedString[1000];
          strcpy(splicedString, lengthField);
          strtok(splicedString, " ");
          char *l = strtok(NULL, "\r\n");
          promisedLength = atoi(l);
        }
      }

      // Am I about to receive data?
      char *fileFragment = strstr(serverResponse, "\r\n\r\n");
      char isFragmented = fileFragment && !anticipateFile;
      if (!sendFile && isFragmented)
      {
        anticipateFile = 1;
        if (!hasWritten)
        {
          fd = open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0644); // Prepare to receive a file and write to it
          if (fd == -1)
            perror("Could not open file for writing");
        }
      }

      if (anticipateFile && !hasWritten)
      {
        // Download file
        char *fileSource = serverResponse;
        if (isFragmented)  // If serverResponse contains HTTP headers, don't count them to bytesOfData
          fileSource = fileFragment + 4; // Skip past \r\n\r\n
        int bytesToRead = (promisedLength > recvStatus) ? (recvStatus - (fileSource - serverResponse)) : promisedLength;

        int bytesOfData = 0;
        if ((bytesOfData = write(fd, fileSource, bytesToRead)) == -1)
          perror("Couldn't write contents to file");

        bytesRead += bytesOfData;
      }

      // Prepare for next loop
      memset(serverResponse, 0, 1000);
    }

    if (httpStatus == -1)
      puts("[CLIENT] Server terminated the connection");
    else if (!sendFile && !isHead && bytesRead != promisedLength && !hasWritten)
      printf("[CLIENT] Was promised %i bytes; received %i bytes instead\n", promisedLength, bytesRead);

    if (recvStatus == -1)
      perror("Could not receive server response");

    // Close the connection
    close(socketfd);
    if (fd >= 0)
      close(fd);
    hasWritten = 1; // So that we don't continuously open and close files
  }

  return 0;
}
