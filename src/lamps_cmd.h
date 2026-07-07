/* =========================================================================
 * lamps_cmd.h — LAMPS EPICS Remote Command Channel
 *
 * Phase 3: minimal one-way command IPC from the EPICS bridge to LAMPS.
 * Phase 4: extended to carry a run name for remote START.
 *
 * Design principles:
 *   - Completely separate from lamps_shm.h (data SHM is unchanged).
 *   - One tiny POSIX SHM segment: /lamps_cmd_shm
 *   - Bridge writes commands; LAMPS polls and clears them.
 *   - No mutex: a single atomic uint32 is sufficient for one writer +
 *     one reader on the same host (x86 guarantees 32-bit aligned stores
 *     are atomic; stdatomic is used for correctness on other arches).
 *   - run_name is written BEFORE the atomic cmd store, so LAMPS always
 *     sees a consistent name when it reads cmd != NONE.
 *
 * Writer (bridge process):
 *   lamps_cmd_open_write()                — create segment
 *   lamps_cmd_post(LAMPS_CMD_STOP)        — post stop/reset
 *   lamps_cmd_post_start("run001")        — post start with run name
 *   lamps_cmd_close()
 *
 * Reader (LAMPS process, polled from the GTK idle handler or main loop):
 *   lamps_cmd_open_read()
 *   cmd = lamps_cmd_poll()               — returns cmd, clears it atomically
 *   lamps_cmd_get_run_name(buf, len)     — read run name (after cmd == START)
 *   lamps_cmd_close()
 * ========================================================================= */

#ifndef LAMPS_CMD_H
#define LAMPS_CMD_H

#include <stdint.h>
#include <stdatomic.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Command values — stored in LampsCmdBlock.cmd
 * -------------------------------------------------------------------------*/
#define LAMPS_CMD_NONE   0   /* no pending command                           */
#define LAMPS_CMD_START  1   /* start acquisition (run_name field is valid)  */
#define LAMPS_CMD_STOP   2   /* stop acquisition cleanly                     */
#define LAMPS_CMD_RESET  3   /* stop + zero all spectra                      */

/* -------------------------------------------------------------------------
 * Shared memory layout — fits comfortably in one cache line + a bit
 *
 * IMPORTANT: bump LAMPS_CMD_MAGIC whenever you change this struct so that
 * a stale segment from a previous version is detected and rejected.
 * -------------------------------------------------------------------------*/
#define LAMPS_CMD_SHM_NAME   "/lamps_cmd_shm"
#define LAMPS_CMD_MAGIC      0xCA110002UL    /* Phase 4: added run_name field */
#define LAMPS_CMD_NAME_LEN   40             /* matches RunName[40] in lamps.h */

typedef struct {
    uint32_t          magic;                    /* sanity: LAMPS_CMD_MAGIC   */
    volatile uint32_t cmd;                      /* LAMPS_CMD_* — atomic      */
    char              run_name[LAMPS_CMD_NAME_LEN]; /* valid when cmd==START */
} LampsCmdBlock;

/* -------------------------------------------------------------------------
 * API — implemented in lamps_cmd.c
 * -------------------------------------------------------------------------*/

/* Bridge side (writer): create the segment */
int  lamps_cmd_open_write(void);

/* LAMPS side (reader): attach to existing segment */
int  lamps_cmd_open_read(void);

/* Bridge: post a stop/reset command.
 * Returns 0 on success, -1 if SHM not attached.                            */
int  lamps_cmd_post(uint32_t cmd);

/* Bridge: post a START command with an associated run name.
 * run_name is copied to the SHM BEFORE the atomic cmd store.               */
int  lamps_cmd_post_start(const char *run_name);

/* LAMPS: read the pending command and atomically clear it.
 * Returns LAMPS_CMD_NONE if nothing pending or SHM not attached.           */
uint32_t lamps_cmd_poll(void);

/* LAMPS: copy the run_name field into buf (safe to call after poll()).
 * buf is always NUL-terminated.  Returns 0 on success, -1 if not attached. */
int  lamps_cmd_get_run_name(char *buf, size_t len);

/* Check if the command SHM is currently attached (non-zero = yes).
 * Used by LAMPS to lazy-retry connection when bridge starts after LAMPS. */
int  lamps_cmd_is_attached(void);

/* Both sides: unmap (writer also unlinks). */
void lamps_cmd_close(void);

#endif /* LAMPS_CMD_H */
