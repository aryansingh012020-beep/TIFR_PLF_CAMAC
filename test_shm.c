#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
int main() {
    int fd = shm_open("/lamps_cmd_shm", O_RDWR, 0);
    if (fd < 0) { perror("shm_open"); return 1; }
    printf("Success!\n");
    return 0;
}
