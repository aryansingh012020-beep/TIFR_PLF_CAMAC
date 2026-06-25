#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdatomic.h>
#include "lamps_shm.h"

int main() {
    int fd;
    LampsShmBlock *s_shm;
    
    printf("Starting LAMPS SHM Simulator...\n");
    
    shm_unlink(LAMPS_SHM_NAME);
    fd = shm_open(LAMPS_SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(LampsShmBlock));
    s_shm = mmap(NULL, sizeof(LampsShmBlock), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    memset(s_shm, 0, sizeof(LampsShmBlock));
    s_shm->magic = LAMPS_SHM_MAGIC;
    s_shm->version = LAMPS_SHM_VERSION;
    s_shm->generation = 0;
    
    s_shm->n_scaler = 4;
    s_shm->scaler_name[0][0] = 0;
    s_shm->scaler_name[1][0] = 0;
    
    s_shm->n_oned = 1;
    s_shm->oned_name[0][0] = 0;
    s_shm->oned_chan[0] = 100;
    
    printf("Simulator ready. Writing data...\n");
    
    uint64_t events = 0;
    while(1) {
        atomic_fetch_add_explicit((_Atomic uint64_t *)&s_shm->generation, 1, memory_order_release);
        
        s_shm->acq_state = LAMPS_ACQ_RUN;
        strcpy(s_shm->run_name, "TEST_RUN_01");
        s_shm->total_events = events;
        s_shm->buffers_acquired = events / 100;
        s_shm->elapsed_seconds = (double)events / 10.0;
        s_shm->event_rate = 10.0;
        
        s_shm->scaler_val[0] = events;
        s_shm->scaler_val[1] = events * 2;
        
        for(int i=0; i<100; i++) {
            s_shm->oned_data[i] = (events % 100) + i;
        }
        
        atomic_fetch_add_explicit((_Atomic uint64_t *)&s_shm->generation, 1, memory_order_release);
        
        events += 10;
        usleep(200000); // 5 Hz update
    }
    return 0;
}
