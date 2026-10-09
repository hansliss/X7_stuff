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
#define O_CREAT  0x200
#define O_TRUNC  0x400

typedef uint size_t;
typedef int  ssize_t;

long sys_open(const char *filename, int flags, int mode);
long sys_close(uint fd);
ssize_t sys_read(uint fd, char *buf, size_t count);
ssize_t sys_write(uint fd, const char *buf, size_t count);

API(sys_open,	SYSCALL_API_START, 68);
API(sys_close,	SYSCALL_API_START, 69);
API(sys_read,	SYSCALL_API_START, 70);
API(sys_write,	SYSCALL_API_START, 71);

#define KMODULE_API_START	0x20000

// Linux kernel 2.6 verbatim github.com/mcirsta/ActSDK/blob/master/psp_rel/include/ucos/gfp.h#L14
// Actions are very much likely violating GPL by not releasing the full source
#define __GFP_WAIT	0x10u	/* Can wait and reschedule? */
#define __GFP_IO	0x40u	/* Can start physical IO? */
#define __GFP_FS	0x80u	/* Can call down to low-level FS? */

#define DISKA_LBA      0x22000
#define DISKA_SECTORS  0x200000
#define FILE_SECTORS   0x8000      /* 4 MiB in 512-byte sectors */

#define GFP_KERNEL	(__GFP_WAIT | __GFP_IO | __GFP_FS)

void *kmalloc(size_t size, uint flags);
void kfree(const void *);

void * kernel_sym(char *name);

API(kmalloc,	KMODULE_API_START, 35);
API(kfree,	KMODULE_API_START, 36);

API(kernel_sym,	KMODULE_API_START, 62);

// thin wrapper for FTL_Read github.com/bnister/am12xx_8268b/blob/master/sdk/linux/drivers/mtd/am7x_nftl/Flash/common/logic/logic_adapt_layer.c#L419
int (* nand_adfu_read)(uint lba, void *buf, uint length);
// likewise for PHY_PageRead (GL5009 NAND physical layer source never published)
int (* brec_sector_read)(uint block, uint sector, void *buf);

#define CHUNK 0x8000 // physical IO may have to reserve rather limited SRAM

#define FW_LBA 0x20000 // /mnt/sdisk is only 0x12000 + 2x 0x2000

void make_filename(char *nbuf, int num) {
  nbuf[14] = (num % 1000) / 100 + '0';  
  nbuf[15] = (num % 100) / 10 + '0';  
  nbuf[16] = (num % 10) + '0';  
}

static int file_exists(const char *path)
{
    int fd;

    fd = sys_open(path, O_RDONLY, 0);
    if (fd < 0)
        return 0;

    sys_close(fd);
    return 1;
}

void __attribute__ ((section (".init"))) _init(void)
{
    uchar *buf;
    int fd;
    int file_no;
    int start;
    char *name = "/mnt/card/dump000.bin"; // 14, 15, 16
    char *okname = "/mnt/card/dump000.ok"; // 14, 15, 16
    uint done;

    nand_adfu_read = kernel_sym("nand_adfu_read");
    buf = kmalloc(CHUNK, GFP_KERNEL);

    for (file_no = 0; file_no < DISKA_SECTORS / FILE_SECTORS; file_no++) {
      make_filename(name, file_no);    /* /mnt/card/dump000.bin etc. */
      make_filename(okname, file_no);    /* /mnt/card/dump000.bin etc. */

      if (file_exists(okname)) {
        continue;
      }
      
      fd = sys_open(name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
      if (fd < 0)
        break;
      
      start = file_no * FILE_SECTORS;
      
      for (done = 0; done < FILE_SECTORS; done += CHUNK / 512) {
        nand_adfu_read(DISKA_LBA + start + done,
                       buf, CHUNK / 512);
        
        if (sys_write(fd, buf, CHUNK) != CHUNK)
          break;
      }
      
      sys_close(fd);
      fd = sys_open(okname, O_WRONLY | O_CREAT | O_TRUNC, 0666);
      if (fd < 0)
        break;
      sys_write(fd, "OK\n", 3);
      sys_close(fd);
    }
    if (buf)
        kfree(buf);
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
