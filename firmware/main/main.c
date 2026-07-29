#include "gcu/defaults.h"
#include "gcu/domain.h"
#include "gcu/hal.h"
#include "gcu/version.h"
#include "hal_board.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Embedded boot riff (u8 mono @ GCU_SAMPLE_RATE_HZ). */
extern const uint8_t _binary_boot_riff_pcm_start[];
extern const uint8_t _binary_boot_riff_pcm_end[];

/*
 * Identity on the link (#78 / #79): answer "identity" with fw_name=… fw_version=….
 * stdin non-blocking so product face tick is never parked.
 */
static int g_stdin_nonblock;

static void stdin_set_nonblocking(void) {
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  if (flags < 0) {
    g_stdin_nonblock = 0;
    return;
  }
  if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) == 0) {
    g_stdin_nonblock = 1;
  } else {
    g_stdin_nonblock = 0;
  }
}

static void drain_identity_command(void) {
  static char line[48];
  static int n;
  int c;

  if (!g_stdin_nonblock) {
    return;
  }

  while ((c = getchar()) != EOF) {
    if (c == '\r' || c == '\n') {
      if (n > 0) {
        line[n] = '\0';
        char *p = line;
        while (*p && isspace((unsigned char)*p)) {
          p++;
        }
        if (strcmp(p, "identity") == 0) {
          char id[64];
          gcu_identity_line(id, (int)sizeof id);
          printf("%s\n", id);
          fflush(stdout);
        }
        n = 0;
      }
      continue;
    }
    if (n < (int)sizeof(line) - 1) {
      line[n++] = (char)c;
    } else {
      n = 0;
    }
  }
  clearerr(stdin);
  if (errno == EAGAIN || errno == EWOULDBLOCK) {
    errno = 0;
  }
}

void app_main(void) {
  char id[64];
  gcu_state_t st;
  gcu_hal_t *hal = gcu_make_board_hal();

  gcu_identity_line(id, (int)sizeof id);
  printf("%s\n", id);
  fflush(stdout);

  stdin_set_nonblocking();
  if (!g_stdin_nonblock) {
    printf("WARN: stdin not non-blocking; identity knock drain disabled\n");
    fflush(stdout);
  }

  int boot_len =
      (int)(_binary_boot_riff_pcm_end - _binary_boot_riff_pcm_start);
  gcu_init(&st, hal);
  gcu_set_assets(&st, _binary_boot_riff_pcm_start, boot_len, NULL, 0,
                 GCU_SAMPLE_RATE_HZ);
  /* Boot riff ~2.5s — operator may hear speaker greeting. */
  gcu_start_boot(&st);

  for (;;) {
    drain_identity_command();
    gcu_tick(&st);
    if (hal && hal->delay_ms) {
      hal->delay_ms(hal, gcu_tick_sleep_ms(&st));
    }
  }
}
