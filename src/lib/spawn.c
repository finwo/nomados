#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "spawn.h"

int
spawn(char *argv[])
{
  int pid;
  switch (pid = fork()) {
    case 0:
      sigprocmask(SIG_UNBLOCK, &set, NULL);
      setsid();
      execvp(argv[0], argv);
      perror("execvp -- ");
      printf("errno: %d\n", errno);
      _exit(1);
    case -1:
      perror("fork");
  }
  return pid;
}
