#ifndef __TESTER_H
#define __TESTER_H

struct HttpRequest
{
  char method[5];
  char file[256];
};

int main();
void displayMenu();
void retrievalMenu();
void uploadMenu();
void submitRequest(struct HttpRequest *request);
void printDirectory(char *dir);
int awaitOption(int numOptions);
void awaitFilename(char *filename);
void startServer();
void stopServer();
void shutdown(int signal);

#endif
