#ifndef GCU_DISP_BOARD_H
#define GCU_DISP_BOARD_H

/* Returns 1 on success; 0 leaves the product running face-less
 * (serial + LEDs + audio still work; a clear status line is printed). */
int gcu_board_disp_init(void);

/* Push an RGB565 region (row-major w*h) to the panel; blocks briefly. */
void gcu_board_disp_blit(int x, int y, int w, int h, const unsigned short *px);

#endif
