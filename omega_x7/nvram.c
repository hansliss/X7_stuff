#include <string.h>
#include "../syscalls.h"
#include "nvram.h"

/* Explicit byte format, independent of compiler layout and host endianness:
 * 8-byte magic/version, LE32 payload length, LE32 CRC, 256 board RAM bytes. */
static const unsigned char magic[8]={'X','7','O','R','N','V','1',0};
static unsigned crc32(const unsigned char *data,unsigned size){
    unsigned crc=~0u,i,j;
    for(i=0;i<size;i++){
        crc^=data[i];
        for(j=0;j<8;j++)crc=(crc>>1)^((0u-(crc&1))&0xedb88320u);
    }
    return ~crc;
}
static unsigned get32(const unsigned char *p){
    return (unsigned)p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);
}
static void put32(unsigned char *p,unsigned n){
    unsigned i;for(i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));
}
int omega_nvram_load(unsigned char *ram){
    unsigned char file[16+OMEGA_NVRAM_SIZE];
    struct x7_stat st;unsigned n=0,i;int fd,r,valid=1;
    fd=sys_open(OMEGA_NVRAM_PATH,X7_O_RDONLY,0);
    if(fd<0)return fd==-2?1:-1;
    memset(&st,0,sizeof(st));
    if(sys_fstat(fd,&st)<0||st.st_size!=(x7_s64)sizeof(file))valid=0;
    while(valid&&n<sizeof(file)){
        r=sys_read(fd,file+n,sizeof(file)-n);
        if(r<=0||(unsigned)r>sizeof(file)-n){valid=0;break;}
        n+=(unsigned)r;
    }
    if(sys_close(fd)<0)valid=0;
    if(!valid)return -1;
    for(i=0;i<sizeof(magic);i++)if(file[i]!=magic[i])return -1;
    if(get32(file+8)!=OMEGA_NVRAM_SIZE||get32(file+12)!=crc32(file+16,OMEGA_NVRAM_SIZE))return -1;
    memcpy(ram,file+16,OMEGA_NVRAM_SIZE);
    return 0;
}
int omega_nvram_save(const unsigned char *ram){
    unsigned char file[16+OMEGA_NVRAM_SIZE];unsigned n=0;int fd,r,valid=1;
    memcpy(file,magic,sizeof(magic));put32(file+8,OMEGA_NVRAM_SIZE);
    put32(file+12,crc32(ram,OMEGA_NVRAM_SIZE));memcpy(file+16,ram,OMEGA_NVRAM_SIZE);
    fd=sys_open(OMEGA_NVRAM_TEMP,X7_O_WRONLY|X7_O_CREAT|X7_O_TRUNC,0600);
    if(fd<0)return -1;
    while(n<sizeof(file)){
        r=sys_write(fd,file+n,sizeof(file)-n);
        if(r<=0||(unsigned)r>sizeof(file)-n){valid=0;break;}
        n+=(unsigned)r;
    }
    if(valid&&sys_fsync(fd)<0)valid=0;
    if(sys_close(fd)<0)valid=0;
    if(!valid)return -1;
    return sys_rename(OMEGA_NVRAM_TEMP,OMEGA_NVRAM_PATH)<0?-1:0;
}
