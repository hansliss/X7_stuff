#ifndef X7_CONTROLS_H
#define X7_CONTROLS_H
/* Edit button masks here, then rebuild. Multiple buttons may share an action.
 * Left stick aliases the D-pad; right stick aliases the face buttons in KEY.KO.
 * Names below denote physical buttons, not configurable actions. */
#define X7_BUTTON_SELECT 0x000004u
#define X7_BUTTON_START  0x000008u
#define X7_BUTTON_UP     0x000010u
#define X7_BUTTON_DOWN   0x000020u
#define X7_BUTTON_LEFT   0x000040u
#define X7_BUTTON_RIGHT  0x000080u
#define X7_BUTTON_L      0x000100u
#define X7_BUTTON_R      0x000200u
#define X7_BUTTON_A      0x000400u
#define X7_BUTTON_B      0x000800u
#define X7_BUTTON_X      0x001000u
#define X7_BUTTON_Y      0x002000u
#define X7_BUTTON_HOME   0x200000u

/* Gameplay actions. Use 0 to disable an action. */
#define X7_FORWARD      X7_BUTTON_UP
#define X7_BACKWARD     X7_BUTTON_DOWN
#define X7_TURN_LEFT    X7_BUTTON_LEFT
#define X7_TURN_RIGHT   X7_BUTTON_RIGHT
#define X7_STRAFE_LEFT  X7_BUTTON_L
#define X7_STRAFE_RIGHT X7_BUTTON_R
#define X7_FIRE         X7_BUTTON_A
#define X7_USE          X7_BUTTON_B
#define X7_RUN          X7_BUTTON_X
#define X7_NEXT_WEAPON  X7_BUTTON_Y
#define X7_AUTOMAP      X7_BUTTON_SELECT
#define X7_OPEN_MENU    X7_BUTTON_START

/* Menu/prompt actions are independent of gameplay mappings. */
#define X7_MENU_UP      X7_BUTTON_UP
#define X7_MENU_DOWN    X7_BUTTON_DOWN
#define X7_MENU_LEFT    (X7_BUTTON_LEFT | X7_BUTTON_L)
#define X7_MENU_RIGHT   (X7_BUTTON_RIGHT | X7_BUTTON_R)
#define X7_MENU_CONFIRM X7_BUTTON_A
#define X7_MENU_BACK    X7_BUTTON_B
#define X7_MENU_CLOSE   X7_BUTTON_START
/* Require these held actions to be released when the menu context changes. */
#define X7_CONTEXT_RELEASE (X7_FIRE | X7_USE | X7_RUN | X7_NEXT_WEAPON | X7_AUTOMAP | X7_STRAFE_LEFT | X7_STRAFE_RIGHT | X7_MENU_CONFIRM | X7_MENU_BACK)
#endif
