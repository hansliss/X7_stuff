/* Bounded, freestanding X7 syscall probe. See README.md before running. */
#include "../syscalls.h"

#ifndef PROBE_MODE
#define PROBE_MODE 0 /* 0 filesystem, 1 metadata, 2 directory, 3 watchdog */
#endif
#ifndef PROBE_YIELD
#define PROBE_YIELD 1
#endif
#if PROBE_MODE < 0 || PROBE_MODE > 3
#error "Invalid PROBE_MODE"
#endif

#define CARD "/mnt/card/"
#define WORK CARD "sysprobe-work.bin"
#define RENAMED CARD "sysprobe-renamed.bin"
#define DIR CARD "sysprobe-dir"
#define CHILD DIR "/entry.bin"

static const char *const log_paths[] = {
    CARD "sysprobe-fs.txt", CARD "sysprobe-metadata.txt",
    CARD "sysprobe-directory.txt", CARD "sysprobe-watchdog.txt"
};
static int log_fd = -1;
static int logging_ok = 1;
static unsigned int failures;
static char line[256];
static unsigned int line_len;
#if PROBE_MODE != 3
static const char payload[] = "0123456789abcdefABCDEFGHIJKLMNOP";
#endif

/* No printf, libc formatting, allocation, division, or helper dependencies. */
static void append(const char *s)
{
    while (*s && line_len < sizeof(line) - 1) line[line_len++] = *s++;
}
static void hex32(x7_u32 n)
{
    static const char digits[] = "0123456789abcdef";
    int shift;
    for (shift = 28; shift >= 0; shift -= 4)
        if (line_len < sizeof(line) - 1) line[line_len++] = digits[(n >> shift) & 15];
}
static int write_all(int fd, const void *p, unsigned int size)
{
    const char *bytes = p;
    unsigned int attempts;
    /* Bounded even if an unexpected implementation reports short writes. */
    for (attempts = 0; size && attempts < 32; ++attempts) {
        int n = sys_write(fd, bytes, size);
        if (n <= 0 || (unsigned int)n > size) return 0;
        bytes += n;
        size -= (unsigned int)n;
    }
    return size == 0;
}
static int record(const char *tag, x7_u64 value)
{
    line_len = 0;
    append(tag);
    append(" 0x");
    hex32((x7_u32)(value >> 32));
    hex32((x7_u32)value);
    append("\n");
    if (!logging_ok || !write_all(log_fd, line, line_len)) {
        logging_ok = 0;
        return 0;
    }
    /* Persist BEFORE markers so a reboot identifies the call that did not return. */
    if (sys_fsync(log_fd) < 0) {
        logging_ok = 0;
        return 0;
    }
    return 1;
}
static void result(const char *name, x7_s64 actual, x7_s64 expected)
{
    record(name, (x7_u64)actual);
    record("EXPECTED", (x7_u64)expected);
    record(actual == expected ? "PASS" : "FAIL", 0);
    if (actual != expected) ++failures;
}
static int before(const char *name)
{
    if (!record(name, 0)) return 0;
#if PROBE_YIELD
    OSTimeDly(1); /* Release the CPU briefly; never hold a scheduler lock. */
#endif
    return 1;
}
#if PROBE_MODE != 3
static int vacant(const char *path)
{
    int fd;
    if (!before("BEFORE collision-check open")) return 0;
    fd = sys_open(path, X7_O_RDONLY, 0);
    record("collision-check fd", (x7_u64)(x7_s64)fd);
    if (fd >= 0) {
        sys_close(fd);
        record("SKIP existing scratch path; preserve it", 0);
        return 0;
    }
    /* Firmware uses -2 for ENOENT. Other errors must not authorize creation. */
    if (fd != -2) {
        record("SKIP absence not established", 0);
        return 0;
    }
    return logging_ok;
}
#endif
#if PROBE_MODE == 0 || PROBE_MODE == 1
static int make_work(void)
{
    int fd;
    if (!vacant(WORK) || !before("BEFORE create scratch")) return -1;
    fd = sys_open(WORK, X7_O_RDWR | X7_O_CREAT | X7_O_TRUNC, 0600);
    record("create scratch fd", (x7_u64)(x7_s64)fd);
    if (fd < 0) return -1;
    if (!before("BEFORE write 32 bytes")) { sys_close(fd); return -1; }
    result("write", sys_write(fd, payload, 32), 32);
    return fd;
}

