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

void __attribute__ ((section (".init"))) _init(void) {
	uchar *buf;
	int fd, lba, fw_lba, block, sector;

	nand_adfu_read = kernel_sym("nand_adfu_read");

	buf = kmalloc(CHUNK, GFP_KERNEL);

	fd = sys_open("/mnt/disk0/x12_sdisk.fw", O_WRONLY | O_CREAT | O_TRUNC, 0666);
	nand_adfu_read(0, buf, 512); // LFI Header
	fw_lba = buf[128] | buf[129] << 8 | buf[130] << 16;
	for (lba = 0; lba < fw_lba; lba += CHUNK / 512) {
		nand_adfu_read(lba, buf, CHUNK / 512);
		sys_write(fd, buf, CHUNK);
	}
	sys_close(fd);

	brec_sector_read = kernel_sym("brec_sector_read");

	fd = sys_open("/mnt/disk0/x12_boot.bin", O_WRONLY | O_CREAT | O_TRUNC, 0666);
	for (block = 0; block < 4; block++) { // range checked in sm.drv
		for (sector = 0; sector < 0x400; sector++) {
			brec_sector_read(block, sector, buf);
			sys_write(fd, buf, 512);
		}
	}
	sys_close(fd);

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
