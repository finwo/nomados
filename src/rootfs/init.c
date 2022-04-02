/* #include <arpa/inet.h> */
#include <ifaddrs.h>
#include <linux/if_link.h>
#include <netdb.h>
#include <net/if.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include "mount.h"
#include "spawn.h"

#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#define LEN(x)     (sizeof (x) / sizeof *(x))
#define TIMEO      30

char *cmd_test[]  = { "df", "-h", NULL };
char *cmd_dhcp[]  = { "sdhcp", NULL, NULL };
char *cmd_nomad[] = { "/sbin/nomad", "agent", "-dev", "-log-level=TRACE", "-config=/etc/nomad/init.json", NULL };

int ifup(char *);
static void sigpoweroff(void);
static void sigreap(void);
static void sigreboot(void);
char * find_iface();

/*
 * Magic values required to use _reboot() system call.
 *
 * Commands accepted by the _reboot() system call.
 *
 * RESTART     Restart system using default command and mode.
 * HALT        Stop OS and give system control to ROM monitor, if any.
 * CAD_ON      Ctrl-Alt-Del sequence causes RESTART command.
 * CAD_OFF     Ctrl-Alt-Del sequence sends SIGINT to init task.
 * POWER_OFF   Stop OS and remove all power from system, if possible.
 * RESTART2    Restart system using given command string.
 * SW_SUSPEND  Suspend system using software suspend if compiled in.
 * KEXEC       Restart system using a previously loaded Linux kernel
 */

#define  LINUX_REBOOT_MAGIC1          0xfee1dead
#define  LINUX_REBOOT_MAGIC2          672274793
#define  LINUX_REBOOT_MAGIC2A         85072278
#define  LINUX_REBOOT_MAGIC2B         369367448
#define  LINUX_REBOOT_MAGIC2C         537993216
#define  LINUX_REBOOT_CMD_RESTART     0x01234567
#define  LINUX_REBOOT_CMD_HALT        0xCDEF0123
#define  LINUX_REBOOT_CMD_CAD_ON      0x89ABCDEF
#define  LINUX_REBOOT_CMD_CAD_OFF     0x00000000
#define  LINUX_REBOOT_CMD_POWER_OFF   0x4321FEDC
#define  LINUX_REBOOT_CMD_RESTART2    0xA1B2C3D4
#define  LINUX_REBOOT_CMD_SW_SUSPEND  0xD000FCE2
#define  LINUX_REBOOT_CMD_KEXEC       0x45584543

static struct {
  int sig;
  void (*handler)(void);
} sigmap[] = {
  { SIGUSR1, sigpoweroff },
  { SIGCHLD, sigreap     },
  { SIGALRM, sigreap     },
  { SIGINT,  sigreboot   },
};

int main() {
  int i, sig;

  // Ensure required directories exist
  mkdir("/dev" , S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  mkdir("/proc", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  mkdir("/sys" , S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  // Mount OS essentials
  mnt("devtmpfs", "/dev"          , "devtmpfs", 0, "");
  mnt("proc"    , "/proc"         , "proc"    , 0, "");
  mnt("sysfs"   , "/sys"          , "sysfs"   , 0, "");
  mnt("cgroup"  , "/sys/fs/cgroup", "cgroup2" , 0, "");

  // Required for nomad
  mkdir("/var"               , S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  mkdir("/var/lib"           , S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  mkdir("/var/lib/nomad"     , S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  mkdir("/var/lib/nomad/data", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  // Detect main network interface
  char *iface = find_iface();
  cmd_dhcp[1] = iface;
  if (!iface) {
    printf("No iface found\n");
    exit(EXIT_FAILURE);
  }

  // Enable interfaces
  // Allow hardware to boot
  ifup("lo");
  ifup(iface);
  sleep(5);

  // Launch dhcp client
  // Give it some time to fetch the address
  sethostname("nomados", 7);
  spawn(cmd_dhcp);
  sleep(5);

  // Start nomad
  spawn(cmd_nomad);

  // Act like a reaping init
  while(1) {
    alarm(TIMEO);
    sigwait(&set, &sig);
    for (i = 0; i < LEN(sigmap); i++) {
      if (sigmap[i].sig == sig) {
        sigmap[i].handler();
        break;
      }
    }
  }

  return 0;
}

char * find_iface() {
    char *iface = NULL;

    struct ifaddrs *ifaddr;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        sleep(5);
        exit(EXIT_FAILURE);
    }

    /* Walk through linked list, maintaining head pointer so we can free list later. */
    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!strcmp("lo", ifa->ifa_name)) continue;      // Not interested in local device
        if (!strncmp("sit", ifa->ifa_name, 3)) continue; // Not interested to ipv4->ipv6 mapper
        iface = calloc(1, strlen(ifa->ifa_name) + 1);
        strcpy(iface, ifa->ifa_name);
        break;
    }

    freeifaddrs(ifaddr);
    return iface;
}

int ifup(char *ifname) {
    int sockfd;
    struct ifreq ifr;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        return sockfd;
    memset(&ifr, 0, sizeof ifr);
    strncpy(ifr.ifr_name, ifname, MIN(strlen(ifname),IFNAMSIZ));
    ifr.ifr_flags |= IFF_UP;
    ioctl(sockfd, SIOCSIFFLAGS, &ifr);
    printf("IFUP: %s\n", ifname);
    return sockfd;
}

static void
sigpoweroff(void) {
  if (syscall(__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL) < 0)
    perror("poweroff");
  _exit(1);
}

static void
sigreap(void) {
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  alarm(TIMEO);
}

static void
sigreboot(void) {
  sleep(1);
}
