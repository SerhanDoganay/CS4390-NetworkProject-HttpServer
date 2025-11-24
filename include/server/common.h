#ifndef __COMMON_H
#define __COMMON_H

extern char *uriPrefix;
extern char baseURI[100];
extern char uploadDir[256];
extern char buildDir[200];

struct ServerArg
{
  int clientfd;
  char ipaddr[16];
};

#endif

