#ifndef XUSSC_LEDS_H
#define XUSSC_LEDS_H

#include <stdint.h>

int leds_init(void);
void leds_set_rgb(uint8_t r, uint8_t g, uint8_t b);

#endif
