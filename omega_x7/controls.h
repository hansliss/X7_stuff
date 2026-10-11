#ifndef OMEGA_CONTROLS_H
#define OMEGA_CONTROLS_H
/* Raw KEY.KO masks; stick directions alias the adjacent buttons. */
#define OMEGA_LEFT   0x40u
#define OMEGA_RIGHT  0x80u
#define OMEGA_FIRE   0x400u
#define OMEGA_THRUST (0x800u | 0x10u)
#define OMEGA_COIN   0x4u
#define OMEGA_START  0x8u
#define OMEGA_QUIT   0x200000u
#define OMEGA_TURN_STEP 2u /* positions per 40 Hz update; 64 positions/revolution */
#endif
