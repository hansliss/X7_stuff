/* Group-7 runtime and group-18 lifecycle, recovered from calculat.app.
 * Kept separate from the SYSCFG group-1/group-2 public syscall tables. */
#ifndef X7_APP_ABI_H
#define X7_APP_ABI_H
#include "../syscalls.h"
struct app_thread_attr { x7_u32 words[9]; }; /* vendor pthread_attr_t: 36 bytes */
typedef x7_u32 app_thread_id;                /* vendor pthread_t: 4 bytes */
/* Only the prefix accessed by the vendor CRT; full process object is opaque. */
struct app_process_prefix {
    x7_u32 unknown_00, unknown_04;
    int (*exec_release)(void *);
    x7_u32 unknown_0c[21];
    char **argv;                           /* +96 */
};
_Static_assert(sizeof(struct app_thread_attr) == 36, "vendor thread attr");
_Static_assert(__builtin_offsetof(struct app_process_prefix, exec_release) == 8 &&
               __builtin_offsetof(struct app_process_prefix, argv) == 96,
               "vendor process prefix");
int app_get_pid(int *pid);
int app_release_pid(int pid);
struct app_process_prefix *app_get_process(int pid);
int app_change_thread_pid(int pid);
int app_getpid(void);
int app_set_envp(int pid, const char *path, char **argv, char **envp);
int app_current_pid(void);
int app_insert_child(int pid, int parent);
int app_remove_child(int pid, int parent);
int app_thread_create(app_thread_id *, const struct app_thread_attr *,
                      void *(*start)(void *), void *argument);
int app_attr_init(struct app_thread_attr *);
int app_attr_destroy(struct app_thread_attr *);
int app_attr_inheritsched(struct app_thread_attr *, int);
int app_attr_detachstate(struct app_thread_attr *, int);
void app_exit(int status);
/* Group 18 application lifecycle, calculat.app main at 0x65802090/0x6580212c.
 * Console-verified init/quit sequence restores clean tools-menu exit.
 * Call only after loading applib.so. Return registers are not interpreted:
 * vendor callers do not check them. The third init argument is always NULL
 * in the inspected calculator call; its non-NULL semantics are unresolved. */
void app_ui_init(int argc, char **argv, void *option);
void app_ui_quit(void);
void *process_start(void *argument);
void app_start_error(int stage, int status);
#endif
