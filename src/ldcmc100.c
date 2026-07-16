/* Loader for CMC100 driver. The lamps Makefile compiles it.
 * Updated to use the actual driver path in the lamps installation directory.
 * Now Acquire->LOAD DRIVER in the lamps menu will work.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
int main(void)
{
int Err;
char kopath[PATH_MAX];
char cmd[PATH_MAX+64];

/* Find the directory of this executable to locate cmcamac.ko */
ssize_t len = readlink("/proc/self/exe", kopath, sizeof(kopath)-1);
if (len < 0) { perror("readlink"); return 1; }
kopath[len] = '\0';

/* Strip executable name, then go into driver/ subdirectory */
char *slash = strrchr(kopath, '/');
if (slash) *slash = '\0';
strncat(kopath, "/driver/cmcamac.ko", sizeof(kopath)-strlen(kopath)-1);

/* Unload old module if present (ignore errors) */
system("/sbin/rmmod cmcamac 2>/dev/null");

/* Load the module from the correct path */
snprintf(cmd, sizeof(cmd), "insmod %s", kopath);
Err = system(cmd);
if (Err) {
    printf("insmod failed! Check that %s exists and this program is run as root.\n", kopath);
    return 1;
}

sleep(12);  /* Wait for driver probe: CMCSLEEP=8s + communication time */
Err=system("chmod 066 /dev/cmcamac0 2>/dev/null");
if (Err) printf("CMC100 device is off or not connected!\n");
else printf("CMC100 driver loaded\n");
return 0;
}
