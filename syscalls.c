/* See syscalls.h and syscalls.md. Each stub is exactly three instructions.
 * The firmware trap dispatcher returns to the caller's $ra. There is no
 * jr $ra after syscall, matching the vendor binaries and libg1.c.
 */
#include "syscalls.h"
#if !defined(__mips__) || !defined(__MIPSEL__) || !defined(_MIPS_SIM) || _MIPS_SIM != 1
#error "Build for little-endian MIPS O32 (-EL -mabi=32)"
#endif
_Static_assert(sizeof(void *) == 4 && sizeof(x7_off_t) == 8,
               "X7 requires 32-bit pointers and 64-bit offsets");
_Static_assert(sizeof(struct x7_stat) == 64 &&
               __builtin_offsetof(struct x7_stat, st_size) == 32 &&
               __builtin_offsetof(struct x7_stat, timestamp_38) == 56 &&
               sizeof(struct x7_statfs) == 52,
               "Recovered filesystem output layouts require MIPS O32 alignment");
_Static_assert(__builtin_offsetof(struct x7_dirent, name) == 16 &&
               __builtin_offsetof(struct x7_dirent, record_bytes) == 12,
               "Recovered directory record header offsets");
#define X7_STRING_INNER(x) #x
#define X7_STRING(x) X7_STRING_INNER(x)
#define X7_API(symbol, base, index) __asm__( \
    ".pushsection .text,\"ax\",@progbits\n" \
    ".set push\n.set noreorder\n.set nomips16\n.set nomicromips\n" \
    ".balign 4\n.globl " #symbol "\n.type " #symbol ", @function\n" \
    ".ent " #symbol "\n" #symbol ":\n.frame $sp,0,$ra\n" \
    "lui $v1, (" X7_STRING(base) ") >> 16\n" \
    "ori $v1, $v1, " X7_STRING(index) "\n" \
    "syscall\n.end " #symbol "\n.size " #symbol ", .-" #symbol "\n" \
    ".set pop\n.popsection\n")

