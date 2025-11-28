#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "tester/tester.h"
#include <unistd.h>

char buildDir[200];
char downloadDir[256];
char uploadDir[256];
char clientPath[256];
char serverPath[256];
int serverPID = -1;
int serverPort = -1;
int serverOut = -1;

int main()
{
  signal(SIGINT, shutdown);
  signal(SIGTERM, shutdown);

  // Find Download and Upload directories
  char p[200];
  int len = readlink("/proc/self/exe", p, 199);
  if (len)
  {
    strcpy(buildDir, dirname(p));
    sprintf(downloadDir, "%s/../Download", buildDir);
    sprintf(uploadDir, "%s/../Upload", buildDir);
    sprintf(clientPath, "%s/client", buildDir);
    sprintf(serverPath, "%s/server", buildDir);
  }
  else
    exit(EXIT_FAILURE);

  // Start
  startServer();
  while (1)
    displayMenu();
  
  return 0;
}

void displayMenu()
{
  printf("=== [HTTP CLIENT/SERVER TESTER] ===\nSelect an option:\n1) Retrieve a file from the server\n2) Upload a file to the server\n3) Restart server\n4) Quit\n");
  int opt = awaitOption(4);

  switch (opt)
  {
    case 1: // RETRIEVE
      retrievalMenu();
      break;
    case 2: // UPLOAD
      uploadMenu();
      break;
    case 3: // RESTART SERVER
      printf("Restarting server...\n");
      stopServer();
      startServer();
      break;
    case 4: // QUIT
      shutdown(SIGTERM);
      break;
  }
}

void retrievalMenu()
{
  printf("=== [RETRIEVAL MENU] ===\nList of server files:\n");
  printDirectory(uploadDir);
 
  struct HttpRequest request = {0};
 
  awaitFilename(request.file);

  printf("=== [RETRIEVAL MENU] ===\nSelect an option:\n1) Download this file [GET]\n2) Fetch metadata [HEAD]\n");
  int opt = awaitOption(2);

  if (opt == 1)
    strcpy(request.method, "GET");
  else
    strcpy(request.method, "HEAD");

  submitRequest(&request);
}

void uploadMenu()
{
  printf("=== [UPLOAD MENU] ===\nList of client files:\n");
  printDirectory(downloadDir); // We're UPLOADING from the DOWNLOAD directory

  struct HttpRequest request = {0};

  awaitFilename(request.file);

  printf("=== [UPLOAD MENU] ===\nSelect an option:\n1) Create this file in the server [POST]\n2) Replace this file in the server [PUT]\n");
  int opt = awaitOption(2);

  if (opt == 1)
    strcpy(request.method, "POST");
  else
    strcpy(request.method, "PUT");

  submitRequest(&request);
}

void submitRequest(struct HttpRequest *request)
{
  // DoS menu
  printf("=== [DOS MENU] ===\nDo you want to send rapid requests?\n1) Yes (test DoS protection)\n2) No\n");
  int opt = awaitOption(2);

  int dos = 0;
  if (opt == 1)
  {
    printf("=== [DOS MENU] ===\nHow many rapid requests do you want to send?\n");
    dos = awaitOption(10000);
  }

  // Submit the request
  printf("***** STARTING CLIENT *****\n");
  int clientpid = -1;
  if ((clientpid = fork()) > 0)
  {
    // Parent
    int stat;
    waitpid(clientpid, &stat, 0);
  }
  else
  {
    char *argv[] = {clientPath, "127.0.0.1", NULL, request->file, request->method, NULL, NULL, NULL};
    char portStr[6];
    sprintf(portStr, "%i", serverPort);
    argv[2] = portStr;

    if (dos)
    {
      char *dFlag = "-d";
      char dCount[6];
      sprintf(dCount, "%i", dos);

      argv[5] = dFlag;
      argv[6] = dCount;
    }

    if (execvp(argv[0], argv))
      perror("Failed to exec client");
  }
}

void awaitFilename(char *filename)
{
  while (1)
  {
    printf("Input a file name: ");
    if (!scanf("%s", filename) || strchr(filename, '/') || strchr(filename, '\\'))
      printf("Special character detected. Try again\n");
    else
      return;
  }
}

void printDirectory(char *dir)
{
  int lspid = -1;
  if ((lspid = fork()) > 0)
  {
    // Parent
    int stat;
    waitpid(lspid, &stat, 0);
  }
  else
  {
    char *argv[] = {"ls", "-l", dir, NULL};
    if (execvp(argv[0], argv))
      perror("Failed to exec ls");
  }
}

int awaitOption(int numOptions)
{
  int ret = 0;
  while (1)
  {
    printf("Input your response: ");
    if (!scanf("%d", &ret) || ret < 1 || ret > numOptions)
      printf("Invalid input\n");
    else
      return ret;
  }
}

void startServer()
{
  // Check if the server is running
  if (serverPID != -1)
    return;

  // Keep starting the server until we find a port number that works
  serverPort = 4390;
  char validPort = 0;
  char *argv[3] = {serverPath, NULL, NULL};
  int pipefd[2];
  while (!validPort)
  {
    if (pipe(pipefd))
    {
      perror("Could not create a pipe");
      return;
    }

    char portStr[5];
    sprintf(portStr, "%i", serverPort);
    argv[1] = portStr;

    if ((serverPID = fork()) > 0)
    {
      // Parent
      if (close(pipefd[1])) // Parent will only read
        perror("Could not close write end of pipe");

      if (usleep(10000)) // 1 ms should be enough time to catch an error
        perror("Could not set timer");

      // Make it so that reading from pipefd[0] doesn't block
      int flags = fcntl(pipefd[0], F_GETFL, 0);
      fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

      char buf[20] = {0};
      int bytesRead = 0;
      bytesRead = read(pipefd[0], buf, 20);
      serverOut = pipefd[0];

      if (bytesRead > 0)
      {
        // Uh oh, something went wrong...
        stopServer();
        serverPort++; // Try again with a new port number
      }
      else
        validPort = 1; // The server is up and running!
    }
    else
    {
      // Child
      if (close(pipefd[0])) // Child will only write
        perror("Could not close read end of pipe");
      
      // Redirect stderr to the pipe
      dup2(pipefd[1], 2);
      close(pipefd[1]);
      
      // Start the server
      if (execvp(argv[0], argv))
        perror("Failed to exec server");
    }
  }

  printf("Started server on port %i\n", serverPort);
}

void stopServer()
{
  // Check if the server is running
  if (serverPID == -1)
    return;

  // Kill the server
  if (kill(serverPID, SIGTERM))
    perror("Failed to kill server");

  // Close the receiving end of the server's stderr pipe
  close(serverOut);
  serverPID = -1;
  serverOut = -1;
}

void shutdown(int signal)
{
  stopServer();
  exit(signal);
}
