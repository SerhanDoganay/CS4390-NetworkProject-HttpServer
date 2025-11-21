#include "common.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "visitors.h"

void *httpserver(void *threadArg)
{
  int clientfd = *(int *)threadArg;

  // Listen for HTTP requests
  char request[10000];
  struct HttpResponse response;
  while (read(clientfd, request, 10000) > 0)
  {
    memset(&response, 0, sizeof(response));
    handleHttpRequest(request, &response);
    sendHttpResponse(clientfd, &response);
  }

  pthread_exit(NULL);
}

void handleHttpRequest(char *request, struct HttpResponse *response)
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
  if (!GetCookie(userAgent, response))
  {
    response->status = STATUS_FORBIDDEN; // Don't handle requests from users that are trying to DoS us
    writeContent(-1, response);
    return;
  }

  // If request contains post/put data, check content length
  char *lengthField = strstr(request, "Content-Length: ");
  int dataLength = 0;
  char *inputFile;
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
    handleGetRequest(targetFile, response);
  else if (!strcmp(method, "HEAD"))
    handleHeadRequest(targetFile, response);
  else if (!strcmp(method, "POST"))
    uploadContent(targetFile, inputFile, dataLength, response, 1);
  else if (!strcmp(method, "PUT"))
    uploadContent(targetFile, inputFile, dataLength, response, 0);
  else  
    printf("Skipping unknown HTTP method: %s\n", method);
}

void handleGetRequest(char *targetFile, struct HttpResponse *response)
{
  int fd = handleHeadRequest(targetFile, response);
  writeContent(fd, response);
}

int handleHeadRequest(char *targetFile, struct HttpResponse *response)
{
  if (!strcmp(targetFile, "/"))
    targetFile = "/index.html"; // If no file explicity requested, return index.html

  determineContentType(targetFile, response);

  char path[256];
  sprintf(path, "Upload%s", targetFile);
  int fd = open(path, O_RDONLY);
  
  response->status = STATUS_OK; // Assume everything's OK

  // Can we open the file?
  if (fd == -1)
  {
    // Why not? (TEST)
    response->status = STATUS_NOT_FOUND;
    perror("Failed to open file");
  }
  
  return fd;
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
  if (write(clientfd, msg, strlen(msg)) == -1)
    perror("Failed to send HTTP response");
  
  if (response->cookie)
  {
    if (write(clientfd, response->cookie, strlen(response->cookie)) == -1)
      perror("Failed to send HTTP response");
  }

  if (response->hasLocation)
  {
    sprintf(msg, "\nLocation: %s", response->location);
    // Send response fragment
    if (write(clientfd, msg, strlen(msg)) == -1)
      perror("Failed to send HTTP response");
  }

  if (response->hasContent)
  {
    sprintf(msg, "\nContent-Type: %s\nContent-Length: %i\n\n", response->contentType, response->contentLength);
    // Send response fragment
    if (write(clientfd, msg, strlen(msg)) == -1)
      perror("Failed to send HTTP response");

    // Send file contents
    if (write(clientfd, response->content, response->contentLength) == -1)
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
    char errorhtml[30];
    sprintf(errorhtml, "Upload/%i.html", response->status);
    fd = open(errorhtml, O_RDONLY);

    determineContentType(errorhtml, response);

    if (fd == -1)
      perror("Failed to open error html");
  }

  response->contentLength = read(fd, response->content, 10000);
}

void uploadContent(char *targetFile, char *inputData, int dataLength, struct HttpResponse *response, char create)
{
  char path[256];
  sprintf(path, "Upload%s", targetFile);

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
  write(fd, inputData, dataLength);
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
