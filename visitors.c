#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "visitors.h"

struct Visitors *visitorList = NULL;
pthread_mutex_t visitorLock; 

struct VisitorEntry *GetVisitor(char *userAgent)
{
  pthread_mutex_lock(&visitorLock);

  struct VisitorNode *i = visitorList->head;
  while (i)
  {
    if (!strcmp(i->data->userAgent, userAgent))
      break; // Match!
    i = i->next;
  }

  pthread_mutex_unlock(&visitorLock);
  if (i)
    return i->data;
  return NULL;
}

void PutVisitor(char *userAgent, int numVisits, int lastVisitTime)
{
  // Does entry already exist?
  struct VisitorEntry *i = GetVisitor(userAgent);
  if (i)
  {
    // Update entry
    i->numVisits = numVisits;
    i->lastVisitTime = lastVisitTime;

    // Return now
    return;
  }

  pthread_mutex_lock(&visitorLock);
  
  // Push a new entry to the stack
  // Create entry
  struct VisitorEntry *newEntry = malloc(sizeof(struct VisitorEntry));
  if (!newEntry)
  {
    puts("Failed to create new visitor entry");
    return;
  }

  strcpy(newEntry->userAgent, userAgent);
  newEntry->numVisits = numVisits;
  newEntry->lastVisitTime = lastVisitTime;

  // Create node
  struct VisitorNode *newNode = malloc(sizeof(struct VisitorNode));
  if (!newNode)
  {
    puts("Failed to create new visitor node");
    return;
  }

  newNode->data = newEntry;
  newNode->next = visitorList->head;
  visitorList->head = newNode;
  visitorList->numEntries++;

  pthread_mutex_unlock(&visitorLock);
}

void LoadVisitors()
{
  if (visitorList)
    return; // Already loaded

  pthread_mutex_init(&visitorLock, NULL);
  
  // Initialize empty visitor list
  visitorList = malloc(sizeof(struct Visitors));
  if (!visitorList)
  {
    puts("Failed to create initial visitor list");
    return;
  }

  visitorList->numEntries = 0;
  visitorList->head = NULL;

  // Open the visitors database
  int fd = open("visitors.csv", O_RDONLY);
  if (fd == -1)
  {
    // Was it because the file didn't exist?
    if (errno != ENOENT)
      perror("Couldn't open visitors.csv"); // No, it was for some other reason
    return; // Nonetheless, we have no data to read from
  }

  // Read
  char rawdb[10000];
  if (read(fd, rawdb, 10000) == -1)
  {
    perror("Could not read visitors.csv");
    return;
  }

  char *dbentry = strtok(rawdb, ",\n");
  while (dbentry)
  {
    // Parse this row
    char *userAgent = dbentry;
    dbentry = strtok(NULL, ",\n");
    int numVisits = atoi(dbentry);
    dbentry = strtok(NULL, ",\n");
    int lastVisitTime = atoi(dbentry);
    dbentry = strtok(NULL, ",\n");

    PutVisitor(userAgent, numVisits, lastVisitTime);
  }

  if (close(fd))
    perror("Couldn't close visitors.csv");
}

void GetCookie(char *userAgent, struct HttpResponse *response)
{
  struct VisitorEntry *visitor = GetVisitor(userAgent);
  int numVisits = 1;
  int lastVisitTime = (int)time(NULL);
  if (!visitor)
  {
    // New visitor!
    PutVisitor(userAgent, numVisits, lastVisitTime);
  }
  else
  {
    // Update existing entry
    numVisits = ++visitor->numVisits;
    visitor->lastVisitTime = lastVisitTime;
  }

  sprintf(response->cookie, "\nSet-Cookie: num_visits=%i\nSet-Cookie: last_visit_time=%i", numVisits, lastVisitTime);
}

void SaveVisitors(int signal)
{
  int fd = open("visitors.csv", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd == -1)
  {
    perror("Could not open visitors.csv");
    exit(signal);
  }

  struct VisitorNode *i = visitorList->head;
  while (i)
  {
    char line[1500];
    sprintf(line, "%s,%i,%i\n", i->data->userAgent, i->data->numVisits, i->data->lastVisitTime);
    if (write(fd, line, strlen(line)) == -1)
      perror("Could not write to visitors.csv");

    // Free resources
    struct VisitorNode *n = i;
    free(n->data);
    i = n->next;
    free(n);
  }
  free(visitorList);

  if (close(fd))
    perror("Could not close visitors.csv");
  exit(signal);
}
