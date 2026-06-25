#include "libcamac.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stddef.h>

#define CMC_CTL_RESET   _IO('Z', 0)
#define CMC_CTL_R1      _IO('Z', 1)

int camac_open(const char *device_path) {
    return open(device_path, O_RDWR);
}

void camac_close(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

int camac_reset(int fd) {
    return ioctl(fd, CMC_CTL_RESET);
}

int camac_status(int fd) {
    return ioctl(fd, CMC_CTL_R1);
}

ssize_t camac_write(int fd, const void *buf, size_t count) {
    return write(fd, buf, count);
}

ssize_t camac_read(int fd, void *buf, size_t count) {
    return read(fd, buf, count);
}

int camac_flush(int fd) {
    int data = 0x0E000000;
    return camac_write(fd, &data, 4);
}

ssize_t camac_write_short(int fd, short value) {
    return write(fd, &value, sizeof(short));
}

int camac_stop_acqn(int fd, int crate) {
    // IOC_VTRANSPD_WRITE1 = 0x8004, IOC_VTRANSPD_WRITE2 = 0x8005
    int cmd = (crate == 1) ? 0x8004 : 0x8005;
    return ioctl(fd, cmd, NULL);
}

int camac_naf_full(int fd, int data, int n, int a, int f, int *x_res, int *q_res, int *lam) {
    int d, response;

    d = 0xFFFFFFFF;
    camac_write(fd, &d, 4);
    d = 0x00000000;
    camac_write(fd, &d, 4);
    
    if ((f > 15) && (f < 24)) {
        d = (1 << 24) | data;
        camac_write(fd, &d, 4);
    }
    
    d = a | (f << 4) | (n << 9);
    camac_write(fd, &d, 4);
    
    camac_flush(fd);
    
    response = 0;
    camac_read(fd, &response, 4);
    
    if (x_res) *x_res = (response >> 24) & 1;
    if (q_res) *q_res = (response >> 25) & 1;
    if (lam)   *lam = (response >> 26) & 1;
    
    return response;
}

int camac_naf(int fd, int data, int n, int a, int f, int *x_res, int *q_res, int *lam) {
    return camac_naf_full(fd, data, n, a, f, x_res, q_res, lam) & 0xFFFFFF;
}

int camac_lp_header(int fd) {
    int d;
    d = 0xFFFFFFFF; camac_write(fd, &d, 4);
    d = 0x00000000; camac_write(fd, &d, 4);
    return 0;
}

int camac_lp_store_next(int fd, int program_location) {
    int d = (3 << 24) | program_location;
    return camac_write(fd, &d, 4);
}

int camac_lp_naf(int fd, int n, int a, int f) {
    int d = a | (f << 4) | (n << 9);
    return camac_write(fd, &d, 4);
}

int camac_lp_delay(int fd, int value) {
    int d = (5 << 24) | value;
    return camac_write(fd, &d, 4);
}

int camac_lp_literal(int fd, int value) {
    int d = (12 << 24) | value;
    return camac_write(fd, &d, 4);
}

int camac_lp_repeat(int fd, int stop_if_no_q, int repeats) {
    int d = (2 << 24) | (stop_if_no_q << 23) | repeats;
    return camac_write(fd, &d, 4);
}

int camac_lp_quit(int fd) {
    int d = (31 << 24);
    return camac_write(fd, &d, 4);
}
