#ifndef __VISITORS_H
#define __VISITORS_H

#include "server.h"

struct VisitorEntry
{
  char userAgent[1000];
  char ipaddr[16];
  int numVisits;
  int lastVisitTime;

  // Runtime
  float requestsPerSec;
  int numVisitsThisSession;
  char isBlocked;
};

struct VisitorNode
{
  struct VisitorEntry *data;
  struct VisitorNode *next;
};

struct HttpResponse;

struct VisitorEntry *GetVisitor(char *userAgent, char *ipaddr);
void PutVisitor(char *userAgent, char *ipaddr, int numVisits, int lastVisitTime);
void LoadVisitors();
int GetCookie(char *userAgent, char *ipaddr, struct HttpResponse *response);
void SaveVisitors(int signal);

#endif
