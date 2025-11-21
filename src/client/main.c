#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char downloadDir[256];

int main(int argc, char **argv)
{
  if (argc < 5)
  {
    printf("Usage: %s <server host> <server port> <filename> <command> [options]\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // Determine the Download directory
  char p[200];
  int len = readlink("/proc/self/exe", p, 199);
  if (len)
    sprintf(downloadDir, "%s/../Download", dirname(p));
  else
    exit(EXIT_FAILURE);
  return 0;
}