#endif
#if PROBE_MODE == 0
static void run_probe(void)
{
    char readback[32];
    int fd, n;
    unsigned int i;
    x7_off_t position;
    if (!vacant(RENAMED)) return;
    fd = make_work();
    if (fd < 0) return;
    if (!before("BEFORE tell after write")) goto close_work;
    result("tell after write", sys_tell(fd), 32);
    if (!before("BEFORE seek SET 4")) goto close_work;
    result("seek SET", sys_lseek(fd, 4, X7_SEEK_SET), 4);
    if (!before("BEFORE seek CUR -2")) goto close_work;
    result("seek CUR negative", sys_lseek(fd, -2, X7_SEEK_CUR), 2);
    if (!before("BEFORE seek END -4")) goto close_work;
    result("seek END negative", sys_lseek(fd, -4, X7_SEEK_END), 28);
    if (!before("BEFORE read last 4 bytes")) goto close_work;
    n = sys_read(fd, readback, 4);
    result("read length", n, 4);
    if (n == 4) {
        for (i = 0; i < 4 && readback[i] == payload[28 + i]; ++i) {}
        result("read content match", i == 4, 1);
    }
    if (!before("BEFORE invalid-fd 64-bit tell")) goto close_work;
    result("invalid-fd tell sign extension", sys_tell(-1), -9);
    if (!before("BEFORE scratch fsync")) goto close_work;
    result("scratch fsync", sys_fsync(fd), 0);
    if (!before("BEFORE ftruncate 8")) goto close_work;
    result("ftruncate status", sys_ftruncate(fd, 8), 0);
    if (!before("BEFORE seek END after ftruncate")) goto close_work;
    position = sys_lseek(fd, 0, X7_SEEK_END);
    result("size after ftruncate", position, 8);
close_work:
    if (!before("BEFORE scratch close")) { sys_close(fd); return; }
    result("scratch close", sys_close(fd), 0);
    if (!before("BEFORE truncate path 4")) return;
    result("truncate status", sys_truncate(WORK, 4), 0);
    if (!before("BEFORE reopen scratch read-only")) return;
    fd = sys_open(WORK, X7_O_RDONLY, 0);
    record("reopen fd", (x7_u64)(x7_s64)fd);
    if (fd >= 0) {
        if (before("BEFORE seek END after truncate"))
            result("size after truncate", sys_lseek(fd, 0, X7_SEEK_END), 4);
        sys_close(fd);
    } else ++failures;
    if (!before("BEFORE rename scratch")) return;
    n = sys_rename(WORK, RENAMED);
    result("rename", n, 0);
    if (!before("BEFORE remove owned scratch")) return;
    result("remove", sys_remove(n == 0 ? RENAMED : WORK), 0);
}
#endif

