#include "gcu/details.h"

#include <stdio.h>
#include <string.h>

#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c);              \
      return 1;                                                                \
    }                                                                          \
  } while (0)

/* Every character must be inside the required glyph set (§4.2). */
static int glyph_safe(const char *s) {
  for (; *s; s++) {
    char c = *s;
    if (c == ' ' || c == '+' || c == '-' || c == '.' ||
        (c >= '0' && c <= '9')) {
      continue;
    }
    return 0;
  }
  return 1;
}

int main(void) {
  char out[16];

  CHECK(gcu_fmt_milli1(out, sizeof out, 1234) > 0);
  CHECK(strcmp(out, "1.2") == 0);
  CHECK(gcu_fmt_milli1(out, sizeof out, 0) > 0);
  CHECK(strcmp(out, "0.0") == 0);
  CHECK(gcu_fmt_milli1(out, sizeof out, -50) > 0);
  CHECK(strcmp(out, "-0.0") == 0);
  CHECK(gcu_fmt_milli1(out, sizeof out, -1999) > 0);
  CHECK(strcmp(out, "-1.9") == 0);
  CHECK(gcu_fmt_milli1(out, sizeof out, 9999) > 0);
  CHECK(strcmp(out, "9.9") == 0);

  CHECK(glyph_safe("abc") == 0); /* letters rejected by the numeric-field check */
  CHECK(glyph_safe(out) == 1);
  CHECK(gcu_fmt_milli1(out, sizeof out, 25000) > 0);
  CHECK(glyph_safe(out) == 1);

  /* Too-small buffer fails closed, no partial garbage claimed. */
  char tiny[3];
  CHECK(gcu_fmt_milli1(tiny, sizeof tiny, 1234) == 0);

  printf("OK details\n");
  return 0;
}
