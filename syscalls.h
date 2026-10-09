/* X7 / Actions firmware ABI recovered from the local SYSCFG.SYS.
 * See syscalls.md for evidence, confidence, and unresolved layouts.
 * Freestanding little-endian MIPS32 O32; this is not Linux's syscall ABI.
 */
#ifndef X7_SYSCALLS_H
#define X7_SYSCALLS_H

typedef unsigned char x7_u8;
typedef unsigned short x7_u16;
typedef unsigned int x7_u32;
typedef int x7_s32;
typedef unsigned long long x7_u64;
typedef long long x7_s64;
typedef x7_u32 x7_word_t;
typedef x7_u32 x7_size_t;
typedef x7_s32 x7_ssize_t;
/* Never substitute the toolchain libc's off_t (possibly only 32 bits). */
typedef x7_s64 x7_off_t;

struct x7_os_tcb;
struct x7_os_event;
struct x7_os_sem_data;
struct x7_os_flag_group;
/* SD/FAT stat layout: on-console dumps + FS.KO 0xc080cf98/0xc080d078.
 * Unknown fields stay unnamed semantically. Initialize to zero before use.
 * Padding at 28 and 60 is not written by these filesystem methods.
 * Other filesystem implementations have not been tested. */
struct x7_stat {
    x7_u32 unknown_00;
    x7_u32 unknown_04;
    x7_u32 st_mode;            /* 8: 0x8000 regular, 0x4000 directory */
    x7_u32 unknown_0c;
    x7_u32 unknown_10;
    x7_u32 unknown_14;
    x7_u32 unknown_18;
    x7_u32 padding_1c;
    x7_off_t st_size;          /* 32: little-endian low/high words */
    x7_u32 st_blksize;         /* 40: allocation unit; 32768 on tested card */
    x7_u32 st_blocks;          /* 44: allocation units, NOT POSIX 512-byte blocks */
    x7_u32 timestamp_30;       /* 48: epoch seconds; atime/mtime/ctime order TBD */
    x7_u32 timestamp_34;
    x7_u32 timestamp_38;
    x7_u32 padding_3c;
};
/* FS.KO 0xc080d170 writes exactly 13 words. Familiar names are interpretations;
 * type 8 is this firmware's filesystem code, not Linux's FAT magic. */
struct x7_statfs {
    x7_u32 f_type;
    x7_u32 f_bsize;
    x7_u32 f_frsize;
    x7_u32 f_blocks;
    x7_u32 f_bfree;
    x7_u32 f_bavail;
    x7_u32 unknown_18;
    x7_u32 unknown_1c;
    x7_u32 unknown_20;
    x7_u32 unknown_24;
    x7_u32 unknown_28;
    x7_u32 f_namemax;          /* 255 */
    x7_u32 unknown_30;         /* 4095 for type 8; not established as flags */
};
/* Variable-length SD/FAT directory record, NOT a libc struct dirent.
 * Step by record_bytes, never sizeof. Some bytes are left untouched.
 * next_position is a zero-extended 32-bit cursor passed as x7_off_t.
 * Console-verified for '.', '..', and the regular file 'entry.bin'.
 * With sufficient capacity, return 0 and untouched output indicate EOF on
 * the tested directory. Too-small capacity can also return 0. */
struct x7_dirent {
    x7_u32 unknown_00;
    x7_u32 next_position;
    x7_u32 timestamp;
    x7_u16 record_bytes;
    x7_u8 attributes;          /* 0x10 directory; 0x20 archive on entry.bin */
    x7_u8 unknown_0f;
    char name[];              /* offset 16, NUL-terminated, padded to 4 bytes */
};
struct x7_utimbuf;
struct x7_file_operations;
struct x7_block_operations;
struct x7_filesystem_type;
struct x7_gendisk;
struct x7_request_queue;
struct x7_request;
struct x7_driver;
struct x7_file;
struct x7_dentry;
struct x7_timer;
struct x7_work;
struct x7_tasklet;
struct x7_module;
struct x7_inode;
struct x7_dma_channel;

typedef void (*x7_task_fn)(void *argument);
/* Callback signatures are inferred; verify before registering one. */
typedef int (*x7_irq_fn)(int irq, void *device, void *registers);
typedef void (*x7_dma_fn)(int channel, void *argument);
typedef void (*x7_request_fn)(struct x7_request_queue *queue);
typedef void (*x7_tasklet_fn)(x7_word_t argument);

#define SYSCALL_API_START 0x10000
#define KMODULE_API_START 0x20000
#define X7_SYSCALL_API_COUNT 130
#define X7_KMODULE_API_COUNT 113

#define X7_O_RDONLY 0
#define X7_O_WRONLY 1
#define X7_O_RDWR 2
#define X7_O_CREAT 0x200
#define X7_O_TRUNC 0x400
#define X7_O_DIRECTORY 0x10000 /* Verified directory-open flag */
#define X7_DIRENT_ATTR_DIRECTORY 0x10u
#define X7_DIRENT_ATTR_ARCHIVE 0x20u
#define X7_SEEK_SET 0
#define X7_SEEK_CUR 1
#define X7_SEEK_END 2
#define X7_GFP_KERNEL 0xd0u