#if PROBE_MODE == 1 || PROBE_MODE == 2
/* Bounds are provisional; guards detect corruption, not prevent it. */
static struct {
    x7_u64 leading[8];
    x7_u64 aligned_data[512]; /* 4096 bytes */
    x7_u64 trailing[8];
} output;
static void prepare_output(void)
{
    unsigned int i;
    for (i = 0; i < 8; ++i) output.leading[i] = output.trailing[i] = 0x5aa55aa5a55aa55aULL;
    for (i = 0; i < 512; ++i) output.aligned_data[i] = 0xa5a5a5a5a5a5a5a5ULL;
}
static int save_output(const char *path)
{
    unsigned int i, changed = 0, last = 0;
    int fd, ok = 1;
    unsigned char *bytes = (unsigned char *)output.aligned_data;
    for (i = 0; i < 8; ++i)
        if (output.leading[i] != 0x5aa55aa5a55aa55aULL ||
            output.trailing[i] != 0x5aa55aa5a55aa55aULL) ok = 0;
    record(ok ? "GUARDS OK" : "GUARDS CORRUPTED; stop", 0);
    for (i = 0; i < sizeof(output.aligned_data); ++i)
        if (bytes[i] != 0xa5) { ++changed; last = i + 1; }
    record("changed byte count (sentinel comparison)", changed);
    record("last changed byte plus one", last);
    /* Diagnostic files, like logs, are deliberately replaced on a repeated run. */
    fd = sys_open(path, X7_O_WRONLY | X7_O_CREAT | X7_O_TRUNC, 0600);
    if (fd >= 0) {
        record("dump write success", write_all(fd, bytes, sizeof(output.aligned_data)));
        record("dump fsync", (x7_u64)(x7_s64)sys_fsync(fd));
        record("dump close", (x7_u64)(x7_s64)sys_close(fd));
    } else { record("dump open failed", (x7_u64)(x7_s64)fd); ++failures; }
    if (!ok) ++failures;
    return ok && logging_ok;
}
#endif

#if PROBE_MODE == 1
static void run_probe(void)
{
    int fd = make_work();
    if (fd < 0) return;
    prepare_output();
    if (!before("BEFORE fstat guarded 4096-byte output")) goto out;
    record("fstat status", (x7_u64)(x7_s64)sys_fstat(fd, (struct x7_stat *)output.aligned_data));
    if (!save_output(CARD "sysprobe-fstat.bin")) goto out;
    prepare_output();
    if (!before("BEFORE stat guarded 4096-byte output")) goto out;
    record("stat status", (x7_u64)(x7_s64)sys_stat(WORK, (struct x7_stat *)output.aligned_data));
    if (!save_output(CARD "sysprobe-stat.bin")) goto out;
    prepare_output();
    if (!before("BEFORE statfs guarded 4096-byte output")) goto out;
    record("statfs status", (x7_u64)(x7_s64)sys_statfs(CARD, (struct x7_statfs *)output.aligned_data));
    save_output(CARD "sysprobe-statfs.bin");
out:
    sys_close(fd);
    if (before("BEFORE remove owned scratch")) result("remove", sys_remove(WORK), 0);
}
#endif

#if PROBE_MODE == 2
static void run_probe(void)
{
    int fd = -1, child, n;
    unsigned int attempt;
    x7_off_t position = 0;
    static const char *const dumps[] = {
        CARD "sysprobe-readdir.bin", CARD "sysprobe-readdir-1.bin",
        CARD "sysprobe-readdir-2.bin", CARD "sysprobe-readdir-3.bin"
    };
    if (!vacant(DIR) || !before("BEFORE mkdir scratch directory")) return;
    n = sys_mkdir(DIR, 0700);
    result("mkdir", n, 0);
    if (n != 0) return;
    if (!before("BEFORE create directory entry")) goto cleanup;
    child = sys_open(CHILD, X7_O_WRONLY | X7_O_CREAT | X7_O_TRUNC, 0600);
    record("child fd", (x7_u64)(x7_s64)child);
    if (child < 0) goto cleanup;
    result("child write", sys_write(child, payload, 32), 32);
    sys_close(child);
    if (!before("BEFORE open directory with 0x10000")) goto cleanup;
    fd = sys_open(DIR, X7_O_RDONLY | X7_O_DIRECTORY, 0);
    record("directory fd", (x7_u64)(x7_s64)fd);
    if (fd < 0) goto cleanup;
    if (!before("BEFORE tell directory")) goto cleanup;
    record("directory tell", (x7_u64)sys_tell(fd));
    for (attempt = 0; attempt < 4 && logging_ok; ++attempt) {
        struct x7_dirent *entry = (struct x7_dirent *)output.aligned_data;
        unsigned int i;
        prepare_output();
        record("directory iteration", attempt);
        record("requested directory position", (x7_u64)position);
        if (!before("BEFORE readdir capacity 1024")) goto cleanup;
        n = sys_readdir(fd, position, output.aligned_data, 1024);
        record("readdir record count", (x7_u64)(x7_s64)n);
        if (!save_output(dumps[attempt])) goto cleanup;
        if (n == 0) break;
        /* Only interpret the single-record case observed so far. */
        if (n != 1) { record("STOP unexpected record count", 0); break; }
        record("record bytes", entry->record_bytes);
        record("next position", entry->next_position);
        record("attributes", entry->attributes);
        if (entry->record_bytes < 17 || entry->record_bytes > 1024 ||
            (entry->record_bytes & 3)) {
            record("STOP invalid record length", 0); ++failures; break;
        }
        for (i = 16; i < entry->record_bytes &&
             ((const char *)entry)[i]; ++i) {}
        if (i == entry->record_bytes) {
            record("STOP missing name terminator", 0); ++failures; break;
        }
        if ((x7_off_t)entry->next_position <= position) {
            record("STOP nonadvancing cursor", 0); break;
        }
        position = entry->next_position;
    }
    if (!before("BEFORE seekdir position 0")) goto cleanup;
    record("seekdir status", (x7_u64)(x7_s64)sys_seekdir(fd, 0));
    if (!before("BEFORE rewinddir")) goto cleanup;
    record("rewinddir status", (x7_u64)(x7_s64)sys_rewinddir(fd));
cleanup:
    if (fd >= 0) sys_close(fd);
    if (before("BEFORE remove owned directory entry")) record("remove child", (x7_u64)(x7_s64)sys_remove(CHILD));
    if (before("BEFORE rmdir owned directory")) result("rmdir", sys_rmdir(DIR), 0);
}
#endif

