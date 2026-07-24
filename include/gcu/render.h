#ifndef GCU_RENDER_H
#define GCU_RENDER_H

#include "gcu/details.h"
#include "gcu/gfx.h"
#include "gcu/themes.h"

/* Portable compositor (§4.2, §4.5, §5.2). Regional paints so routine animation
 * never clears the whole panel: wink -> eye box, banner -> hair bar, cue ->
 * cue strip, Details numbers -> value fields. Full clears are for mode changes
 * only (theme switch, enter/leave Details). */

/* Snapshot the UI needs to paint the living face. */
typedef struct {
  gcu_theme_t theme;
  int wink_closed;   /* right eye currently closed */
  int banner_offset; /* hair scroll offset in px, wraps at banner span */
  int playing;       /* show the "First by Tig" playing cue */
} gcu_face_view_t;

/* One banner scroll span (px) for the current text+scale, used to wrap the
 * offset from gcu_banner_offset(). */
int gcu_render_banner_span(int scale);

/* Full face repaint (mode change): bg clear + hair + eyes + smile + cue +
 * hints. */
void gcu_render_face(gcu_gfx_t *g, const gcu_face_view_t *v);

/* Regional repaints (routine animation). */
void gcu_render_hair(gcu_gfx_t *g, const gcu_face_view_t *v);
void gcu_render_eye(gcu_gfx_t *g, const gcu_face_view_t *v, int right);
void gcu_render_cue(gcu_gfx_t *g, const gcu_face_view_t *v);
void gcu_render_hints(gcu_gfx_t *g, const gcu_face_view_t *v);

/* Details screen (§4.5). Chrome is a full repaint (mode change); values are a
 * regional repaint of the number fields only. id_line is the identity string
 * shown at the top. */
void gcu_render_details_chrome(gcu_gfx_t *g, gcu_theme_t theme,
                               const char *id_line);
void gcu_render_details_values(gcu_gfx_t *g, gcu_theme_t theme,
                               const gcu_details_t *d);

#endif