#ifdef __cplusplus
extern "C" {
#endif
/* E: machine-code ABI evidence; I: inference; R: raw words, unknown ABI.
 * R calls pass a0..a3 and stack+16..28 verbatim. Split/pad 64-bit
 * arguments yourself. Only v0 is exposed; a 64-bit result needs a new
 * verified prototype. Reserved no-op slots have void results.
 * E establishes machine-level placement/width, not all parameter semantics.
 * Opaque structures intentionally have no invented layout.
 */
/* SYSCALL[0], 0xc0020bb0, CP0_vEnableIM; I. */
#define X7_API_CP0_vEnableIM (SYSCALL_API_START + 0)
void CP0_vEnableIM(x7_u32 mask);

/* SYSCALL[1], 0xc0012b30, OS_Sched; I. */
#define X7_API_OS_Sched (SYSCALL_API_START + 1)
void OS_Sched(void);

/* SYSCALL[2], 0xc00127c8, OSSchedLock; I. */
#define X7_API_OSSchedLock (SYSCALL_API_START + 2)
void OSSchedLock(void);

/* SYSCALL[3], 0xc0012c30, OSSchedUnlock; I. */
#define X7_API_OSSchedUnlock (SYSCALL_API_START + 3)
void OSSchedUnlock(void);

/* SYSCALL[4], 0xc00158e8, OSTaskCreate; I. */
#define X7_API_OSTaskCreate (SYSCALL_API_START + 4)
x7_u8 OSTaskCreate(x7_task_fn task, void *arg, x7_u32 *stack_top, x7_u8 priority);

/* SYSCALL[5], 0xc0015a68, -; R. */
#define X7_API_x7_syscall_005 (SYSCALL_API_START + 5)
x7_word_t x7_syscall_005(x7_word_t word0, x7_word_t word1, x7_word_t word2, x7_word_t word3, x7_word_t word4, x7_word_t word5, x7_word_t word6, x7_word_t word7);

/* SYSCALL[6], 0xc0015e64, OSTaskDel; I. */
#define X7_API_OSTaskDel (SYSCALL_API_START + 6)
x7_u8 OSTaskDel(x7_u8 priority);

/* SYSCALL[7], 0xc0016088, -; E. */
#define X7_API_x7_syscall_007 (SYSCALL_API_START + 7)
x7_u8 x7_syscall_007(x7_u8 priority, x7_u32 task_id);

/* SYSCALL[8], 0xc00161f4, OSTaskDelReq; I. */
#define X7_API_OSTaskDelReq (SYSCALL_API_START + 8)
x7_u8 OSTaskDelReq(x7_u8 priority);

/* SYSCALL[9], 0xc00154f8, OSTaskChangePrio; I. */
#define X7_API_OSTaskChangePrio (SYSCALL_API_START + 9)
x7_u8 OSTaskChangePrio(x7_u8 old_priority, x7_u8 new_priority);

/* SYSCALL[10], 0xc00162a8, OSTaskResume; I. */
#define X7_API_OSTaskResume (SYSCALL_API_START + 10)
x7_u8 OSTaskResume(x7_u8 priority);

/* SYSCALL[11], 0xc00164a8, OSTaskSuspend; I. */
#define X7_API_OSTaskSuspend (SYSCALL_API_START + 11)
x7_u8 OSTaskSuspend(x7_u8 priority);

/* SYSCALL[12], 0xc0015778, OSTaskCreateExt; I. */
#define X7_API_OSTaskCreateExt (SYSCALL_API_START + 12)
x7_u8 OSTaskCreateExt(x7_task_fn task, void *arg, x7_u32 *stack_top, x7_u8 priority, x7_u16 id, x7_u32 *stack_bottom, x7_u32 stack_words, void *extension, x7_u16 options);

/* SYSCALL[13], 0xc00165d8, OSTaskQuery; I. */
#define X7_API_OSTaskQuery (SYSCALL_API_START + 13)
x7_u8 OSTaskQuery(x7_u8 priority, struct x7_os_tcb *result);

/* SYSCALL[14], 0xc0012e3c, OS_Getidfromprio; E. */
#define X7_API_OS_Getidfromprio (SYSCALL_API_START + 14)
x7_u16 OS_Getidfromprio(x7_u8 priority);

/* SYSCALL[15], 0xc0012e68, OS_GetTaskID; E. */
#define X7_API_OS_GetTaskID (SYSCALL_API_START + 15)
x7_u8 OS_GetTaskID(void);

/* SYSCALL[16], 0xc00168c0, OSTimeDly; I. */
#define X7_API_OSTimeDly (SYSCALL_API_START + 16)
void OSTimeDly(x7_u16 ticks);

/* SYSCALL[17], 0xc0016960, OSTimeDlyResume; I. */
#define X7_API_OSTimeDlyResume (SYSCALL_API_START + 17)
x7_u8 OSTimeDlyResume(x7_u8 priority);

/* SYSCALL[18], 0xc0016a44, OSTimeGet; I. */
#define X7_API_OSTimeGet (SYSCALL_API_START + 18)
x7_u32 OSTimeGet(void);

/* SYSCALL[19], 0xc0016a78, OSTimeSet; I. */
#define X7_API_OSTimeSet (SYSCALL_API_START + 19)
void OSTimeSet(x7_u32 ticks);

/* SYSCALL[20], 0xc0013a5c, OSQAccept; I. */
#define X7_API_OSQAccept (SYSCALL_API_START + 20)
void * OSQAccept(struct x7_os_event *queue);

/* SYSCALL[21], 0xc0013ba4, OSQCreate; I. */
#define X7_API_OSQCreate (SYSCALL_API_START + 21)
struct x7_os_event * OSQCreate(void **storage, x7_u16 entries);

/* SYSCALL[22], 0xc0013cf8, OSQDel; I. */
#define X7_API_OSQDel (SYSCALL_API_START + 22)
struct x7_os_event * OSQDel(struct x7_os_event *queue, x7_u8 option, x7_u8 *error);

/* SYSCALL[23], 0xc0013ee0, OSQFlush; I. */
#define X7_API_OSQFlush (SYSCALL_API_START + 23)
x7_u8 OSQFlush(struct x7_os_event *queue);

/* SYSCALL[24], 0xc0013fa8, OSQPend; I. */
#define X7_API_OSQPend (SYSCALL_API_START + 24)
void * OSQPend(struct x7_os_event *queue, x7_u16 timeout, x7_u8 *error);

/* SYSCALL[25], 0xc00141b8, OSQPost; I. */
#define X7_API_OSQPost (SYSCALL_API_START + 25)
x7_u8 OSQPost(struct x7_os_event *queue, void *message);

/* SYSCALL[26], 0xc00142ec, OSQPostByPrio; E. */
#define X7_API_OSQPostByPrio (SYSCALL_API_START + 26)
x7_u8 OSQPostByPrio(struct x7_os_event *queue, void *message, x7_u8 priority, x7_u16 option);

/* SYSCALL[27], 0xc0014568, OSQPostISR; I. */
#define X7_API_OSQPostISR (SYSCALL_API_START + 27)
x7_u8 OSQPostISR(struct x7_os_event *queue, void *message);

/* SYSCALL[28], 0xc0014cec, OSSemAccept; I. */
#define X7_API_OSSemAccept (SYSCALL_API_START + 28)
x7_u16 OSSemAccept(struct x7_os_event *semaphore);

/* SYSCALL[29], 0xc0014dc8, OSSemCreate; I. */
#define X7_API_OSSemCreate (SYSCALL_API_START + 29)
struct x7_os_event * OSSemCreate(x7_u16 count);

/* SYSCALL[30], 0xc0014ed4, OSSemDel; I. */
#define X7_API_OSSemDel (SYSCALL_API_START + 30)
struct x7_os_event * OSSemDel(struct x7_os_event *semaphore, x7_u8 option, x7_u8 *error);

/* SYSCALL[31], 0xc00150ec, OSSemPend; I. */
#define X7_API_OSSemPend (SYSCALL_API_START + 31)
void OSSemPend(struct x7_os_event *semaphore, x7_u16 timeout, x7_u8 *error);

/* SYSCALL[32], 0xc001532c, OSSemPost; I. */
#define X7_API_OSSemPost (SYSCALL_API_START + 32)
x7_u8 OSSemPost(struct x7_os_event *semaphore);

/* SYSCALL[33], 0xc001544c, OSSemQuery; I. */
#define X7_API_OSSemQuery (SYSCALL_API_START + 33)
x7_u8 OSSemQuery(struct x7_os_event *semaphore, struct x7_os_sem_data *result);

/* SYSCALL[34], 0xc0012f94, OSFlagAccept; I. */
#define X7_API_OSFlagAccept (SYSCALL_API_START + 34)
x7_u32 OSFlagAccept(struct x7_os_flag_group *group, x7_u32 flags, x7_u8 wait_type, x7_u8 *error);

/* SYSCALL[35], 0xc0013154, OSFlagCreate; I. */
#define X7_API_OSFlagCreate (SYSCALL_API_START + 35)
struct x7_os_flag_group * OSFlagCreate(x7_u32 flags, x7_u8 *error);

/* SYSCALL[36], 0xc0013378, OSFlagDel; I. */
#define X7_API_OSFlagDel (SYSCALL_API_START + 36)
struct x7_os_flag_group * OSFlagDel(struct x7_os_flag_group *group, x7_u8 option, x7_u8 *error);

/* SYSCALL[37], 0xc00134ec, OSFlagPend; I. */
#define X7_API_OSFlagPend (SYSCALL_API_START + 37)
x7_u32 OSFlagPend(struct x7_os_flag_group *group, x7_u32 flags, x7_u8 wait_type, x7_u16 timeout, x7_u8 *error);

/* SYSCALL[38], 0xc00137d8, OSFlagPost; I. */
#define X7_API_OSFlagPost (SYSCALL_API_START + 38)
x7_u32 OSFlagPost(struct x7_os_flag_group *group, x7_u32 flags, x7_u8 option, x7_u8 *error);

/* SYSCALL[39], 0xc00139d8, OSFlagQuery; I. */
#define X7_API_OSFlagQuery (SYSCALL_API_START + 39)
x7_u32 OSFlagQuery(struct x7_os_flag_group *group, x7_u8 *error);

/* SYSCALL[40], 0xc0006f94, api_install; E. */
#define X7_API_api_install (SYSCALL_API_START + 40)
int api_install(x7_u8 group, const void *dispatch_table);

/* SYSCALL[41], 0xc0007014, api_uninstall; E. */
#define X7_API_api_uninstall (SYSCALL_API_START + 41)
int api_uninstall(x7_u8 group);

/* SYSCALL[42], 0xc0011e40, -; E. */
#define X7_API_x7_syscall_042 (SYSCALL_API_START + 42)
void x7_syscall_042(void);

/* SYSCALL[43], 0xc0011e40, -; E. */
#define X7_API_x7_syscall_043 (SYSCALL_API_START + 43)
void x7_syscall_043(void);

/* SYSCALL[44], 0xc0011e40, -; E. */
#define X7_API_x7_syscall_044 (SYSCALL_API_START + 44)
void x7_syscall_044(void);

/* SYSCALL[45], 0xc0011e40, -; E. */
#define X7_API_x7_syscall_045 (SYSCALL_API_START + 45)
void x7_syscall_045(void);

/* SYSCALL[46], 0xc0011e40, -; E. */
#define X7_API_x7_syscall_046 (SYSCALL_API_START + 46)
void x7_syscall_046(void);

/* SYSCALL[47], 0xc0011e40, -; E. */
#define X7_API_x7_syscall_047 (SYSCALL_API_START + 47)
void x7_syscall_047(void);

/* SYSCALL[48], 0xc0011e40, -; E. */
#define X7_API_x7_syscall_048 (SYSCALL_API_START + 48)
void x7_syscall_048(void);

/* SYSCALL[49], 0xc0011e40, -; E. */
#define X7_API_x7_syscall_049 (SYSCALL_API_START + 49)
void x7_syscall_049(void);

/* SYSCALL[50], 0xc0011000, printf; I. */
#define X7_API_x7_printf (SYSCALL_API_START + 50)
int x7_printf(const char *format, ...);

/* SYSCALL[51], 0xc000fe78, serial_getc; I. */
#define X7_API_serial_getc (SYSCALL_API_START + 51)
int serial_getc(void);

/* SYSCALL[52], 0xc000fc8c, reset_baudrate; E. */
#define X7_API_reset_baudrate (SYSCALL_API_START + 52)
int reset_baudrate(void);

/* SYSCALL[53], 0xc0003958, mdelay; I. */
#define X7_API_mdelay (SYSCALL_API_START + 53)
void mdelay(x7_u32 milliseconds);

/* SYSCALL[54], 0xc00039bc, udelay; I. */
#define X7_API_udelay (SYSCALL_API_START + 54)
void udelay(x7_u32 microseconds);

/* SYSCALL[55], 0xc000f348, rand; I. */
#define X7_API_x7_rand (SYSCALL_API_START + 55)
int x7_rand(void);

/* SYSCALL[56], 0xc0006440, get_count; I. */
#define X7_API_get_count (SYSCALL_API_START + 56)
x7_u32 get_count(void);

/* SYSCALL[57], 0xc00064a4, get_c0_count; I. */
#define X7_API_get_c0_count (SYSCALL_API_START + 57)
x7_u32 get_c0_count(void);

/* SYSCALL[58], 0xc000c7b4, malloc; I. */
#define X7_API_x7_malloc (SYSCALL_API_START + 58)
void * x7_malloc(x7_size_t bytes);

/* SYSCALL[59], 0xc000bf60, free; I. */
#define X7_API_x7_free (SYSCALL_API_START + 59)
void x7_free(void *pointer);

/* SYSCALL[60], 0xc001f270, kmalloc; E. */
#define X7_API_x7_sys_kmalloc (SYSCALL_API_START + 60)
void * x7_sys_kmalloc(x7_size_t bytes, x7_u32 flags);

/* SYSCALL[61], 0xc001f288, kfree; E. */
#define X7_API_x7_sys_kfree (SYSCALL_API_START + 61)
void x7_sys_kfree(const void *pointer);

/* SYSCALL[62], 0xc000c314, realloc; I. */
#define X7_API_x7_realloc (SYSCALL_API_START + 62)
void * x7_realloc(void *pointer, x7_size_t bytes);

/* SYSCALL[63], 0xc000e45c, os_mem_query; I. */
#define X7_API_os_mem_query (SYSCALL_API_START + 63)
x7_u32 os_mem_query(void);

/* SYSCALL[64], 0xc000e46c, print_mem; I. */
#define X7_API_print_mem (SYSCALL_API_START + 64)
void print_mem(void);

/* SYSCALL[65], 0xc001c23c, sys_mount; E. */
#define X7_API_sys_mount (SYSCALL_API_START + 65)
int sys_mount(const char *source, const char *target, const char *filesystem, x7_u32 flags, const void *data);

/* SYSCALL[66], 0xc001bd24, sys_umount; I. */
#define X7_API_sys_umount (SYSCALL_API_START + 66)
int sys_umount(const char *target);

/* SYSCALL[67], 0xc001afa4, sys_mknod; I. */
#define X7_API_sys_mknod (SYSCALL_API_START + 67)
int sys_mknod(const char *path, x7_u32 mode, x7_u32 device);

/* SYSCALL[68], 0xc001c848, sys_open; E. */
#define X7_API_sys_open (SYSCALL_API_START + 68)
int sys_open(const char *path, int flags, x7_u32 mode);

/* SYSCALL[69], 0xc001ca2c, sys_close; E. */
#define X7_API_sys_close (SYSCALL_API_START + 69)
int sys_close(int fd);

/* SYSCALL[70], 0xc001d1b0, sys_read; E. */
#define X7_API_sys_read (SYSCALL_API_START + 70)
x7_ssize_t sys_read(int fd, void *buffer, x7_size_t count);

/* SYSCALL[71], 0xc001d230, sys_write; E. */
#define X7_API_sys_write (SYSCALL_API_START + 71)
x7_ssize_t sys_write(int fd, const void *buffer, x7_size_t count);

/* SYSCALL[72], 0xc001cd34, sys_lseek; E. */
#define X7_API_sys_lseek (SYSCALL_API_START + 72)
x7_off_t sys_lseek(int fd, x7_off_t offset, int whence);

/* SYSCALL[73], 0xc0019724, sys_fcntl; I. */
#define X7_API_sys_fcntl (SYSCALL_API_START + 73)
int sys_fcntl(int fd, int command, x7_word_t argument);

/* SYSCALL[74], 0xc001c850, sys_creat; I. */
#define X7_API_sys_creat (SYSCALL_API_START + 74)
int sys_creat(const char *path, x7_u32 mode);

/* SYSCALL[75], 0xc001b2b8, sys_rename; I. */
#define X7_API_sys_rename (SYSCALL_API_START + 75)
int sys_rename(const char *old_path, const char *new_path);

/* SYSCALL[76], 0xc001b988, sys_chdir; I. */
#define X7_API_sys_chdir (SYSCALL_API_START + 76)
int sys_chdir(const char *path);

/* SYSCALL[77], 0xc00180dc, sys_getcwd; E. */
#define X7_API_sys_getcwd (SYSCALL_API_START + 77)
int sys_getcwd(char *buffer, x7_size_t size);

/* SYSCALL[78], 0xc001ae18, sys_mkdir; I. */
#define X7_API_sys_mkdir (SYSCALL_API_START + 78)
int sys_mkdir(const char *path, x7_u32 mode);

/* SYSCALL[79], 0xc001b43c, sys_rmdir; I. */
#define X7_API_sys_rmdir (SYSCALL_API_START + 79)
int sys_rmdir(const char *path);

/* SYSCALL[80], 0xc001f308, sys_fstat; E. */
#define X7_API_sys_fstat (SYSCALL_API_START + 80)
int sys_fstat(int fd, struct x7_stat *result);

/* SYSCALL[81], 0xc001f448, sys_stat; I. */
#define X7_API_sys_stat (SYSCALL_API_START + 81)
int sys_stat(const char *path, struct x7_stat *result);

/* SYSCALL[82], 0xc001f558, sys_statfs; I. */
#define X7_API_sys_statfs (SYSCALL_API_START + 82)
int sys_statfs(const char *path, struct x7_statfs *result);

/* SYSCALL[83], 0xc001c4a0, sys_utime; I. */
#define X7_API_sys_utime (SYSCALL_API_START + 83)
int sys_utime(const char *path, const struct x7_utimbuf *times);

/* SYSCALL[84], 0xc001c278, sys_truncate; E. */
#define X7_API_sys_truncate (SYSCALL_API_START + 84)
int sys_truncate(const char *path, x7_off_t length);

/* SYSCALL[85], 0xc001c398, sys_ftruncate; E. */
#define X7_API_sys_ftruncate (SYSCALL_API_START + 85)
int sys_ftruncate(int fd, x7_off_t length);

/* SYSCALL[86], 0xc0017a44, sys_fsync; E. */
#define X7_API_sys_fsync (SYSCALL_API_START + 86)
int sys_fsync(int fd);

/* SYSCALL[87], 0xc001ba80, sys_fchdir; I. */
#define X7_API_sys_fchdir (SYSCALL_API_START + 87)
int sys_fchdir(int fd);

/* SYSCALL[88], 0xc001bb30, sys_flock; I. */
#define X7_API_sys_flock (SYSCALL_API_START + 88)
int sys_flock(int fd, int operation);

/* SYSCALL[89], 0xc001ca78, sys_remove; I. */
#define X7_API_sys_remove (SYSCALL_API_START + 89)
int sys_remove(const char *path);

/* SYSCALL[90], 0xc001ccf0, sys_tell; E. */
#define X7_API_sys_tell (SYSCALL_API_START + 90)
x7_off_t sys_tell(int fd);

/* SYSCALL[91], 0xc001b5dc, sys_readdir; E. */
#define X7_API_sys_readdir (SYSCALL_API_START + 91)
/* capacity_bytes is buffer space; return value counts records, not bytes. */
int sys_readdir(int fd, x7_off_t position, void *entries, x7_u32 capacity_bytes);

/* SYSCALL[92], 0xc001b694, sys_seekdir; E. */
#define X7_API_sys_seekdir (SYSCALL_API_START + 92)
int sys_seekdir(int fd, x7_off_t position);

/* SYSCALL[93], 0xc001b738, sys_rewinddir; E. */
#define X7_API_sys_rewinddir (SYSCALL_API_START + 93)
int sys_rewinddir(int fd);

/* SYSCALL[94], 0xc001b7c0, sys_lastdir; E. */
#define X7_API_sys_lastdir (SYSCALL_API_START + 94)
int sys_lastdir(int fd);

/* SYSCALL[95], 0xc001b848, sys_prevdir; E. */
#define X7_API_sys_prevdir (SYSCALL_API_START + 95)
int sys_prevdir(int fd, x7_off_t position, void *entries, x7_u32 count);

/* SYSCALL[96], 0xc001b900, sys_reset2parentdir; E. */
#define X7_API_sys_reset2parentdir (SYSCALL_API_START + 96)
int sys_reset2parentdir(int fd);

/* SYSCALL[97], 0xc001a44c, sys_ioctl; I. */
#define X7_API_sys_ioctl (SYSCALL_API_START + 97)
int sys_ioctl(int fd, x7_u32 command, x7_word_t argument);

/* SYSCALL[98], 0xc001a658, sys_mmap; E. */
#define X7_API_sys_mmap (SYSCALL_API_START + 98)
void * sys_mmap(void *address, x7_size_t length, int protection, int flags, int fd, x7_off_t offset);

/* SYSCALL[99], 0xc001a73c, sys_munmap; E. */
#define X7_API_sys_munmap (SYSCALL_API_START + 99)
int sys_munmap(void *address, x7_size_t length);

/* SYSCALL[100], 0xc001ab2c, sys_msync; E. */
#define X7_API_sys_msync (SYSCALL_API_START + 100)
int sys_msync(void *address, x7_size_t length, int flags);

/* SYSCALL[101], 0xc0019bbc, fork; E. */
#define X7_API_x7_fork (SYSCALL_API_START + 101)
int x7_fork(void);

/* SYSCALL[102], 0xc0019634, _execve; E. */
#define X7_API_x7_execve (SYSCALL_API_START + 102)
int x7_execve(const char *path, x7_word_t argument1, x7_word_t argument2);

/* SYSCALL[103], 0xc0019494, _exit; I. */
#define X7_API_x7_exit (SYSCALL_API_START + 103)
int x7_exit(x7_u32 task_id);

/* SYSCALL[104], 0xc001f0e8, sys_shm_open; E. */
#define X7_API_sys_shm_open (SYSCALL_API_START + 104)
int sys_shm_open(const char *name, int flags, x7_u32 mode);

/* SYSCALL[105], 0xc001f1bc, sys_shm_unlink; E. */
#define X7_API_sys_shm_unlink (SYSCALL_API_START + 105)
int sys_shm_unlink(const char *name);

/* SYSCALL[106], 0xc001aa10, insmod; E. */
#define X7_API_insmod (SYSCALL_API_START + 106)
int insmod(const char *path, void *module_information);

/* SYSCALL[107], 0xc001aa1c, rmmod; I. */
#define X7_API_rmmod (SYSCALL_API_START + 107)
int rmmod(const char *name);

/* SYSCALL[108], 0xc0018d00, dlopen; I. */
#define X7_API_x7_dlopen (SYSCALL_API_START + 108)
void * x7_dlopen(const char *path, int flags);

/* SYSCALL[109], 0xc0019308, dlclose; I. */
#define X7_API_x7_dlclose (SYSCALL_API_START + 109)
int x7_dlclose(void *handle);

/* SYSCALL[110], 0xc0019314, dlsym; I. */
#define X7_API_x7_dlsym (SYSCALL_API_START + 110)
void * x7_dlsym(void *handle, const char *name);

/* SYSCALL[111], 0xc001943c, dlerror; E. */
#define X7_API_x7_dlerror (SYSCALL_API_START + 111)
char * x7_dlerror(void);

/* SYSCALL[112], 0xc00033a8, -; R. */
#define X7_API_x7_syscall_112 (SYSCALL_API_START + 112)
x7_word_t x7_syscall_112(x7_word_t word0, x7_word_t word1, x7_word_t word2, x7_word_t word3, x7_word_t word4, x7_word_t word5, x7_word_t word6, x7_word_t word7);

/* SYSCALL[113], 0xc0011e48, -; E. */
#define X7_API_x7_syscall_113 (SYSCALL_API_START + 113)
void * x7_syscall_113(void);

/* SYSCALL[114], 0xc000c8f4, -; I. */
#define X7_API_x7_syscall_114 (SYSCALL_API_START + 114)
void * x7_syscall_114(x7_size_t bytes, x7_u32 flags);

/* SYSCALL[115], 0xc000cae4, -; I. */
#define X7_API_x7_syscall_115 (SYSCALL_API_START + 115)
int x7_syscall_115(void *pointer);

/* SYSCALL[116], 0xc000cbc0, -; E. */
#define X7_API_x7_syscall_116 (SYSCALL_API_START + 116)
x7_word_t x7_syscall_116(const void *pointer);

/* SYSCALL[117], 0xc000cc18, -; E. */
#define X7_API_x7_syscall_117 (SYSCALL_API_START + 117)
x7_u32 x7_syscall_117(void);

/* SYSCALL[118], 0xc00214e0, -; R. */
#define X7_API_x7_syscall_118 (SYSCALL_API_START + 118)
x7_word_t x7_syscall_118(x7_word_t word0, x7_word_t word1, x7_word_t word2, x7_word_t word3, x7_word_t word4, x7_word_t word5, x7_word_t word6, x7_word_t word7);

/* SYSCALL[119], 0xc0021518, -; R. */
#define X7_API_x7_syscall_119 (SYSCALL_API_START + 119)
x7_word_t x7_syscall_119(x7_word_t word0, x7_word_t word1, x7_word_t word2, x7_word_t word3, x7_word_t word4, x7_word_t word5, x7_word_t word6, x7_word_t word7);

/* SYSCALL[120], 0xc000b1e8, get_ic_version; E. */
#define X7_API_get_ic_version (SYSCALL_API_START + 120)
x7_u8 get_ic_version(void);

/* SYSCALL[121], 0xc000b1f4, get_ic_type; E. */
#define X7_API_get_ic_type (SYSCALL_API_START + 121)
x7_u8 get_ic_type(void);

/* SYSCALL[122], 0xc000b200, get_sdram_cap; E. */
#define X7_API_get_sdram_cap (SYSCALL_API_START + 122)
x7_u8 get_sdram_cap(void);

/* SYSCALL[123], 0xc000e37c, -; I. */
#define X7_API_x7_syscall_123 (SYSCALL_API_START + 123)
int x7_syscall_123(x7_word_t task_address, x7_word_t value);

/* SYSCALL[124], 0xc000e3c0, -; I. */
#define X7_API_x7_syscall_124 (SYSCALL_API_START + 124)
void x7_syscall_124(void);

/* SYSCALL[125], 0xc000a3b0, -; I. */
#define X7_API_x7_syscall_125 (SYSCALL_API_START + 125)
int x7_syscall_125(int operation, x7_word_t argument);

/* SYSCALL[126], 0xc0003514, -; I. */
#define X7_API_x7_syscall_126 (SYSCALL_API_START + 126)
int x7_syscall_126(const void *pointer, x7_size_t bytes);

/* SYSCALL[127], 0xc0010104, -; I. */
#define X7_API_x7_syscall_127 (SYSCALL_API_START + 127)
int x7_syscall_127(int character);

/* SYSCALL[128], 0xc000b240, -; E. */
#define X7_API_x7_syscall_128 (SYSCALL_API_START + 128)
x7_u32 x7_syscall_128(void);

/* SYSCALL[129], 0xc000b26c, -; I. */
#define X7_API_x7_syscall_129 (SYSCALL_API_START + 129)
int x7_syscall_129(x7_u32 selector);

/* KMODULE[0], 0xc0020b6c, os_cpu_save_sr; E. */
#define X7_API_os_cpu_save_sr (KMODULE_API_START + 0)
x7_u32 os_cpu_save_sr(void);

/* KMODULE[1], 0xc0020b78, os_cpu_store_sr; E. */
#define X7_API_os_cpu_store_sr (KMODULE_API_START + 1)
void os_cpu_store_sr(x7_u32 saved_status);

/* KMODULE[2], 0xc00211c0, memcpy; I. */
#define X7_API_x7_memcpy (KMODULE_API_START + 2)
void * x7_memcpy(void *destination, const void *source, x7_size_t bytes);

/* KMODULE[3], 0xc00213c0, memset; I. */
#define X7_API_x7_memset (KMODULE_API_START + 3)
void * x7_memset(void *destination, int value, x7_size_t bytes);

/* KMODULE[4], 0xc0011780, strcpy; I. */
#define X7_API_x7_strcpy (KMODULE_API_START + 4)
char * x7_strcpy(char *destination, const char *source);

/* KMODULE[5], 0xc00117a0, strncpy; I. */
#define X7_API_x7_strncpy (KMODULE_API_START + 5)
char * x7_strncpy(char *destination, const char *source, x7_size_t bytes);

/* KMODULE[6], 0xc0011874, strcat; I. */
#define X7_API_x7_strcat (KMODULE_API_START + 6)
char * x7_strcat(char *destination, const char *source);

/* KMODULE[7], 0xc00119bc, strcmp; I. */
#define X7_API_x7_strcmp (KMODULE_API_START + 7)
int x7_strcmp(const char *left, const char *right);

/* KMODULE[8], 0xc00119e8, strncmp; I. */
#define X7_API_x7_strncmp (KMODULE_API_START + 8)
int x7_strncmp(const char *left, const char *right, x7_size_t bytes);

/* KMODULE[9], 0xc0011a54, strrchr; I. */
#define X7_API_x7_strrchr (KMODULE_API_START + 9)
char * x7_strrchr(const char *string, int character);

/* KMODULE[10], 0xc0011d54, strstr; I. */
#define X7_API_x7_strstr (KMODULE_API_START + 10)
char * x7_strstr(const char *string, const char *substring);

/* KMODULE[11], 0xc00117cc, strlen; I. */
#define X7_API_x7_strlen (KMODULE_API_START + 11)
x7_size_t x7_strlen(const char *string);

/* KMODULE[12], 0xc000b000, simple_strtoul; I. */
#define X7_API_simple_strtoul (KMODULE_API_START + 12)
x7_u32 simple_strtoul(const char *string, char **end, unsigned int base);

/* KMODULE[13], 0xc000b184, simple_strtol; I. */
#define X7_API_simple_strtol (KMODULE_API_START + 13)
x7_s32 simple_strtol(const char *string, char **end, unsigned int base);

/* KMODULE[14], 0xc0011c94, memmove; I. */
#define X7_API_x7_memmove (KMODULE_API_START + 14)
void * x7_memmove(void *destination, const void *source, x7_size_t bytes);

/* KMODULE[15], 0xc0011d00, memcmp; I. */
#define X7_API_x7_memcmp (KMODULE_API_START + 15)
int x7_memcmp(const void *left, const void *right, x7_size_t bytes);

/* KMODULE[16], 0xc0011e08, memchr; I. */
#define X7_API_x7_memchr (KMODULE_API_START + 16)
void * x7_memchr(const void *buffer, int value, x7_size_t bytes);

/* KMODULE[17], 0xc000afac, strswab; I. */
#define X7_API_strswab (KMODULE_API_START + 17)
char * strswab(char *string);

/* KMODULE[18], 0xc00118a8, strncat; I. */
#define X7_API_x7_strncat (KMODULE_API_START + 18)
char * x7_strncat(char *destination, const char *source, x7_size_t bytes);

/* KMODULE[19], 0xc000af58, sprintf; I. */
#define X7_API_x7_sprintf (KMODULE_API_START + 19)
int x7_sprintf(char *destination, const char *format, ...);

/* KMODULE[20], 0xc0009c80, r4k_dma_cache_wback_inv; I. */
#define X7_API_r4k_dma_cache_wback_inv (KMODULE_API_START + 20)
void r4k_dma_cache_wback_inv(x7_u32 address, x7_size_t bytes);

/* KMODULE[21], 0xc0009c00, r4k_dma_cache_inv; I. */
#define X7_API_r4k_dma_cache_inv (KMODULE_API_START + 21)
void r4k_dma_cache_inv(x7_u32 address, x7_size_t bytes);

/* KMODULE[22], 0xc00099c4, r4k_flush_icache_range; I. */
#define X7_API_r4k_flush_icache_range (KMODULE_API_START + 22)
void r4k_flush_icache_range(x7_u32 start, x7_u32 end);

/* KMODULE[23], 0xc0005630, request_irq; I. */
#define X7_API_request_irq (KMODULE_API_START + 23)
int request_irq(unsigned int irq, x7_irq_fn handler, x7_u32 flags, const char *name, void *device);

/* KMODULE[24], 0xc00057f4, free_irq; I. */
#define X7_API_free_irq (KMODULE_API_START + 24)
void free_irq(unsigned int irq, void *device);

/* KMODULE[25], 0xc0003c34, request_act213x_dma; E. */
#define X7_API_request_act213x_dma (KMODULE_API_START + 25)
int request_act213x_dma(int kind, const char *name, x7_irq_fn handler, x7_u32 flags, void *device);

/* KMODULE[26], 0xc0004070, free_act213x_dma; I. */
#define X7_API_free_act213x_dma (KMODULE_API_START + 26)
void free_act213x_dma(int channel);

/* KMODULE[27], 0xc0003ba0, get_dma_chan; E. */
#define X7_API_get_dma_chan (KMODULE_API_START + 27)
struct x7_dma_channel * get_dma_chan(int channel);

/* KMODULE[28], 0xc000b58c, -; E. */
#define X7_API_x7_kmodule_028 (KMODULE_API_START + 28)
void x7_kmodule_028(void);

/* KMODULE[29], 0xc000b58c, -; E. */
#define X7_API_x7_kmodule_029 (KMODULE_API_START + 29)
void x7_kmodule_029(void);

/* KMODULE[30], 0xc000b58c, -; E. */
#define X7_API_x7_kmodule_030 (KMODULE_API_START + 30)
void x7_kmodule_030(void);

/* KMODULE[31], 0xc000b58c, -; E. */
#define X7_API_x7_kmodule_031 (KMODULE_API_START + 31)
void x7_kmodule_031(void);

/* KMODULE[32], 0xc0012500, get_sysmsg; E. */
#define X7_API_get_sysmsg (KMODULE_API_START + 32)
int get_sysmsg(void);

/* KMODULE[33], 0xc0012490, put_sysmsg; E. */
#define X7_API_put_sysmsg (KMODULE_API_START + 33)
int put_sysmsg(x7_word_t message);

/* KMODULE[34], 0xc0012454, flush_sysmsg; I. */
#define X7_API_flush_sysmsg (KMODULE_API_START + 34)
void flush_sysmsg(void);

/* KMODULE[35], 0xc001f270, kmalloc; E. */
#define X7_API_kmalloc (KMODULE_API_START + 35)
void * kmalloc(x7_size_t bytes, x7_u32 flags);

/* KMODULE[36], 0xc001f288, kfree; E. */
#define X7_API_kfree (KMODULE_API_START + 36)
void kfree(const void *pointer);

/* KMODULE[37], 0xc0017b58, register_chrdev; I. */
#define X7_API_register_chrdev (KMODULE_API_START + 37)
int register_chrdev(unsigned int major, const char *name, const struct x7_file_operations *operations);

/* KMODULE[38], 0xc0017cdc, unregister_chrdev; I. */
#define X7_API_unregister_chrdev (KMODULE_API_START + 38)
int unregister_chrdev(unsigned int major, const char *name);

/* KMODULE[39], 0xc0019bc4, register_blkdev; E. */
#define X7_API_register_blkdev (KMODULE_API_START + 39)
int register_blkdev(unsigned int major, const char *name);

/* KMODULE[40], 0xc0019dc4, unregister_blkdev; I. */
#define X7_API_unregister_blkdev (KMODULE_API_START + 40)
int unregister_blkdev(unsigned int major, const char *name);

/* KMODULE[41], 0xc0019b00, register_filesystem; I. */
#define X7_API_register_filesystem (KMODULE_API_START + 41)
int register_filesystem(struct x7_filesystem_type *type);

/* KMODULE[42], 0xc0019b6c, unregister_filesystem; I. */
#define X7_API_unregister_filesystem (KMODULE_API_START + 42)
int unregister_filesystem(struct x7_filesystem_type *type);

/* KMODULE[43], 0xc0019eec, alloc_disk; I. */
#define X7_API_alloc_disk (KMODULE_API_START + 43)
struct x7_gendisk * alloc_disk(int minors);

/* KMODULE[44], 0xc0019fe0, add_disk; I. */
#define X7_API_add_disk (KMODULE_API_START + 44)
void add_disk(struct x7_gendisk *disk);

/* KMODULE[45], 0xc001a098, del_gendisk; I. */
#define X7_API_del_gendisk (KMODULE_API_START + 45)
void del_gendisk(struct x7_gendisk *disk);

/* KMODULE[46], 0xc0019fa0, put_disk; I. */
#define X7_API_put_disk (KMODULE_API_START + 46)
void put_disk(struct x7_gendisk *disk);

/* KMODULE[47], 0xc001a4b0, blk_init_queue; E. */
#define X7_API_blk_init_queue (KMODULE_API_START + 47)
struct x7_request_queue * blk_init_queue(x7_request_fn request);

/* KMODULE[48], 0xc001a54c, blk_cleanup_queue; I. */
#define X7_API_blk_cleanup_queue (KMODULE_API_START + 48)
void blk_cleanup_queue(struct x7_request_queue *queue);

/* KMODULE[49], 0xc001a610, elv_next_request; E. */
#define X7_API_elv_next_request (KMODULE_API_START + 49)
struct x7_request * elv_next_request(struct x7_request_queue *queue);

/* KMODULE[50], 0xc0018668, driver_register; I. */
#define X7_API_driver_register (KMODULE_API_START + 50)
int driver_register(struct x7_driver *driver);

/* KMODULE[51], 0xc0018770, driver_unregister; I. */
#define X7_API_driver_unregister (KMODULE_API_START + 51)
void driver_unregister(struct x7_driver *driver);

/* KMODULE[52], 0xc001a640, end_request; E. */
#define X7_API_end_request (KMODULE_API_START + 52)
void end_request(struct x7_request *request, int status);

/* KMODULE[53], 0xc0017a10, blk_read; E. */
#define X7_API_blk_read (KMODULE_API_START + 53)
x7_ssize_t blk_read(struct x7_inode *inode, struct x7_file *file, void *buffer, x7_size_t bytes, x7_off_t *position);

/* KMODULE[54], 0xc0017a24, blk_write; E. */
#define X7_API_blk_write (KMODULE_API_START + 54)
x7_ssize_t blk_write(struct x7_inode *inode, struct x7_file *file, const void *buffer, x7_size_t bytes, x7_off_t *position);

/* KMODULE[55], 0xc0017a3c, blk_ioctl; E. */
#define X7_API_blk_ioctl (KMODULE_API_START + 55)
int blk_ioctl(struct x7_inode *inode, struct x7_file *file, x7_u32 command, x7_word_t argument);

/* KMODULE[56], 0xc001846c, get_rel_name; E. */
#define X7_API_get_rel_name (KMODULE_API_START + 56)
char * get_rel_name(const char *path);

/* KMODULE[57], 0xc00185ac, get_base_name; E. */
#define X7_API_get_base_name (KMODULE_API_START + 57)
int get_base_name(const char *path, char *destination);

/* KMODULE[58], 0xc001863c, lookup_dentry; I. */
#define X7_API_lookup_dentry (KMODULE_API_START + 58)
struct x7_dentry * lookup_dentry(const char *path);

/* KMODULE[59], 0xc001c5f4, filp_open; I. */
#define X7_API_filp_open (KMODULE_API_START + 59)
struct x7_file * filp_open(const char *path, int flags, x7_u32 mode);

/* KMODULE[60], 0xc001c85c, filp_close; I. */
#define X7_API_filp_close (KMODULE_API_START + 60)
int filp_close(struct x7_file *file);

/* KMODULE[61], 0xc00199a4, -; I. */
#define X7_API_x7_kmodule_061 (KMODULE_API_START + 61)
void x7_kmodule_061(void *object);

/* KMODULE[62], 0xc001f95c, kernel_sym; E. */
#define X7_API_kernel_sym (KMODULE_API_START + 62)
void * kernel_sym(const char *name);

/* KMODULE[63], 0xc001fc88, init_timer; E. */
#define X7_API_init_timer (KMODULE_API_START + 63)
void init_timer(struct x7_timer *timer);

/* KMODULE[64], 0xc001fcec, add_timer; I. */
#define X7_API_add_timer (KMODULE_API_START + 64)
int add_timer(struct x7_timer *timer);

/* KMODULE[65], 0xc001fd0c, mod_timer; E. */
#define X7_API_mod_timer (KMODULE_API_START + 65)
int mod_timer(struct x7_timer *timer, x7_u32 expires);

/* KMODULE[66], 0xc001fd74, del_timer; E. */
#define X7_API_del_timer (KMODULE_API_START + 66)
int del_timer(struct x7_timer *timer);

/* KMODULE[67], 0xc001fdec, del_timer_sync; E. */
#define X7_API_del_timer_sync (KMODULE_API_START + 67)
int del_timer_sync(struct x7_timer *timer);

/* KMODULE[68], 0xc001f2c4, _spin_lock; E. */
#define X7_API__spin_lock (KMODULE_API_START + 68)
void _spin_lock(void *lock);

/* KMODULE[69], 0xc001f2e0, _spin_unlock; E. */
#define X7_API__spin_unlock (KMODULE_API_START + 69)
void _spin_unlock(void *lock);

/* KMODULE[70], 0xc001f2e8, _spin_lock_irq; E. */
#define X7_API__spin_lock_irq (KMODULE_API_START + 70)
void _spin_lock_irq(void *lock);

/* KMODULE[71], 0xc001f2f8, _spin_unlock_irq; E. */
#define X7_API__spin_unlock_irq (KMODULE_API_START + 71)
void _spin_unlock_irq(void *lock);

/* KMODULE[72], 0xc001f2a0, _spin_lock_irqsave; E. */
#define X7_API__spin_lock_irqsave (KMODULE_API_START + 72)
x7_u32 _spin_lock_irqsave(void *lock);

/* KMODULE[73], 0xc001f2ac, _spin_unlock_irqrestore; E. */
#define X7_API__spin_unlock_irqrestore (KMODULE_API_START + 73)
void _spin_unlock_irqrestore(void *lock, x7_u32 flags);

/* KMODULE[74], 0xc00204dc, schedule_work; I. */
#define X7_API_schedule_work (KMODULE_API_START + 74)
int schedule_work(struct x7_work *work);

/* KMODULE[75], 0xc00204ec, schedule_delayed_work; I. */
#define X7_API_schedule_delayed_work (KMODULE_API_START + 75)
int schedule_delayed_work(struct x7_work *work, x7_u32 delay);

/* KMODULE[76], 0xc0020504, flush_scheduled_work; I. */
#define X7_API_flush_scheduled_work (KMODULE_API_START + 76)
void flush_scheduled_work(void);

/* KMODULE[77], 0xc0020590, cancel_rearming_delayed_work; I. */
#define X7_API_cancel_rearming_delayed_work (KMODULE_API_START + 77)
void cancel_rearming_delayed_work(struct x7_work *work);

/* KMODULE[78], 0xc00114c8, tasklet_init; I. */
#define X7_API_tasklet_init (KMODULE_API_START + 78)
void tasklet_init(struct x7_tasklet *tasklet, x7_tasklet_fn function, x7_word_t argument);

/* KMODULE[79], 0xc00114e4, tasklet_kill; I. */
#define X7_API_tasklet_kill (KMODULE_API_START + 79)
void tasklet_kill(struct x7_tasklet *tasklet);

/* KMODULE[80], 0xc001123c, __tasklet_schedule; I. */
#define X7_API___tasklet_schedule (KMODULE_API_START + 80)
void __tasklet_schedule(struct x7_tasklet *tasklet);

/* KMODULE[81], 0xc00111fc, __tasklet_hi_schedule; I. */
#define X7_API___tasklet_hi_schedule (KMODULE_API_START + 81)
void __tasklet_hi_schedule(struct x7_tasklet *tasklet);

/* KMODULE[82], 0xc000f308, module_load; E. */
#define X7_API_module_load (KMODULE_API_START + 82)
int module_load(x7_word_t unused, x7_word_t task_address);

/* KMODULE[83], 0xc000452c, set_except_vector; I. */
#define X7_API_set_except_vector (KMODULE_API_START + 83)
void * set_except_vector(int exception, void *handler);

/* KMODULE[84], 0xc0001a70, set_hook_switch; I. */
#define X7_API_set_hook_switch (KMODULE_API_START + 84)
int set_hook_switch(x7_word_t hook);

/* KMODULE[85], 0xc0002b8c, config_pcnt; E. */
#define X7_API_config_pcnt (KMODULE_API_START + 85)
void config_pcnt(x7_u8 counter, const x7_u8 *configuration);

/* KMODULE[86], 0xc0002ab0, get_pcnt; E. */
#define X7_API_get_pcnt (KMODULE_API_START + 86)
x7_s64 get_pcnt(x7_u8 counter);

/* KMODULE[87], 0xc000b210, get_run_mode; E. */
#define X7_API_get_run_mode (KMODULE_API_START + 87)
x7_u8 get_run_mode(void);

/* KMODULE[88], 0xc000b234, set_run_mode; I. */
#define X7_API_set_run_mode (KMODULE_API_START + 88)
void set_run_mode(x7_u8 mode);

/* KMODULE[89], 0xc000b21c, is_stub_installed; E. */
#define X7_API_is_stub_installed (KMODULE_API_START + 89)
x7_u8 is_stub_installed(void);

/* KMODULE[90], 0xc000b228, install_stub; I. */
#define X7_API_install_stub (KMODULE_API_START + 90)
void install_stub(x7_u8 installed);

/* KMODULE[91], 0xc0002ccc, get_regs; E. */
#define X7_API_get_regs (KMODULE_API_START + 91)
x7_u32 get_regs(int selector);

/* KMODULE[92], 0xc0003574, get_ab_timer; E. */
#define X7_API_get_ab_timer (KMODULE_API_START + 92)
x7_u32 get_ab_timer(void);

/* KMODULE[93], 0xc0003760, get_ab_ticks; E. */
#define X7_API_get_ab_ticks (KMODULE_API_START + 93)
x7_u64 get_ab_ticks(void);

/* KMODULE[94], 0xc001d2b0, set_load_from_pc; I. */
#define X7_API_set_load_from_pc (KMODULE_API_START + 94)
int set_load_from_pc(int mode);

/* KMODULE[95], 0xc001d33c, set_image_path_name_of_pc; E. */
#define X7_API_set_image_path_name_of_pc (KMODULE_API_START + 95)
int set_image_path_name_of_pc(int image_type, const char *path);

/* KMODULE[96], 0xc000198c, -; I. */
#define X7_API_x7_kmodule_096 (KMODULE_API_START + 96)
int x7_kmodule_096(void *hook);

/* KMODULE[97], 0xc0001a00, -; E. */
#define X7_API_x7_kmodule_097 (KMODULE_API_START + 97)
void * x7_kmodule_097(void);

/* KMODULE[98], 0xc00107f8, -; E. */
#define X7_API_x7_kmodule_098 (KMODULE_API_START + 98)
int x7_kmodule_098(const char *format, ...);

/* KMODULE[99], 0xc00101dc, -; E. */
#define X7_API_x7_kmodule_099 (KMODULE_API_START + 99)
int x7_kmodule_099(void);

/* KMODULE[100], 0xc00101b0, -; I. */
#define X7_API_x7_kmodule_100 (KMODULE_API_START + 100)
int x7_kmodule_100(void *callback);

/* KMODULE[101], 0xc0001c18, set_uview_mode; E. */
#define X7_API_set_uview_mode (KMODULE_API_START + 101)
int set_uview_mode(int mode, x7_word_t argument);

/* KMODULE[102], 0xc0009b00, -; I. */
#define X7_API_x7_kmodule_102 (KMODULE_API_START + 102)
void x7_kmodule_102(x7_u32 address, x7_size_t bytes);

/* KMODULE[103], 0xc0009b80, -; I. */
#define X7_API_x7_kmodule_103 (KMODULE_API_START + 103)
void x7_kmodule_103(x7_u32 address, x7_size_t bytes);

/* KMODULE[104], 0xc000f28c, free_welcome_buffer; I. */
#define X7_API_free_welcome_buffer (KMODULE_API_START + 104)
void free_welcome_buffer(void);

/* KMODULE[105], 0xc001fce4, init_timer_lo; E. */
#define X7_API_init_timer_lo (KMODULE_API_START + 105)
void init_timer_lo(struct x7_timer *timer);

/* KMODULE[106], 0xc001fcfc, add_timer_lo; I. */
#define X7_API_add_timer_lo (KMODULE_API_START + 106)
int add_timer_lo(struct x7_timer *timer);

/* KMODULE[107], 0xc001fd40, mod_timer_lo; E. */
#define X7_API_mod_timer_lo (KMODULE_API_START + 107)
int mod_timer_lo(struct x7_timer *timer, x7_u32 expires);

/* KMODULE[108], 0xc001fdf4, del_timer_lo; E. */
#define X7_API_del_timer_lo (KMODULE_API_START + 108)
int del_timer_lo(struct x7_timer *timer);

/* KMODULE[109], 0xc001fdfc, del_timer_sync_lo; E. */
#define X7_API_del_timer_sync_lo (KMODULE_API_START + 109)
int del_timer_sync_lo(struct x7_timer *timer);

/* KMODULE[110], 0xc000b58c, -; E. */
#define X7_API_x7_kmodule_110 (KMODULE_API_START + 110)
void x7_kmodule_110(void);

/* KMODULE[111], 0xc000b58c, -; E. */
#define X7_API_x7_kmodule_111 (KMODULE_API_START + 111)
void x7_kmodule_111(void);

/* KMODULE[112], 0xc000b58c, -; E. */
#define X7_API_x7_kmodule_112 (KMODULE_API_START + 112)
void x7_kmodule_112(void);

#ifdef __cplusplus
}
#endif
#endif /* X7_SYSCALLS_H */
