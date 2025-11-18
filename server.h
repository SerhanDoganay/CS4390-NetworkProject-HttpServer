#ifndef __SERVER_H
#define __SERVER_H

#define HTTP_GET 0
#define HTTP_HEAD 1
#define HTTP_POST 2
#define HTTP_PUT 3

#define STATUS_OK 200
#define STATUS_CREATED 201
#define STATUS_FORBIDDEN 403
#define STATUS_NOT_FOUND 404

struct HttpResponse
{
  int method;
  int status;
  char hasLocation;
  char hasContent;
  int contentLength;
  char location[100];
  char cookie[200];
  char contentType[100];
  char content[10000];
};

void *httpserver(void *);
void handleHttpRequest(char *request, struct HttpResponse *response);
void handleGetRequest(char *targetFile, struct HttpResponse *response);
int handleHeadRequest(char *targetFile, struct HttpResponse *response);
void handlePostRequest(char *targetFile, char *inputData, int dataLength, struct HttpResponse *response);
void handlePutRequest(char *targetFile, char *inputData, int dataLength, struct HttpResponse *response);
void sendHttpResponse(int clientfd, struct HttpResponse *response);
void determineContentType(char *targetFile, struct HttpResponse *response);

#endif
