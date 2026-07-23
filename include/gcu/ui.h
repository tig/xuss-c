#ifndef GCU_UI_H
#define GCU_UI_H

/* Product UI state machine — pure logic, wall-clock driven (spec.md §4, §5).
 * No device headers. Backends consume dirty flags as regional-paint
 * requests and service audio requests. */

typedef enum {
  GCU_SCREEN_FACE,
  GCU_SCREEN_DETAILS,
} gcu_screen_t;

typedef enum {
  GCU_THEME_BLUE,
  GCU_THEME_ORANGE,
  GCU_THEME_RED,
  GCU_THEME_GREEN,
  GCU_THEME_BLACK,
  GCU_THEME_COUNT,
} gcu_theme_t;

typedef enum {
  GCU_SONG_IDLE,
  GCU_SONG_PLAYING,
  GCU_SONG_PAUSED,
} gcu_song_t;

typedef enum {
  GCU_BTN_A,
  GCU_BTN_B,
  GCU_BTN_C,
} gcu_button_t;

/* Audio request for the backend (resume offset is backend-owned). */
typedef enum {
  GCU_AUDIO_NONE,
  GCU_AUDIO_START,  /* from byte 0 */
  GCU_AUDIO_PAUSE,  /* keep offset */
  GCU_AUDIO_RESUME, /* from kept offset */
} gcu_audio_req_t;

/* Regional-paint dirty flags (spec §4.2/§5.2.7: wink repaints the eye
 * only; banner repaints the hair strip only; full clears are for mode
 * changes). */
#define GCU_DIRTY_SCREEN 0x01u  /* mode change: full repaint of new screen */
#define GCU_DIRTY_THEME 0x02u   /* retint face + sides + banner ink */
#define GCU_DIRTY_EYE 0x04u     /* right eye open<->closed only */
#define GCU_DIRTY_BANNER 0x08u  /* banner strip step only */
#define GCU_DIRTY_CUE 0x10u     /* playing cue show/hide only */
#define GCU_DIRTY_DETAILS 0x20u /* Details value fields only */

typedef struct {
  gcu_screen_t screen;
  int theme; /* gcu_theme_t */
  int song;  /* gcu_song_t */
  int mute;  /* commissioning: 1 blocks starting playback */
  int start_blocked; /* set when mute blocked a start; caller reports+clears */
  int banner_step;   /* monotonic; renderer wraps by text width */
  int eye_closed;

  unsigned wink_due_ms;
  unsigned wink_open_ms;
  unsigned banner_due_ms;
  unsigned details_due_ms;

  unsigned dirty;
} gcu_ui_t;

void gcu_ui_init(gcu_ui_t *ui, unsigned now_ms);
gcu_audio_req_t gcu_ui_on_button(gcu_ui_t *ui, gcu_button_t btn,
                                 unsigned now_ms);
void gcu_ui_tick(gcu_ui_t *ui, unsigned now_ms);
void gcu_ui_song_ended(gcu_ui_t *ui);  /* natural end: idle, no loop */
void gcu_ui_song_error(gcu_ui_t *ui);  /* asset missing/short: back to idle */
unsigned gcu_ui_take_dirty(gcu_ui_t *ui);

#endif
