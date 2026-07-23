#ifndef GCU_APP_BOARD_H
#define GCU_APP_BOARD_H

/* Run the product app loop (buttons + link + UI + audio task).
 * song_len = PCM bytes in the audio partition (0 = missing: playback
 * refused with a clear link status). Never returns. */
void gcu_board_app_run(unsigned song_len);

#endif
