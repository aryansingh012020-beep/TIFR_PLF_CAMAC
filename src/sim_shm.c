#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdatomic.h>
#include "lamps_shm.h"

/* -------------------------------------------------------------------------
 * Realistic gamma spectrum simulator
 *
 * Generates a 8192-channel spectrum with:
 *   Peak A  — "Co-60 (1173 keV)"  at ch  512,  FWHM ~20 ch
 *   Peak B  — "Cs-137 (662 keV)"  at ch 1024,  FWHM ~15 ch
 *   Peak C  — "K-40 (1461 keV)"   at ch 2048,  FWHM ~25 ch
 *   Compton continuum + flat noise floor
 *
 * Counts accumulate every tick (5 Hz) so the spectrum grows like a real run.
 * ------------------------------------------------------------------------- */

#define N_CHAN  8192
#define SIM_HZ  5

static double gauss(double ch, double mu, double fwhm, double amplitude) {
    double sigma = fwhm / 2.3548;
    double x = (ch - mu) / sigma;
    return amplitude * exp(-0.5 * x * x);
}

/* Simple XOR-shift PRNG */
static uint64_t s_rng = 0xdeadbeefcafe1234ULL;
static double rng_uniform(void) {
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 7;
    s_rng ^= s_rng << 17;
    return (double)(s_rng & 0xFFFFFFFFULL) / 4294967296.0;
}

/* Poisson variate */
static uint32_t poisson(double lam) {
    if (lam <= 0.0) return 0;
    if (lam > 30.0) {
        double u1 = rng_uniform() + 1e-12;
        double u2 = rng_uniform();
        double z  = sqrt(-2.0 * log(u1)) * cos(6.28318530 * u2);
        long   n  = (long)(lam + z * sqrt(lam) + 0.5);
        return (n < 0) ? 0 : (uint32_t)n;
    }
    uint32_t k = 0;
    double   L = exp(-lam), p = 1.0;
    do { p *= rng_uniform(); k++; } while (p > L);
    return k - 1;
}

int main(void) {
    int fd;
    LampsShmBlock *shm;
    uint32_t spectrum[N_CHAN];

    printf("Starting LAMPS SHM Simulator (realistic Gaussian spectrum)...\n");

    shm_unlink(LAMPS_SHM_NAME);
    fd = shm_open(LAMPS_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd < 0) { perror("shm_open"); return 1; }
    ftruncate(fd, sizeof(LampsShmBlock));
    shm = mmap(NULL, sizeof(LampsShmBlock),
               PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) { perror("mmap"); return 1; }
    close(fd);

    memset(shm, 0, sizeof(LampsShmBlock));
    memset(spectrum, 0, sizeof(spectrum));

    shm->magic      = LAMPS_SHM_MAGIC;
    shm->version    = LAMPS_SHM_VERSION;
    shm->generation = 0;
    shm->n_scaler   = 4;
    shm->n_oned     = 1;
    shm->oned_chan[0] = N_CHAN;
    strncpy(shm->oned_name[0], "DET1", 31);

    printf("3 peaks: ch512 (Co-60), ch1024 (Cs-137), ch2048 (K-40)\n");
    printf("Press Ctrl-C to stop.\n\n");

    uint64_t events = 0;
    uint64_t tick   = 0;

    while (1) {
        uint32_t new_evts = 0;

        for (int ch = 0; ch < N_CHAN; ch++) {
            double rate = 0.0;
            rate += gauss(ch,  512,  20,  8.0);   /* Co-60  */
            rate += gauss(ch, 1024,  15, 12.0);   /* Cs-137 */
            rate += gauss(ch, 2048,  25,  5.0);   /* K-40   */
            rate += 0.5 * exp(-(double)ch / 1200.0); /* Compton */
            rate += 0.05;                          /* noise floor */
            uint32_t n = poisson(rate);
            spectrum[ch] += n;
            new_evts     += n;
        }
        events += new_evts;
        tick++;

        /* seqlock write — generation is always odd during update */
        atomic_fetch_add_explicit(
            (_Atomic uint64_t *)&shm->generation, 1,
            memory_order_release);

        shm->acq_state         = LAMPS_ACQ_RUN;
        strncpy(shm->run_name, "SIM_RUN_01", 31);
        shm->total_events      = events;
        shm->buffers_acquired  = (uint32_t)tick;
        shm->buffers_processed = (uint32_t)tick;
        shm->elapsed_seconds   = (double)tick / SIM_HZ;
        shm->event_rate        = (double)new_evts * SIM_HZ;
        shm->scaler_val[0]     = (uint32_t)(events);
        shm->scaler_val[1]     = (uint32_t)(events * 2);
        shm->scaler_val[2]     = (uint32_t)(events / 3);
        shm->scaler_val[3]     = (uint32_t)(events / 7);

        memcpy(&shm->oned_data[0], spectrum, N_CHAN * sizeof(uint32_t));

        atomic_fetch_add_explicit(
            (_Atomic uint64_t *)&shm->generation, 1,
            memory_order_release);

        if (tick % (SIM_HZ * 10) == 0)
            printf("  t=%4.0fs  events=%7.0fk  "
                   "ch512=%6u  ch1024=%6u  ch2048=%6u\n",
                   (double)tick / SIM_HZ,
                   (double)events / 1000.0,
                   spectrum[512], spectrum[1024], spectrum[2048]);

        usleep(1000000 / SIM_HZ);
    }
    return 0;
}
