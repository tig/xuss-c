#ifndef XUSSC_STORAGE_H
#define XUSSC_STORAGE_H

#include <stddef.h>

int storage_init(void);
/* Returns size of /spiffs/first.pcm, or 0 if missing/empty. */
size_t storage_first_pcm_size(void);
const char *storage_first_pcm_path(void);

#endif
