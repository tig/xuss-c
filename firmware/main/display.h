#ifndef XUSSC_DISPLAY_H
#define XUSSC_DISPLAY_H

#include <stdint.h>

int display_init(void);
void display_fill_rect(int x, int y, int w, int h, uint16_t rgb565);
/* Shadow framebuffer for esprec shot (RGB565 words, SPI byte order as drawn). */
const uint16_t *display_framebuffer(void);
int display_width(void);
int display_height(void);

#endif
