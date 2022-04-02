/**
 * initrd init
 *
 * loads an included tarball as ramdisk and uses that as rootfs
 */

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include "mount.h"
#include "spawn.h"

char *cmd_extract[] = { "/tar", "-xf", "rootfs.tar", "-C", "/mnt", NULL };
char *cmd_init[]    = { "/sbin/init", NULL };

int main() {

  // Setup location to extract root
  mkdir("/mnt", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  mnt("tmpfs", "/mnt", "tmpfs", 0, NULL);

  // Extract rootfs
  int status, extractor = spawn(cmd_extract);
  waitpid(extractor, &status, 0);

  // Setup execution environment
  setenv("PATH"           , "/sbin:/usr/sbin:/usr/bin:/bin:/usr/local/bin", 1);
  setenv("LD_LIBRARY_PATH", "/lib64:/lib:/usr/lib64:/usr/lib"             , 1);

  // Switch root (no cleanup yet)
  // TODO: check https://git.suckless.org/ubase/file/switch_root.c.html for cleanup directions
  chdir("/mnt");
  mnt(".", "/", NULL, MS_MOVE, NULL);
  chroot(".");

  // Replace ourselves with new root's init
  execvp(cmd_init[0], cmd_init);
  perror("execvp -- ");
  printf("errno: %d\n", errno);
  _exit(1);
  return 1;
}
