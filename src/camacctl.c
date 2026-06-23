/* camacctl - CAMAC controller command-line utility
 * Uses libcamac only. No GTK, no protocol knowledge here.
 *
 * Usage:
 *   camacctl status
 *   camacctl reset
 *   camacctl flush
 *   camacctl naf <N> <A> <F>
 *   camacctl naf <N> <A> <F> <data>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libcamac.h"

#define DEFAULT_DEVICE "/dev/cmc100_0"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: camacctl <status|reset|flush|naf N A F [data]>\n");
        return 1;
    }

    const char *device = getenv("CAMAC_DEVICE");
    if (!device) device = DEFAULT_DEVICE;

    int fd = camac_open(device);
    if (fd < 0) {
        perror("camac_open");
        fprintf(stderr, "Cannot open %s\n", device);
        return 2;
    }

    const char *cmd = argv[1];
    int rc = 0;

    if (strcmp(cmd, "status") == 0) {
        int s = camac_status(fd);
        if (s < 0) { perror("camac_status"); rc = 3; }
        else printf("Status: 0x%08X  unit=%d S=%d LAM=%d FIFObits=%d\n",
                    s, (s & 112) >> 4, (s & 8) >> 3, (s & 4) >> 2, s & 3);

    } else if (strcmp(cmd, "reset") == 0) {
        rc = camac_reset(fd);
        if (rc < 0) { perror("camac_reset"); rc = 3; }
        else printf("Reset OK\n");

    } else if (strcmp(cmd, "flush") == 0) {
        rc = camac_flush(fd);
        if (rc < 0) { perror("camac_flush"); rc = 3; }
        else printf("Flush OK\n");

    } else if (strcmp(cmd, "naf") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Usage: camacctl naf N A F [data]\n");
            rc = 1;
        } else {
            int n    = atoi(argv[2]);
            int a    = atoi(argv[3]);
            int f    = atoi(argv[4]);
            int data = (argc >= 6) ? atoi(argv[5]) : 0;
            int x_res = 0, q_res = 0, lam = 0;
            int result = camac_naf(fd, data, n, a, f, &x_res, &q_res, &lam);
            printf("NAF N=%d A=%d F=%d data=%d => result=0x%06X (%d)  X=%d Q=%d LAM=%d\n",
                   n, a, f, data, result, result, x_res, q_res, lam);
        }

    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        rc = 1;
    }

    camac_close(fd);
    return rc;
}