#if PROBE_MODE == 3
static void run_probe(void)
{
    int fd, n;
    unsigned int i;
    if (!before("BEFORE open /dev/wd")) return;
    fd = sys_open("/dev/wd", X7_O_RDWR, 0);
    record("watchdog fd", (x7_u64)(x7_s64)fd);
    if (fd < 0) return;
    /* Driver command zero stores this numeric millisecond budget and resets
     * this descriptor's counter. It expects a value, NOT a pointer. */
    if (!before("BEFORE ioctl watchdog command 0 budget 2000 ms")) goto out;
    n = sys_ioctl(fd, 0, 2000);
    result("watchdog arm", n, 0);
    if (n != 0) goto out;
    for (i = 0; i < 4 && logging_ok; ++i) {
        if (!before("BEFORE refresh own watchdog 2000 ms")) break;
        n = sys_ioctl(fd, 0, 2000);
        result("watchdog refresh", n, 0);
        if (n != 0) break;
        /* One tick, not a stress test. Do not try a timeout/reboot experiment. */
        if (!before("BEFORE one-tick watchdog delay")) break;
        OSTimeDly(1);
    }
out:
    /* Close immediately even if logging failed, releasing our registration. */
    n = sys_close(fd);
    result("watchdog close", n, 0);
}
#endif

void __attribute__((section(".init"))) _init(void)
{
    log_fd = sys_open(log_paths[PROBE_MODE], X7_O_WRONLY | X7_O_CREAT | X7_O_TRUNC, 0600);
    if (log_fd < 0) return;
    if (record("X7 syscall probe v3; mode", PROBE_MODE) &&
        record("yield between checkpoints", PROBE_YIELD) &&
        before("BEFORE initial OSTimeGet")) {
        record("initial ticks", OSTimeGet());
        run_probe();
        if (logging_ok) {
            record("final ticks", OSTimeGet());
            record("assertion failures (observational calls excluded)", failures);
            record("DONE", PROBE_MODE);
        }
    }
    sys_close(log_fd);
    log_fd = -1;
}
void __attribute__((section(".fini"))) _fini(void) {}
static const char *G1_SO_VERSION = "R0.00";
static const char version_name[] __attribute__((section(".dlstr"))) = "G1_SO_VERSION";
static const struct {
    const void *value;
    const char *name;
} version_symbol __attribute__((section(".dlsym"), used)) = { &G1_SO_VERSION, version_name };
