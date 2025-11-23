#ifndef __SERVER_H
#define __SERVER_H

#define STATUS_OK 200
#define STATUS_CREATED 201
#define STATUS_FORBIDDEN 403
#define STATUS_NOT_FOUND 404

struct HttpResponse
{
  int status;
  char hasLocation;
  char hasContent;
  char isBanned;
  char isHead;
  int contentLength;
  char location[100];
  char cookie[200];
  char contentType[100];
  char content[10000];
};

void *httpserver(void *);
void handleHttpRequest(char *request, struct HttpResponse *response);
void retrieveContent(char *targetFile, struct HttpResponse *response, char isHead);
void sendHttpResponse(int clientfd, struct HttpResponse *response);
void determineContentType(char *targetFile, struct HttpResponse *response);
void writeContent(int fd, struct HttpResponse *request);
void uploadContent(char *targetFile, char *inputData, int dataLength, struct HttpResponse *response, char create);

#endif
