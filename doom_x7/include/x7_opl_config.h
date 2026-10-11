#ifndef X7_OPL_CONFIG_H
#define X7_OPL_CONFIG_H
#define USE_EMU8950_OPL 1
#define EMU8950_NO_FLOAT 1
#define EMU8950_NO_RATECONV 1
#define EMU8950_NO_TIMER 1
#define EMU8950_NO_TEST_FLAG 1
#define EMU8950_NO_PERCUSSION_MODE 1
#define EMU8950_NO_TLL 1
#define EMU8950_NO_WAVE_TABLE_MAP 1
#define EMU8950_LINEAR 1
#define EMU8950_LINEAR_SKIP 1
#define EMU8950_LINEAR_END_OF_NOTE_OPTIMIZATION 1
#define EMU8950_SLOT_RENDER 0
#define EMU8950_ASM 0
#ifndef static_assert
#define static_assert _Static_assert
#endif
/* Assertions in the vendor buffer interface are guaranteed by our 128-sample
 * chunks. Keep freestanding builds independent of a host assertion runtime. */
#ifdef X7_PLAYABLE
#define assert(condition) ((void)0)
#endif
#endif
