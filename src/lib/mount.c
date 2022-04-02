#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>

#include "mount.h"

void mnt(const char * src, const char *dst, const char * type, int rwflag, const char *data)
{
    if (!mount(src, dst, type, rwflag, data)) {
        /* printf("MOUNT: %s\n", dst); */
    } else {
        perror("MOUNT");
        printf("ERROR:   %s: %s\n", strerror(errno), dst);
    }
}

void umnt(const char * target)
{
    if (!umount(target)) {
        /* printf("UMOUNT: %s\n", target); */
    } else {
        perror("UMOUT");
        printf("ERROR:   %s: %s\n", strerror(errno), target);
    }
}
