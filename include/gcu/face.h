#ifndef GCU_FACE_H
#define GCU_FACE_H

/* Screen renderer — pure logic (spec.md §4.2/§4.5). Paints any sub-rect
 * of the current screen into an RGB565 buffer; the display backend pushes
 * bands, host tests assert pixels. Regional-update contracts live here:
 * the wink rect covers only the right eye, the banner rect only the hair
 * strip, the Details value rect only the value columns. */

#define GCU_DISP_W 320
#define GCU_DISP_H 240

typedef struct {
  int x, y, w, h;
} gcu_rect_t;

typedef struct {
  int screen; /* gcu_screen_t */
  int theme;  /* gcu_theme_t */
  int song;   /* gcu_song_t: playing cue shown while PLAYING */
  int eye_closed;
  int banner_step;
  /* Details values (used when screen == DETAILS) */
  int ax_mg, ay_mg, az_mg;    /* milli-g */
  int gx_dps, gy_dps, gz_dps; /* deg/s */
  int temp_dc;                /* deci-degC */
  unsigned heap_free;
  int btn_a, btn_b, btn_c;
  char fw_line[32]; /* "XUSSC 0.0.1" */
} gcu_face_view_t;

gcu_rect_t gcu_face_rect_full(void);
gcu_rect_t gcu_face_rect_banner(void);
gcu_rect_t gcu_face_rect_eye(void);   /* right eye only (wink) */
gcu_rect_t gcu_face_rect_cue(void);   /* playing cue line */
gcu_rect_t gcu_face_rect_values(void); /* Details value columns */

/* Paint rect (screen coords) into buf, row-major rect.w * rect.h. */
void gcu_face_paint(const gcu_face_view_t *v, gcu_rect_t rect,
                    unsigned short *buf);

#endif
