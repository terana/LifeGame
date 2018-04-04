#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

void CrashIfError(int status, const char * description) {
  if (status != 0) {
    printf("Error %s, status: %d, errno: %d\n", description, status, errno);
    exit(status);
  }
}

void Assert(int statement, const char * description) {
	CrashIfError(!statement, description);
}