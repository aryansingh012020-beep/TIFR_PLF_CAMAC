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
int camac_naf_full(int fd, int data, int n, int a, int f, int *x_res, int *q_res, int *lam);

/* Execute a CAMAC NAF transaction. 
 * Returns the lower 24-bits of the response.
 * Status bits X, Q, LAM are optionally stored in pointers if non-NULL.
 */
int camac_naf(int fd, int data, int n, int a, int f, int *x_res, int *q_res, int *lam);

/* Write data directly to the CAMAC device stream */
ssize_t camac_write(int fd, const void *buf, size_t count);

/* Read data directly from the CAMAC device stream */
ssize_t camac_read(int fd, void *buf, size_t count);

/* Write a 16-bit short to the device */
ssize_t camac_write_short(int fd, short value);

/* Flush the output buffer to the CAMAC device */
int camac_flush(int fd);

/* Stop acquisition on crate 1 / 2 */
int camac_stop_acqn(int fd, int crate);

/* LP Programming Stream functions */
int camac_lp_header(int fd);
int camac_lp_store_next(int fd, int program_location);
int camac_lp_naf(int fd, int n, int a, int f);
int camac_lp_delay(int fd, int value);
int camac_lp_literal(int fd, int value);
int camac_lp_repeat(int fd, int stop_if_no_q, int repeats);
int camac_lp_quit(int fd);

#endif // LIBCAMAC_H
