#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include "server/common.h"
#include "server/server.h"
#include "server/visitors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

void *httpserver(void *threadArg)
{
  struct ServerArg *clientInfo = (struct ServerArg *)threadArg;
  int clientfd = clientInfo->clientfd;

  // Listen for an HTTP request
  char request[10000] = {0};
  struct HttpResponse response = {0};
  if (recv(clientfd, request, 10000, 0) > 0)
  {
    handleHttpRequest(request, clientInfo->ipaddr, &response);
    if (!response.isBanned)
      sendHttpResponse(clientfd, &response);
  }

  close(clientfd);
  pthread_exit(NULL);
}

void handleHttpRequest(char *request, char *ipaddr, struct HttpResponse *response)
{
  // Get user agent
  char *uaField = strstr(request, "User-Agent: ");
  if (!uaField)
  {
    response->status = STATUS_FORBIDDEN; // Don't handle requests without user agents
    writeContent(-1, response);
    return;
  }
  char uaStr[10000];
  strcpy(uaStr, uaField); // strtok modifies input, so make a copy
  strtok(uaStr, " ");
  char *userAgent = strtok(NULL, "\r\n");

  // Generate cookie
  if (!GetCookie(userAgent, ipaddr, response))
  {
    response->isBanned = 1; // Don't handle requests from users that are trying to DoS us
    return;
  }

  // If request contains post/put data, check content length
  char *lengthField = strstr(request, "Content-Length: ");
  int dataLength = 0;
  char *inputFile = NULL;
  if (lengthField)
  {
    // Copy string because strtok actually modifies the input string
    char splicedString[10000];
    strcpy(splicedString, lengthField);

    strtok(splicedString, " ");
    char *length = strtok(NULL, " \r\n");
    dataLength = atoi(length);
    
    inputFile = (strlen(request) - dataLength) + request;
  }

  char *method = strtok(request, " "); // First word in request is method
  char *targetFile = strtok(NULL, " "); // Second word is target file

  // What's the request?
  if (!strcmp(method, "GET"))
    retrieveContent(targetFile, response, 0);
  else if (!strcmp(method, "HEAD"))
    retrieveContent(targetFile, response, 1);
  else if (inputFile && !strcmp(method, "POST"))
    uploadContent(targetFile, inputFile, dataLength, response, 1);
  else if (inputFile && !strcmp(method, "PUT"))
    uploadContent(targetFile, inputFile, dataLength, response, 0);
  else  
    printf("Skipping unknown HTTP method: %s\n", method);
}

void retrieveContent(char *targetFile, struct HttpResponse *response, char isHead)
{
  response->isHead = isHead;

  if (!strcmp(targetFile, "/"))
    targetFile = "/index.html"; // If no file explicity requested, return index.html

  determineContentType(targetFile, response);

  char path[300];
  sprintf(path, "%s%s", uploadDir, targetFile);
  int fd = open(path, O_RDONLY);
  
  response->status = STATUS_OK; // Assume everything's OK

  // Can we open the file?
  if (fd == -1)
  {
    response->status = STATUS_NOT_FOUND;
    perror("Failed to open file");
  }
  
  writeContent(fd, response);
}

void sendHttpResponse(int clientfd, struct HttpResponse *response)
{
  char msg[10000];
  char statusStr[20] = "OK";
  switch (response->status)
  {
    case STATUS_OK:
      break; // OK by default
    case STATUS_CREATED:
      strcpy(statusStr, "Created");
      break;
    case STATUS_NOT_FOUND:
      strcpy(statusStr, "Not Found");
      break;
    case STATUS_FORBIDDEN:
      strcpy(statusStr, "Forbidden");
      break;
    default:
      printf("Unkown status code %i. Witholding response\n", response->status);
  }
  sprintf(msg, "HTTP/1.0 %i %s", response->status, statusStr);
  
  // Send first part of response
  if (send(clientfd, msg, strlen(msg), 0) == -1)
    perror("Failed to send HTTP response");
  
  // Send cookie
  if (send(clientfd, response->cookie, strlen(response->cookie), 0) == -1)
    perror("Failed to send HTTP response");

  if (response->hasLocation)
  {
    sprintf(msg, "\r\nLocation: %s", response->location);
    // Send response fragment
    if (send(clientfd, msg, strlen(msg), 0) == -1)
      perror("Failed to send HTTP response");
  }

  if (response->hasContent)
  {
    sprintf(msg, "\r\nContent-Type: %s\r\nContent-Length: %i\r\n\r\n", response->contentType, response->contentLength);
    // Send response fragment
    if (send(clientfd, msg, strlen(msg), 0) == -1)
      perror("Failed to send HTTP response");

    // Send file contents
    if (response->isHead)
      return; // ...Except if this is a HEAD request
    if (send(clientfd, response->content, response->contentLength, 0) == -1)
      perror("Failed to send HTTP response");
  }
}

void determineContentType(char *targetFile, struct HttpResponse *response)
{
  // Determine content type by file extension
  char tCopy[256];
  strcpy(tCopy, targetFile);
  char *ext = strtok(tCopy, ".");
  ext = strtok(NULL, ".");
  if (ext && !strcmp(ext, "ico"))
    strcpy(response->contentType, "image/png\nCache-Control: max-age=172800"); // (Because browsers query favico immediately after getting the main page, don't trigger the DoS sensors)
  else // Assume it's html by default
    strcpy(response->contentType, "text/html");
}

void writeContent(int fd, struct HttpResponse *response)
{
  response->hasContent = 1;

  if (fd == -1 && (response->status == STATUS_NOT_FOUND || response->status == STATUS_FORBIDDEN))
  {
    char errorhtml[300];
    sprintf(errorhtml, "%s/%i.html", uploadDir, response->status);
    fd = open(errorhtml, O_RDONLY);

    determineContentType(errorhtml, response);

    if (fd == -1)
      perror("Failed to open error html");
  }

  response->contentLength = read(fd, response->content, 10000);
}

void uploadContent(char *targetFile, char *inputData, int dataLength, struct HttpResponse *response, char create)
{
  char path[300];
  sprintf(path, "%s%s", uploadDir, targetFile);

  if (create && !access(path, F_OK))
  {
    // [POST] File already exists! Discard request
    response->status = STATUS_FORBIDDEN;
    writeContent(-1, response);
    return;
  }
  else if (!create && access(path, F_OK))
  {
    // [PUT] No such file exists! Discard request
    response->status = STATUS_NOT_FOUND;
    writeContent(-1, response);
    return;
  }

  // If we're modifying a file, make sure it's a user-made file
  if (!create)
  {
    struct stat fs;
    if (stat(path, &fs))
    {
      perror("Could not stat file");
      return;
    }

    if ((fs.st_mode & 0777) != 0666)
    {
      // [PUT] Please don't modify this file! Discard request
      response->status = STATUS_FORBIDDEN;
      writeContent(-1, response);
      return;
    }
  }

  // Create file (we identify user-made files by their permissions)
  int oldmask = umask(0000);
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  umask(oldmask);
  if (fd == -1)
  {
    perror("Could not create file");
    return;
  }

  // Write the data
  if (write(fd, inputData, dataLength) == -1)
    perror("Could not write file contents");
  close(fd);

  // Update response
  response->status = create ? STATUS_CREATED : STATUS_OK;
  response->hasLocation = 1;
  sprintf(response->location, "%s%s", baseURI, targetFile);
  response->hasContent = 1;
  memcpy(response->content, inputData, dataLength);
  response->contentLength = dataLength;
  determineContentType(targetFile, response);
}