X7_API(CP0_vEnableIM, SYSCALL_API_START, 0); /* 0xc0020bb0: CP0_vEnableIM */
X7_API(OS_Sched, SYSCALL_API_START, 1); /* 0xc0012b30: OS_Sched */
X7_API(OSSchedLock, SYSCALL_API_START, 2); /* 0xc00127c8: OSSchedLock */
X7_API(OSSchedUnlock, SYSCALL_API_START, 3); /* 0xc0012c30: OSSchedUnlock */
X7_API(OSTaskCreate, SYSCALL_API_START, 4); /* 0xc00158e8: OSTaskCreate */
X7_API(x7_syscall_005, SYSCALL_API_START, 5); /* 0xc0015a68: - */
X7_API(OSTaskDel, SYSCALL_API_START, 6); /* 0xc0015e64: OSTaskDel */
X7_API(x7_syscall_007, SYSCALL_API_START, 7); /* 0xc0016088: - */
X7_API(OSTaskDelReq, SYSCALL_API_START, 8); /* 0xc00161f4: OSTaskDelReq */
X7_API(OSTaskChangePrio, SYSCALL_API_START, 9); /* 0xc00154f8: OSTaskChangePrio */
X7_API(OSTaskResume, SYSCALL_API_START, 10); /* 0xc00162a8: OSTaskResume */
X7_API(OSTaskSuspend, SYSCALL_API_START, 11); /* 0xc00164a8: OSTaskSuspend */
X7_API(OSTaskCreateExt, SYSCALL_API_START, 12); /* 0xc0015778: OSTaskCreateExt */
X7_API(OSTaskQuery, SYSCALL_API_START, 13); /* 0xc00165d8: OSTaskQuery */
X7_API(OS_Getidfromprio, SYSCALL_API_START, 14); /* 0xc0012e3c: OS_Getidfromprio */
X7_API(OS_GetTaskID, SYSCALL_API_START, 15); /* 0xc0012e68: OS_GetTaskID */
X7_API(OSTimeDly, SYSCALL_API_START, 16); /* 0xc00168c0: OSTimeDly */
X7_API(OSTimeDlyResume, SYSCALL_API_START, 17); /* 0xc0016960: OSTimeDlyResume */
X7_API(OSTimeGet, SYSCALL_API_START, 18); /* 0xc0016a44: OSTimeGet */
X7_API(OSTimeSet, SYSCALL_API_START, 19); /* 0xc0016a78: OSTimeSet */
X7_API(OSQAccept, SYSCALL_API_START, 20); /* 0xc0013a5c: OSQAccept */
X7_API(OSQCreate, SYSCALL_API_START, 21); /* 0xc0013ba4: OSQCreate */
X7_API(OSQDel, SYSCALL_API_START, 22); /* 0xc0013cf8: OSQDel */
X7_API(OSQFlush, SYSCALL_API_START, 23); /* 0xc0013ee0: OSQFlush */
X7_API(OSQPend, SYSCALL_API_START, 24); /* 0xc0013fa8: OSQPend */
X7_API(OSQPost, SYSCALL_API_START, 25); /* 0xc00141b8: OSQPost */
X7_API(OSQPostByPrio, SYSCALL_API_START, 26); /* 0xc00142ec: OSQPostByPrio */
X7_API(OSQPostISR, SYSCALL_API_START, 27); /* 0xc0014568: OSQPostISR */
X7_API(OSSemAccept, SYSCALL_API_START, 28); /* 0xc0014cec: OSSemAccept */
X7_API(OSSemCreate, SYSCALL_API_START, 29); /* 0xc0014dc8: OSSemCreate */
X7_API(OSSemDel, SYSCALL_API_START, 30); /* 0xc0014ed4: OSSemDel */
X7_API(OSSemPend, SYSCALL_API_START, 31); /* 0xc00150ec: OSSemPend */
X7_API(OSSemPost, SYSCALL_API_START, 32); /* 0xc001532c: OSSemPost */
X7_API(OSSemQuery, SYSCALL_API_START, 33); /* 0xc001544c: OSSemQuery */
X7_API(OSFlagAccept, SYSCALL_API_START, 34); /* 0xc0012f94: OSFlagAccept */
X7_API(OSFlagCreate, SYSCALL_API_START, 35); /* 0xc0013154: OSFlagCreate */
X7_API(OSFlagDel, SYSCALL_API_START, 36); /* 0xc0013378: OSFlagDel */
X7_API(OSFlagPend, SYSCALL_API_START, 37); /* 0xc00134ec: OSFlagPend */
X7_API(OSFlagPost, SYSCALL_API_START, 38); /* 0xc00137d8: OSFlagPost */
X7_API(OSFlagQuery, SYSCALL_API_START, 39); /* 0xc00139d8: OSFlagQuery */
X7_API(api_install, SYSCALL_API_START, 40); /* 0xc0006f94: api_install */
X7_API(api_uninstall, SYSCALL_API_START, 41); /* 0xc0007014: api_uninstall */
X7_API(x7_syscall_042, SYSCALL_API_START, 42); /* 0xc0011e40: - */
X7_API(x7_syscall_043, SYSCALL_API_START, 43); /* 0xc0011e40: - */
X7_API(x7_syscall_044, SYSCALL_API_START, 44); /* 0xc0011e40: - */
X7_API(x7_syscall_045, SYSCALL_API_START, 45); /* 0xc0011e40: - */
X7_API(x7_syscall_046, SYSCALL_API_START, 46); /* 0xc0011e40: - */
X7_API(x7_syscall_047, SYSCALL_API_START, 47); /* 0xc0011e40: - */
X7_API(x7_syscall_048, SYSCALL_API_START, 48); /* 0xc0011e40: - */
X7_API(x7_syscall_049, SYSCALL_API_START, 49); /* 0xc0011e40: - */
X7_API(x7_printf, SYSCALL_API_START, 50); /* 0xc0011000: printf */
X7_API(serial_getc, SYSCALL_API_START, 51); /* 0xc000fe78: serial_getc */
X7_API(reset_baudrate, SYSCALL_API_START, 52); /* 0xc000fc8c: reset_baudrate */
X7_API(mdelay, SYSCALL_API_START, 53); /* 0xc0003958: mdelay */
X7_API(udelay, SYSCALL_API_START, 54); /* 0xc00039bc: udelay */
X7_API(x7_rand, SYSCALL_API_START, 55); /* 0xc000f348: rand */
X7_API(get_count, SYSCALL_API_START, 56); /* 0xc0006440: get_count */
X7_API(get_c0_count, SYSCALL_API_START, 57); /* 0xc00064a4: get_c0_count */
X7_API(x7_malloc, SYSCALL_API_START, 58); /* 0xc000c7b4: malloc */
X7_API(x7_free, SYSCALL_API_START, 59); /* 0xc000bf60: free */
X7_API(x7_sys_kmalloc, SYSCALL_API_START, 60); /* 0xc001f270: kmalloc */
X7_API(x7_sys_kfree, SYSCALL_API_START, 61); /* 0xc001f288: kfree */
X7_API(x7_realloc, SYSCALL_API_START, 62); /* 0xc000c314: realloc */
X7_API(os_mem_query, SYSCALL_API_START, 63); /* 0xc000e45c: os_mem_query */
X7_API(print_mem, SYSCALL_API_START, 64); /* 0xc000e46c: print_mem */
X7_API(sys_mount, SYSCALL_API_START, 65); /* 0xc001c23c: sys_mount */
X7_API(sys_umount, SYSCALL_API_START, 66); /* 0xc001bd24: sys_umount */
X7_API(sys_mknod, SYSCALL_API_START, 67); /* 0xc001afa4: sys_mknod */
X7_API(sys_open, SYSCALL_API_START, 68); /* 0xc001c848: sys_open */
X7_API(sys_close, SYSCALL_API_START, 69); /* 0xc001ca2c: sys_close */
X7_API(sys_read, SYSCALL_API_START, 70); /* 0xc001d1b0: sys_read */
X7_API(sys_write, SYSCALL_API_START, 71); /* 0xc001d230: sys_write */
X7_API(sys_lseek, SYSCALL_API_START, 72); /* 0xc001cd34: sys_lseek */
X7_API(sys_fcntl, SYSCALL_API_START, 73); /* 0xc0019724: sys_fcntl */
X7_API(sys_creat, SYSCALL_API_START, 74); /* 0xc001c850: sys_creat */
X7_API(sys_rename, SYSCALL_API_START, 75); /* 0xc001b2b8: sys_rename */
X7_API(sys_chdir, SYSCALL_API_START, 76); /* 0xc001b988: sys_chdir */
X7_API(sys_getcwd, SYSCALL_API_START, 77); /* 0xc00180dc: sys_getcwd */
X7_API(sys_mkdir, SYSCALL_API_START, 78); /* 0xc001ae18: sys_mkdir */
X7_API(sys_rmdir, SYSCALL_API_START, 79); /* 0xc001b43c: sys_rmdir */
X7_API(sys_fstat, SYSCALL_API_START, 80); /* 0xc001f308: sys_fstat */
X7_API(sys_stat, SYSCALL_API_START, 81); /* 0xc001f448: sys_stat */
X7_API(sys_statfs, SYSCALL_API_START, 82); /* 0xc001f558: sys_statfs */
X7_API(sys_utime, SYSCALL_API_START, 83); /* 0xc001c4a0: sys_utime */
X7_API(sys_truncate, SYSCALL_API_START, 84); /* 0xc001c278: sys_truncate */
X7_API(sys_ftruncate, SYSCALL_API_START, 85); /* 0xc001c398: sys_ftruncate */
X7_API(sys_fsync, SYSCALL_API_START, 86); /* 0xc0017a44: sys_fsync */
X7_API(sys_fchdir, SYSCALL_API_START, 87); /* 0xc001ba80: sys_fchdir */
X7_API(sys_flock, SYSCALL_API_START, 88); /* 0xc001bb30: sys_flock */
X7_API(sys_remove, SYSCALL_API_START, 89); /* 0xc001ca78: sys_remove */
X7_API(sys_tell, SYSCALL_API_START, 90); /* 0xc001ccf0: sys_tell */
X7_API(sys_readdir, SYSCALL_API_START, 91); /* 0xc001b5dc: sys_readdir */
X7_API(sys_seekdir, SYSCALL_API_START, 92); /* 0xc001b694: sys_seekdir */
X7_API(sys_rewinddir, SYSCALL_API_START, 93); /* 0xc001b738: sys_rewinddir */
X7_API(sys_lastdir, SYSCALL_API_START, 94); /* 0xc001b7c0: sys_lastdir */
X7_API(sys_prevdir, SYSCALL_API_START, 95); /* 0xc001b848: sys_prevdir */
X7_API(sys_reset2parentdir, SYSCALL_API_START, 96); /* 0xc001b900: sys_reset2parentdir */
X7_API(sys_ioctl, SYSCALL_API_START, 97); /* 0xc001a44c: sys_ioctl */
X7_API(sys_mmap, SYSCALL_API_START, 98); /* 0xc001a658: sys_mmap */
X7_API(sys_munmap, SYSCALL_API_START, 99); /* 0xc001a73c: sys_munmap */
X7_API(sys_msync, SYSCALL_API_START, 100); /* 0xc001ab2c: sys_msync */
X7_API(x7_fork, SYSCALL_API_START, 101); /* 0xc0019bbc: fork */
X7_API(x7_execve, SYSCALL_API_START, 102); /* 0xc0019634: _execve */
X7_API(x7_exit, SYSCALL_API_START, 103); /* 0xc0019494: _exit */
X7_API(sys_shm_open, SYSCALL_API_START, 104); /* 0xc001f0e8: sys_shm_open */
X7_API(sys_shm_unlink, SYSCALL_API_START, 105); /* 0xc001f1bc: sys_shm_unlink */
X7_API(insmod, SYSCALL_API_START, 106); /* 0xc001aa10: insmod */
X7_API(rmmod, SYSCALL_API_START, 107); /* 0xc001aa1c: rmmod */
X7_API(x7_dlopen, SYSCALL_API_START, 108); /* 0xc0018d00: dlopen */
X7_API(x7_dlclose, SYSCALL_API_START, 109); /* 0xc0019308: dlclose */
X7_API(x7_dlsym, SYSCALL_API_START, 110); /* 0xc0019314: dlsym */
X7_API(x7_dlerror, SYSCALL_API_START, 111); /* 0xc001943c: dlerror */
X7_API(x7_syscall_112, SYSCALL_API_START, 112); /* 0xc00033a8: - */
X7_API(x7_syscall_113, SYSCALL_API_START, 113); /* 0xc0011e48: - */
X7_API(x7_syscall_114, SYSCALL_API_START, 114); /* 0xc000c8f4: - */
X7_API(x7_syscall_115, SYSCALL_API_START, 115); /* 0xc000cae4: - */
X7_API(x7_syscall_116, SYSCALL_API_START, 116); /* 0xc000cbc0: - */
X7_API(x7_syscall_117, SYSCALL_API_START, 117); /* 0xc000cc18: - */
X7_API(x7_syscall_118, SYSCALL_API_START, 118); /* 0xc00214e0: - */
X7_API(x7_syscall_119, SYSCALL_API_START, 119); /* 0xc0021518: - */
X7_API(get_ic_version, SYSCALL_API_START, 120); /* 0xc000b1e8: get_ic_version */
X7_API(get_ic_type, SYSCALL_API_START, 121); /* 0xc000b1f4: get_ic_type */
X7_API(get_sdram_cap, SYSCALL_API_START, 122); /* 0xc000b200: get_sdram_cap */
X7_API(x7_syscall_123, SYSCALL_API_START, 123); /* 0xc000e37c: - */
X7_API(x7_syscall_124, SYSCALL_API_START, 124); /* 0xc000e3c0: - */
X7_API(x7_syscall_125, SYSCALL_API_START, 125); /* 0xc000a3b0: - */
X7_API(x7_syscall_126, SYSCALL_API_START, 126); /* 0xc0003514: - */
X7_API(x7_syscall_127, SYSCALL_API_START, 127); /* 0xc0010104: - */
X7_API(x7_syscall_128, SYSCALL_API_START, 128); /* 0xc000b240: - */
X7_API(x7_syscall_129, SYSCALL_API_START, 129); /* 0xc000b26c: - */
X7_API(os_cpu_save_sr, KMODULE_API_START, 0); /* 0xc0020b6c: os_cpu_save_sr */
X7_API(os_cpu_store_sr, KMODULE_API_START, 1); /* 0xc0020b78: os_cpu_store_sr */
X7_API(x7_memcpy, KMODULE_API_START, 2); /* 0xc00211c0: memcpy */
X7_API(x7_memset, KMODULE_API_START, 3); /* 0xc00213c0: memset */
X7_API(x7_strcpy, KMODULE_API_START, 4); /* 0xc0011780: strcpy */
X7_API(x7_strncpy, KMODULE_API_START, 5); /* 0xc00117a0: strncpy */
X7_API(x7_strcat, KMODULE_API_START, 6); /* 0xc0011874: strcat */
X7_API(x7_strcmp, KMODULE_API_START, 7); /* 0xc00119bc: strcmp */
X7_API(x7_strncmp, KMODULE_API_START, 8); /* 0xc00119e8: strncmp */
X7_API(x7_strrchr, KMODULE_API_START, 9); /* 0xc0011a54: strrchr */
X7_API(x7_strstr, KMODULE_API_START, 10); /* 0xc0011d54: strstr */
X7_API(x7_strlen, KMODULE_API_START, 11); /* 0xc00117cc: strlen */
X7_API(simple_strtoul, KMODULE_API_START, 12); /* 0xc000b000: simple_strtoul */
X7_API(simple_strtol, KMODULE_API_START, 13); /* 0xc000b184: simple_strtol */
X7_API(x7_memmove, KMODULE_API_START, 14); /* 0xc0011c94: memmove */
X7_API(x7_memcmp, KMODULE_API_START, 15); /* 0xc0011d00: memcmp */
X7_API(x7_memchr, KMODULE_API_START, 16); /* 0xc0011e08: memchr */
X7_API(strswab, KMODULE_API_START, 17); /* 0xc000afac: strswab */
X7_API(x7_strncat, KMODULE_API_START, 18); /* 0xc00118a8: strncat */
X7_API(x7_sprintf, KMODULE_API_START, 19); /* 0xc000af58: sprintf */
X7_API(r4k_dma_cache_wback_inv, KMODULE_API_START, 20); /* 0xc0009c80: r4k_dma_cache_wback_inv */
X7_API(r4k_dma_cache_inv, KMODULE_API_START, 21); /* 0xc0009c00: r4k_dma_cache_inv */
X7_API(r4k_flush_icache_range, KMODULE_API_START, 22); /* 0xc00099c4: r4k_flush_icache_range */
X7_API(request_irq, KMODULE_API_START, 23); /* 0xc0005630: request_irq */
X7_API(free_irq, KMODULE_API_START, 24); /* 0xc00057f4: free_irq */
X7_API(request_act213x_dma, KMODULE_API_START, 25); /* 0xc0003c34: request_act213x_dma */
X7_API(free_act213x_dma, KMODULE_API_START, 26); /* 0xc0004070: free_act213x_dma */
X7_API(get_dma_chan, KMODULE_API_START, 27); /* 0xc0003ba0: get_dma_chan */
X7_API(x7_kmodule_028, KMODULE_API_START, 28); /* 0xc000b58c: - */
X7_API(x7_kmodule_029, KMODULE_API_START, 29); /* 0xc000b58c: - */
X7_API(x7_kmodule_030, KMODULE_API_START, 30); /* 0xc000b58c: - */
X7_API(x7_kmodule_031, KMODULE_API_START, 31); /* 0xc000b58c: - */
X7_API(get_sysmsg, KMODULE_API_START, 32); /* 0xc0012500: get_sysmsg */
X7_API(put_sysmsg, KMODULE_API_START, 33); /* 0xc0012490: put_sysmsg */
X7_API(flush_sysmsg, KMODULE_API_START, 34); /* 0xc0012454: flush_sysmsg */
X7_API(kmalloc, KMODULE_API_START, 35); /* 0xc001f270: kmalloc */
X7_API(kfree, KMODULE_API_START, 36); /* 0xc001f288: kfree */
X7_API(register_chrdev, KMODULE_API_START, 37); /* 0xc0017b58: register_chrdev */
X7_API(unregister_chrdev, KMODULE_API_START, 38); /* 0xc0017cdc: unregister_chrdev */
X7_API(register_blkdev, KMODULE_API_START, 39); /* 0xc0019bc4: register_blkdev */
X7_API(unregister_blkdev, KMODULE_API_START, 40); /* 0xc0019dc4: unregister_blkdev */
X7_API(register_filesystem, KMODULE_API_START, 41); /* 0xc0019b00: register_filesystem */
X7_API(unregister_filesystem, KMODULE_API_START, 42); /* 0xc0019b6c: unregister_filesystem */
X7_API(alloc_disk, KMODULE_API_START, 43); /* 0xc0019eec: alloc_disk */
X7_API(add_disk, KMODULE_API_START, 44); /* 0xc0019fe0: add_disk */
X7_API(del_gendisk, KMODULE_API_START, 45); /* 0xc001a098: del_gendisk */
X7_API(put_disk, KMODULE_API_START, 46); /* 0xc0019fa0: put_disk */
X7_API(blk_init_queue, KMODULE_API_START, 47); /* 0xc001a4b0: blk_init_queue */
X7_API(blk_cleanup_queue, KMODULE_API_START, 48); /* 0xc001a54c: blk_cleanup_queue */
X7_API(elv_next_request, KMODULE_API_START, 49); /* 0xc001a610: elv_next_request */
X7_API(driver_register, KMODULE_API_START, 50); /* 0xc0018668: driver_register */
X7_API(driver_unregister, KMODULE_API_START, 51); /* 0xc0018770: driver_unregister */
X7_API(end_request, KMODULE_API_START, 52); /* 0xc001a640: end_request */
X7_API(blk_read, KMODULE_API_START, 53); /* 0xc0017a10: blk_read */
X7_API(blk_write, KMODULE_API_START, 54); /* 0xc0017a24: blk_write */
X7_API(blk_ioctl, KMODULE_API_START, 55); /* 0xc0017a3c: blk_ioctl */
X7_API(get_rel_name, KMODULE_API_START, 56); /* 0xc001846c: get_rel_name */
X7_API(get_base_name, KMODULE_API_START, 57); /* 0xc00185ac: get_base_name */
X7_API(lookup_dentry, KMODULE_API_START, 58); /* 0xc001863c: lookup_dentry */
X7_API(filp_open, KMODULE_API_START, 59); /* 0xc001c5f4: filp_open */
X7_API(filp_close, KMODULE_API_START, 60); /* 0xc001c85c: filp_close */
X7_API(x7_kmodule_061, KMODULE_API_START, 61); /* 0xc00199a4: - */
X7_API(kernel_sym, KMODULE_API_START, 62); /* 0xc001f95c: kernel_sym */
X7_API(init_timer, KMODULE_API_START, 63); /* 0xc001fc88: init_timer */
X7_API(add_timer, KMODULE_API_START, 64); /* 0xc001fcec: add_timer */
X7_API(mod_timer, KMODULE_API_START, 65); /* 0xc001fd0c: mod_timer */
X7_API(del_timer, KMODULE_API_START, 66); /* 0xc001fd74: del_timer */
X7_API(del_timer_sync, KMODULE_API_START, 67); /* 0xc001fdec: del_timer_sync */
X7_API(_spin_lock, KMODULE_API_START, 68); /* 0xc001f2c4: _spin_lock */
X7_API(_spin_unlock, KMODULE_API_START, 69); /* 0xc001f2e0: _spin_unlock */
X7_API(_spin_lock_irq, KMODULE_API_START, 70); /* 0xc001f2e8: _spin_lock_irq */
X7_API(_spin_unlock_irq, KMODULE_API_START, 71); /* 0xc001f2f8: _spin_unlock_irq */
X7_API(_spin_lock_irqsave, KMODULE_API_START, 72); /* 0xc001f2a0: _spin_lock_irqsave */
X7_API(_spin_unlock_irqrestore, KMODULE_API_START, 73); /* 0xc001f2ac: _spin_unlock_irqrestore */
X7_API(schedule_work, KMODULE_API_START, 74); /* 0xc00204dc: schedule_work */
X7_API(schedule_delayed_work, KMODULE_API_START, 75); /* 0xc00204ec: schedule_delayed_work */
X7_API(flush_scheduled_work, KMODULE_API_START, 76); /* 0xc0020504: flush_scheduled_work */
X7_API(cancel_rearming_delayed_work, KMODULE_API_START, 77); /* 0xc0020590: cancel_rearming_delayed_work */
X7_API(tasklet_init, KMODULE_API_START, 78); /* 0xc00114c8: tasklet_init */
X7_API(tasklet_kill, KMODULE_API_START, 79); /* 0xc00114e4: tasklet_kill */
X7_API(__tasklet_schedule, KMODULE_API_START, 80); /* 0xc001123c: __tasklet_schedule */
X7_API(__tasklet_hi_schedule, KMODULE_API_START, 81); /* 0xc00111fc: __tasklet_hi_schedule */
X7_API(module_load, KMODULE_API_START, 82); /* 0xc000f308: module_load */
X7_API(set_except_vector, KMODULE_API_START, 83); /* 0xc000452c: set_except_vector */
X7_API(set_hook_switch, KMODULE_API_START, 84); /* 0xc0001a70: set_hook_switch */
X7_API(config_pcnt, KMODULE_API_START, 85); /* 0xc0002b8c: config_pcnt */
X7_API(get_pcnt, KMODULE_API_START, 86); /* 0xc0002ab0: get_pcnt */
X7_API(get_run_mode, KMODULE_API_START, 87); /* 0xc000b210: get_run_mode */
X7_API(set_run_mode, KMODULE_API_START, 88); /* 0xc000b234: set_run_mode */
X7_API(is_stub_installed, KMODULE_API_START, 89); /* 0xc000b21c: is_stub_installed */
X7_API(install_stub, KMODULE_API_START, 90); /* 0xc000b228: install_stub */
X7_API(get_regs, KMODULE_API_START, 91); /* 0xc0002ccc: get_regs */
X7_API(get_ab_timer, KMODULE_API_START, 92); /* 0xc0003574: get_ab_timer */
X7_API(get_ab_ticks, KMODULE_API_START, 93); /* 0xc0003760: get_ab_ticks */
X7_API(set_load_from_pc, KMODULE_API_START, 94); /* 0xc001d2b0: set_load_from_pc */
X7_API(set_image_path_name_of_pc, KMODULE_API_START, 95); /* 0xc001d33c: set_image_path_name_of_pc */
X7_API(x7_kmodule_096, KMODULE_API_START, 96); /* 0xc000198c: - */
X7_API(x7_kmodule_097, KMODULE_API_START, 97); /* 0xc0001a00: - */
X7_API(x7_kmodule_098, KMODULE_API_START, 98); /* 0xc00107f8: - */
X7_API(x7_kmodule_099, KMODULE_API_START, 99); /* 0xc00101dc: - */
X7_API(x7_kmodule_100, KMODULE_API_START, 100); /* 0xc00101b0: - */
X7_API(set_uview_mode, KMODULE_API_START, 101); /* 0xc0001c18: set_uview_mode */
X7_API(x7_kmodule_102, KMODULE_API_START, 102); /* 0xc0009b00: - */
X7_API(x7_kmodule_103, KMODULE_API_START, 103); /* 0xc0009b80: - */
X7_API(free_welcome_buffer, KMODULE_API_START, 104); /* 0xc000f28c: free_welcome_buffer */
X7_API(init_timer_lo, KMODULE_API_START, 105); /* 0xc001fce4: init_timer_lo */
X7_API(add_timer_lo, KMODULE_API_START, 106); /* 0xc001fcfc: add_timer_lo */
X7_API(mod_timer_lo, KMODULE_API_START, 107); /* 0xc001fd40: mod_timer_lo */
X7_API(del_timer_lo, KMODULE_API_START, 108); /* 0xc001fdf4: del_timer_lo */
X7_API(del_timer_sync_lo, KMODULE_API_START, 109); /* 0xc001fdfc: del_timer_sync_lo */
X7_API(x7_kmodule_110, KMODULE_API_START, 110); /* 0xc000b58c: - */
X7_API(x7_kmodule_111, KMODULE_API_START, 111); /* 0xc000b58c: - */
X7_API(x7_kmodule_112, KMODULE_API_START, 112); /* 0xc000b58c: - */
