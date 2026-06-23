#ifndef LIBCAMAC_H
#define LIBCAMAC_H

#include <sys/types.h>

/* Open the CMC100 device. Returns fd on success, -1 on failure. */
int camac_open(const char *device_path);

/* Close the device. */
void camac_close(int fd);

/* Reset the CMC100 controller. */
int camac_reset(int fd);

/* Get controller status (e.g. ioctl(fd, CMC_CTL_R1)) */
int camac_status(int fd);

/* Execute a CAMAC NAF transaction. 
 * Returns the full 32-bit response word.
 * Status bits X, Q, LAM are optionally stored in pointers if non-NULL.
 */
int camac_naf(int fd, int data, int n, int a, int f, int *x_res, int *q_res, int *lam);

/* Write data directly to the CAMAC device stream */
ssize_t camac_write(int fd, const void *buf, size_t count);

/* Read data directly from the CAMAC device stream */
ssize_t camac_read(int fd, void *buf, size_t count);

/* Flush the output buffer to the CAMAC device */
int camac_flush(int fd);

#endif // LIBCAMAC_H
