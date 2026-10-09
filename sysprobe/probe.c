#include "app_abi.h"
#ifdef PROBE_UI
#include "ui_abi.h"
#ifdef PROBE_DISCOVERY
#include "discovery_config.h"
#define LOG DISCOVERY_LOG
#define START_LOG DISCOVERY_START_LOG
#elif defined(PROBE_FAST)
#define LOG "/mnt/card/appprobe-dodeca-fast.txt"
#define START_LOG "/mnt/card/appprobe-dodeca-fast-start-error.txt"
#elif defined(PROBE_FB)
#define LOG "/mnt/card/appprobe-dodeca-fb.txt"
#define START_LOG "/mnt/card/appprobe-dodeca-fb-start-error.txt"
#elif defined(PROBE_DODECA)
#define LOG "/mnt/card/appprobe-dodeca.txt"
#define START_LOG "/mnt/card/appprobe-dodeca-start-error.txt"
#elif defined(PROBE_BITMAP)
#define LOG "/mnt/card/appprobe-bitmap.txt"
#define START_LOG "/mnt/card/appprobe-bitmap-start-error.txt"
#elif defined(PROBE_HSV)
#define LOG "/mnt/card/appprobe-hsv.txt"
#define START_LOG "/mnt/card/appprobe-hsv-start-error.txt"
#elif defined(PROBE_FONT)
#define LOG "/mnt/card/appprobe-font.txt"
#define START_LOG "/mnt/card/appprobe-font-start-error.txt"
#else
#define LOG "/mnt/card/appprobe-ui.txt"
#define START_LOG "/mnt/card/appprobe-ui-start-error.txt"
#endif
#else
#define LOG "/mnt/card/appprobe.txt"
#define START_LOG "/mnt/card/appprobe-start-error.txt"
#endif
#define WORK "/mnt/card/appprobe-work.bin"
static int log_fd = -1;
static int log_ok = 1;
static unsigned int failures;
static char line[192];
static unsigned int used;
static void append(const char *s)
{
    while (*s && used < sizeof(line)-1) line[used++] = *s++;
}
static void hex(x7_u32 value)
{
    static const char digits[] = "0123456789abcdef";
    int shift;
    for (shift = 28; shift >= 0; shift -= 4)
        line[used++] = digits[(value >> shift) & 15];
}
static int record(const char *tag, x7_u64 value)
{
    unsigned int attempts, remaining;
    const char *p = line;
    /* Leave room for 0x, 16 digits, and newline even for a long tag. */
    used = 0;
    append(tag);
    if (used > sizeof(line)-21) used = sizeof(line)-21;
    append(" 0x"); hex((x7_u32)(value >> 32)); hex((x7_u32)value); append("\n");
    remaining = used;
    if (!log_ok) return 0;
    for (attempts = 0; remaining && attempts < 32; ++attempts) {
        int n = sys_write(log_fd, p, remaining);
        if (n <= 0 || (unsigned int)n > remaining) { log_ok = 0; return 0; }
        remaining -= (unsigned int)n; p += n;
    }
    if (remaining || sys_fsync(log_fd) < 0) log_ok = 0;
    return log_ok;
}
static int before(const char *tag)
{
    if (!record(tag, 0)) return 0;
    OSTimeDly(1);
    return 1;
}
#ifndef PROBE_UI
static void result(const char *tag, x7_s64 value, x7_s64 expected)
{
    record(tag, (x7_u64)value);
    record("EXPECTED", (x7_u64)expected);
    record(value == expected ? "PASS" : "FAIL", 0);
    if (value != expected) ++failures;
}
#endif
void app_start_error(int stage, int status)
{
    log_fd = sys_open(START_LOG, X7_O_WRONLY | X7_O_CREAT | X7_O_TRUNC, 0600);
    if (log_fd < 0) return;
    record("app CRT failure stage", (x7_u64)(x7_s64)stage);
    record("status", (x7_u64)(x7_s64)status);
    sys_close(log_fd);
}
#ifndef PROBE_UI
static void file_probe(void)
{
    static const char payload[] = "0123456789abcdefABCDEFGHIJKLMNOP";
    struct x7_stat st;
    unsigned int i;
    int fd, n;
    /* Prevent aggregate initialization from introducing a libc memset call. */
    for (i = 0; i < sizeof(st); ++i) ((volatile x7_u8 *)&st)[i] = 0;
    if (!before("BEFORE scratch collision check")) return;
    fd = sys_open(WORK, X7_O_RDONLY, 0);
    record("collision check", (x7_u64)(x7_s64)fd);
    if (fd >= 0) { sys_close(fd); record("SKIP existing scratch", 0); return; }
    if (fd != -2) { record("SKIP absence not established", 0); return; }
    if (!before("BEFORE create 32-byte scratch")) return;
    fd = sys_open(WORK, X7_O_RDWR | X7_O_CREAT | X7_O_TRUNC, 0600);
    record("scratch fd", (x7_u64)(x7_s64)fd);
    if (fd < 0) { ++failures; return; }
    if (!before("BEFORE write")) goto out;
    result("write", sys_write(fd, payload, 32), 32);
    if (!before("BEFORE 64-bit tell")) goto out;
    result("tell", sys_tell(fd), 32);
    if (!before("BEFORE seek END -4")) goto out;
    result("seek", sys_lseek(fd, -4, X7_SEEK_END), 28);
    if (!before("BEFORE fstat recovered layout")) goto out;
    n = sys_fstat(fd, &st);
    result("fstat", n, 0);
    if (n == 0) {
        result("stat size", st.st_size, 32);
        record("stat mode", st.st_mode);
        record("stat allocation size", st.st_blksize);
    }
out:
    sys_close(fd);
    if (before("BEFORE remove owned scratch")) result("remove", sys_remove(WORK), 0);
}
#endif
void *process_start(void *argument)
{
    struct app_process_prefix *process;
    void *libc_fs, *applib = 0;
#ifdef PROBE_DISCOVERY
    static const char *const candidates[] = {
        "/mnt/diska/lib/commonui/commonui.so", "/mnt/diska/lib/fusion.so",
        "/mnt/diska/lib/style.so", "apconfig.so"
    };
    void *candidate_handles[4];
    void *discovery_gui = 0;
    unsigned candidate;
    for (candidate = 0; candidate < 4; ++candidate) candidate_handles[candidate] = 0;
#endif
    int ui_initialized = 0;
    int pid = app_getpid();
    unsigned int argc = 0;
    (void)argument;
    log_fd = sys_open(LOG, X7_O_WRONLY | X7_O_CREAT | X7_O_TRUNC, 0600);
    if (log_fd < 0) { app_exit(0); return 0; }
#ifdef PROBE_UI
#ifdef PROBE_DISCOVERY
    record("X7 default-font discovery " DISCOVERY_NAME " v1", 1);
#elif defined(PROBE_DODECA)
    #ifdef PROBE_FAST
    record("X7 fast framebuffer dodecahedron demo v3", 3);
#elif defined(PROBE_FB)
    record("X7 framebuffer dodecahedron demo v3", 3);
#else
    record("X7 rotating dodecahedron demo v2", 2);
#endif
#elif defined(PROBE_BITMAP)
    record("X7 fullscreen bitmap HSV probe v1", 1);
#elif defined(PROBE_HSV)
    record("X7 fullscreen HSV probe v2", 2);
#elif defined(PROBE_FONT)
    record("X7 standalone font probe v1", 1);
#else
    record("X7 standalone UI probe v4", 4);
#endif
#else
    record("X7 standalone app probe v2", 2);
#endif
    record("worker pid", (x7_u64)(x7_s64)pid);
    record("initial ticks", OSTimeGet());
    if (!before("BEFORE get process struct")) goto done;
    process = app_get_process(pid);
    record("process struct", (x7_word_t)process);
    if (!process) { ++failures; goto done; }
    if (process->argv) {
        /* Bounded scan; these pointers come from the vendor process object. */
        while (argc < 32 && process->argv[argc]) ++argc;
    }
    record("argc (capped at 32)", argc);
    if (!before("BEFORE dlopen libc_fs.so")) goto done;
    /* The vendor process_start does the same open, retrying once. Process exit
     * owns teardown; retain the handle through exit as its CRT does. */
    libc_fs = x7_dlopen("libc_fs.so", 2);
    if (!libc_fs) libc_fs = x7_dlopen("libc_fs.so", 2);
    record("libc_fs handle", (x7_word_t)libc_fs);
    if (!libc_fs) { ++failures; goto done; }
    if (!before("BEFORE dlopen applib.so")) goto done;
    applib = x7_dlopen("/mnt/diska/lib/applib.so", 1);
    record("applib handle", (x7_word_t)applib);
    if (!applib) { ++failures; goto done; }
    if (argc == 32) {
        record("STOP argv exceeded bounded scan", 0);
        ++failures;
        goto done;
    }
#ifdef PROBE_DISCOVERY
    /* Each run adds exactly one candidate, or all four for the combined run.
     * Retain these handles until after applib_quit. GUI also precedes init,
     * matching the calculator's initialization ordering. */
    for (candidate = 0; candidate < 4; ++candidate) {
        /* Vendor order: commonui, fusion, GUI, style, apconfig. */
        if (candidate == 2) {
            if (!before("BEFORE discovery dlopen gui.so")) goto done;
            discovery_gui = x7_dlopen("gui.so", 1);
            record("discovery GUI handle", (x7_word_t)discovery_gui);
            if (!discovery_gui) { ++failures; goto done; }
        }
        if (PROBE_DISCOVERY != 5 && candidate != PROBE_DISCOVERY - 1) continue;
        if (!before("BEFORE candidate dlopen")) goto done;
        record(candidates[candidate], 0);
        candidate_handles[candidate] = x7_dlopen(candidates[candidate], 1);
        record("candidate handle", (x7_word_t)candidate_handles[candidate]);
        if (!candidate_handles[candidate]) {
            const char *error = x7_dlerror();
            if (error) record(error, 0);
            ++failures; goto done;
        }
    }
#endif
    if (!before("BEFORE applib_init")) goto done;
    app_ui_init((int)argc, process->argv, 0);
    ui_initialized = 1;
    record("RETURNED applib_init", 0);
#ifdef PROBE_HSV
    record("ticks after runtime and applib setup", OSTimeGet());
#endif
#ifdef PROBE_DISCOVERY
    if (before("BEFORE default font getter")) {
        const char *path = ui_font_file();
        record("RETURNED default font getter", (x7_word_t)path);
        if (path) record(path, 0);
        else ++failures;
    }
#elif defined(PROBE_UI)
    if (ui_probe_run(record, before) < 0) ++failures;
#else
    file_probe();
#endif
done:
    /* Honor the lifecycle even if logging has failed. No early return after
     * init may bypass quit. The vendor calculator closes applib after quit. */
    if (ui_initialized) {
        before("BEFORE applib_quit");
        app_ui_quit();
        record("RETURNED applib_quit", 0);
    }
#ifdef PROBE_DISCOVERY
    for (candidate = 4; candidate > 0; --candidate) {
        if (!candidate_handles[candidate-1]) continue;
        before("BEFORE candidate dlclose");
        record(candidates[candidate-1], 0);
        record("candidate dlclose status", (x7_u64)(x7_s64)x7_dlclose(candidate_handles[candidate-1]));
    }
    if (discovery_gui) {
        before("BEFORE discovery GUI dlclose");
        record("discovery GUI dlclose status", (x7_u64)(x7_s64)x7_dlclose(discovery_gui));
    }
#endif
    if (applib) {
        before("BEFORE dlclose applib");
        record("applib dlclose status", (x7_u64)(x7_s64)x7_dlclose(applib));
    }
    if (log_ok) {
        record("final ticks", OSTimeGet());
        record("assertion failures", failures);
        record("DONE", 0);
        record("BEFORE group-7 exit(0)", 0);
    }
    sys_close(log_fd);
    log_fd = -1;
    app_exit(0);
    /* If exit unexpectedly returns, return through pthread's trampoline. */
    return 0;
}
