/* end of C89 comment started by the shell script */
typedef unsigned char uchar;
typedef unsigned int  uint;

#define LEAF(sym) \
	"	.globl	" #sym "\n" \
	"	.align	2\n" \
	"	.type	" #sym ", @function\n" \
	"	.ent	" #sym ", 0\n" \
	#sym ":	.frame	$sp,0,$ra\n"

#define END(func) \
	"	.end	" #func "\n" \
	"	.size	" #func ", . -" #func "\n"

#define STR(s) #s

#define API(func, start, index) __asm__( \
	LEAF(func) \
	"	.set	noreorder\n" \
	"	.text\n" \
	"	li	$v1, " STR(start) " + " STR(index) "\n" \
	"	syscall\n" \
	END(func) \
)

#define SYSCALL_API_START	0x10000

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT  0x200
#define O_TRUNC  0x400

typedef uint size_t;
typedef int  ssize_t;
typedef long long off_t;

long sys_open(const char *filename, int flags, int mode);
long sys_close(uint fd);
ssize_t sys_read(uint fd, char *buf, size_t count);
ssize_t sys_write(uint fd, const char *buf, size_t count);
off_t sys_lseek(int fd, off_t offset, int whence);
int sys_fsync(int fd);

#define SEEK_SET 0

API(sys_open,	SYSCALL_API_START, 68);
API(sys_close,	SYSCALL_API_START, 69);
API(sys_read,	SYSCALL_API_START, 70);
API(sys_write,	SYSCALL_API_START, 71);
API(sys_lseek,  SYSCALL_API_START, 72);
API(sys_fsync,  SYSCALL_API_START, 86);

#define KMODULE_API_START	0x20000

// Linux kernel 2.6 verbatim github.com/mcirsta/ActSDK/blob/master/psp_rel/include/ucos/gfp.h#L14
// Actions are very much likely violating GPL by not releasing the full source
#define __GFP_WAIT	0x10u	/* Can wait and reschedule? */
#define __GFP_IO	0x40u	/* Can start physical IO? */
#define __GFP_FS	0x80u	/* Can call down to low-level FS? */

#define GFP_KERNEL	(__GFP_WAIT | __GFP_IO | __GFP_FS)

void *kmalloc(size_t size, uint flags);
void kfree(const void *);

void * kernel_sym(char *name);

API(kmalloc,	KMODULE_API_START, 35);
API(kfree,	KMODULE_API_START, 36);

API(kernel_sym,	KMODULE_API_START, 62);

void __attribute__ ((section (".init"))) _init(void)
{
    int fd;
    char val;

    fd = sys_open("/mnt/diska/apps/usb/usb.app", O_RDWR, 0666);
    int fd2 = sys_open("/mnt/card/foo.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
      sys_write(fd2, "0", 1);
      if (sys_lseek(fd, 0x048c, SEEK_SET) != 0x048c)
        goto out;
      
      sys_write(fd2, "1", 1);
      if (sys_read(fd, &val, 1) != 1 || val != 1)
        goto out;
      
      sys_write(fd2, "2", 1);
      if (sys_lseek(fd, 0x048c, SEEK_SET) != 0x048c)
        goto out;
       
      sys_write(fd2, "3", 1);
      val = 0;
      if (sys_write(fd, &val, 1) != 1)
        goto out;
      
      sys_write(fd2, "4", 1);
      sys_fsync(fd);
    out:
      sys_close(fd);
      sys_close(fd2);
    }
}

void __attribute__ ((section (".fini"))) _fini(void) {
}

static const char *G1_SO_VERSION = "R0.00"; // must not update even R1.00 lib
static const char __dlstrtab_G1_SO_VERSION[]
	__attribute__ ((section (".dlstr")))
	= "G1_SO_VERSION";
static const struct dll_symbol {
	const void *value;
	const char *name;
} __dlsymtab_G1_SO_VERSION
	__attribute__ ((section (".dlsym"), used))
	= { &G1_SO_VERSION, __dlstrtab_G1_SO_VERSION };
