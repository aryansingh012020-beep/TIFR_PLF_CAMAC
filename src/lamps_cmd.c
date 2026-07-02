/* =========================================================================
 * lamps_cmd.c — LAMPS EPICS Remote Command Channel implementation
 *
 * Phase 3: STOP/RESET command delivery via POSIX SHM.
 * Phase 4: Extended to carry run_name for remote START.
 * See lamps_cmd.h for design notes.
 * ========================================================================= */

#include "lamps_cmd.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>

static LampsCmdBlock *s_cmd      = NULL;
static int            s_is_writer = 0;
static const size_t   s_size      = sizeof(LampsCmdBlock);

/* =========================================================================
 * lamps_cmd_open_write()
 * Bridge: create and initialise the command segment.
 * ========================================================================= */
int lamps_cmd_open_write(void)
{
    int fd;

    shm_unlink(LAMPS_CMD_SHM_NAME);  /* clean up any stale segment */

    fd = shm_open(LAMPS_CMD_SHM_NAME,
                  O_CREAT | O_RDWR,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        perror("[LAMPS CMD] shm_open(write)");
        return -1;
    }

    if (ftruncate(fd, (off_t)s_size) < 0) {
        perror("[LAMPS CMD] ftruncate");
        close(fd);
        return -1;
    }

    s_cmd = mmap(NULL, s_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    if (s_cmd == MAP_FAILED) {
        perror("[LAMPS CMD] mmap(write)");
        s_cmd = NULL;
        return -1;
    }

    memset(s_cmd, 0, s_size);
    s_cmd->magic = LAMPS_CMD_MAGIC;
    s_cmd->cmd   = LAMPS_CMD_NONE;
    s_is_writer  = 1;

    fprintf(stderr, "[LAMPS CMD] command channel created (sz=%zu)\n", s_size);
    return 0;
}

/* =========================================================================
 * lamps_cmd_open_read()
 * LAMPS: attach to the existing command segment.
 * ========================================================================= */
int lamps_cmd_open_read(void)
{
    int fd;

    fd = shm_open(LAMPS_CMD_SHM_NAME, O_RDWR, 0);
    if (fd < 0) return -1;

    s_cmd = mmap(NULL, s_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    if (s_cmd == MAP_FAILED) {
        perror("[LAMPS CMD] mmap(read)");
        s_cmd = NULL;
        return -1;
    }

    if (s_cmd->magic != LAMPS_CMD_MAGIC) {
        fprintf(stderr,
                "[LAMPS CMD] bad magic 0x%08lX (expected 0x%08lX) — "
                "bridge may need rebuild. Detaching.\n",
                (unsigned long)s_cmd->magic,
                (unsigned long)LAMPS_CMD_MAGIC);
        munmap(s_cmd, s_size);
        s_cmd = NULL;
        return -1;
    }

    fprintf(stderr, "[LAMPS CMD] command channel attached\n");
    return 0;
}

/* =========================================================================
 * lamps_cmd_post()
 * Bridge: post a STOP or RESET command.
 * ========================================================================= */
int lamps_cmd_post(uint32_t cmd)
{
    if (!s_cmd) return -1;
    atomic_store_explicit((_Atomic uint32_t *)&s_cmd->cmd,
                          cmd, memory_order_release);
    return 0;
}

/* =========================================================================
 * lamps_cmd_post_start()
 * Bridge: post a START command with a run name.
 *
 * The run name is written BEFORE the atomic cmd store so that LAMPS always
 * reads a consistent name once it sees cmd == LAMPS_CMD_START.
 * ========================================================================= */
int lamps_cmd_post_start(const char *run_name)
{
    if (!s_cmd) return -1;

    /* Write run name first (non-atomic, but before the barrier below). */
    strncpy(s_cmd->run_name, run_name ? run_name : "",
            LAMPS_CMD_NAME_LEN - 1);
    s_cmd->run_name[LAMPS_CMD_NAME_LEN - 1] = '\0';

    /* Release barrier: ensures run_name is visible before cmd. */
    atomic_store_explicit((_Atomic uint32_t *)&s_cmd->cmd,
                          LAMPS_CMD_START, memory_order_release);
    return 0;
}

/* =========================================================================
 * lamps_cmd_poll()
 * LAMPS: read and clear the pending command atomically.
 * ========================================================================= */
uint32_t lamps_cmd_poll(void)
{
    uint32_t cmd;
    if (!s_cmd) return LAMPS_CMD_NONE;

    cmd = atomic_load_explicit((_Atomic uint32_t *)&s_cmd->cmd,
                               memory_order_acquire);
    if (cmd != LAMPS_CMD_NONE)
        atomic_store_explicit((_Atomic uint32_t *)&s_cmd->cmd,
                              LAMPS_CMD_NONE, memory_order_release);
    return cmd;
}

/* =========================================================================
 * lamps_cmd_get_run_name()
 * LAMPS: copy the run name into a caller-supplied buffer.
 * Call AFTER lamps_cmd_poll() returns LAMPS_CMD_START.
 * ========================================================================= */
int lamps_cmd_get_run_name(char *buf, size_t len)
{
    if (!s_cmd || !buf || len == 0) return -1;
    strncpy(buf, s_cmd->run_name, len - 1);
    buf[len - 1] = '\0';
    return 0;
}

/* =========================================================================
 * lamps_cmd_close()
 * Writer unlinks; reader just unmaps.
 * ========================================================================= */
void lamps_cmd_close(void)
{
    if (s_cmd) {
        munmap(s_cmd, s_size);
        s_cmd = NULL;
    }
    if (s_is_writer) {
        shm_unlink(LAMPS_CMD_SHM_NAME);
        s_is_writer = 0;
        fprintf(stderr, "[LAMPS CMD] command channel removed\n");
    }
}
