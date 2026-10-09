#include "app_abi.h"
void __attribute__((section(".init"))) _init(void) {}
void __attribute__((section(".fini"))) _fini(void) {}
/* Matches the vendor CRT's empty per-process executable-release callback. */
static int exec_release(void *argument) { (void)argument; return 0; }

/* Loader entry, NOT main(argc, argv). Reconstructed from calculat.app
 * __start 0x6580739c; manager.app has the same sequence at 0x600019f0.
 * Preserve the short scheduler-locked process/thread construction interval.
 * No filesystem I/O or sleeps while the scheduler is locked. */
int __start(const char *path, char **argv, char **envp)
{
    struct app_thread_attr attr;
    struct app_process_prefix *process;
    app_thread_id thread;
    int pid = 0, parent, saved_pid, status, stage = 1;
    int linked = 0, have_pid = 0, have_attr = 0;
    _init();
    OSSchedLock();
    status = app_get_pid(&pid);
    if (status) goto failure;
    have_pid = 1;
    parent = app_current_pid();
    app_insert_child(pid, parent); /* Vendor CRT does not inspect this return. */
    linked = 1;
    stage = 2;
    status = app_set_envp(pid, path, argv, envp);
    if (status) goto failure;
    stage = 3;
    process = app_get_process(pid);
    if (!process) { status = -1; goto failure; }
    process->exec_release = exec_release;
    stage = 4;
    status = app_attr_init(&attr);
    if (status) goto failure;
    have_attr = 1;
    stage = 5;
    status = app_attr_inheritsched(&attr, 2); /* vendor PTHREAD_EXPLICIT_SCHED */
    if (status) goto failure;
    stage = 6;
    status = app_attr_detachstate(&attr, 0); /* vendor PTHREAD_CREATE_JOINABLE */
    if (status) goto failure;
    stage = 7;
    saved_pid = app_change_thread_pid(pid);
    status = app_thread_create(&thread, &attr, process_start, 0);
    app_change_thread_pid(saved_pid);
    app_attr_destroy(&attr);
    have_attr = 0;
    if (status) goto failure;
    OSSchedUnlock();
    return 0;
failure:
    if (have_attr) app_attr_destroy(&attr);
    if (linked) app_remove_child(pid, app_current_pid());
    if (have_pid) app_release_pid(pid);
    OSSchedUnlock();
    app_start_error(stage, status);
    return -1;
}
