/* Real files with fault injection: failed commits must retain old scores. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../../syscalls.h"
#include "../nvram.h"

static const char *directory;
static int fault,writing;
static void path(char *out,const char *name){
    snprintf(out,512,"%s/%s",directory,strrchr(name,'/')+1);
}
int sys_open(const char *name,int flags,x7_u32 mode){
    char p[512];int fd,f=flags&3;(void)mode;path(p,name);
    writing=!!(flags&X7_O_WRONLY);
    if(writing&&fault==1)return -5;
    if(flags&X7_O_CREAT)f|=O_CREAT;
    if(flags&X7_O_TRUNC)f|=O_TRUNC;
    fd=open(p,f,0600);return fd<0?-2:fd;
}
int sys_close(int fd){int r=close(fd);return writing&&fault==4?-5:r;}
x7_ssize_t sys_read(int fd,void *p,x7_size_t n){
    if(fault==6)return -5;
    return read(fd,p,n>7?7:n);
}
x7_ssize_t sys_write(int fd,const void *p,x7_size_t n){
    if(fault==2&&lseek(fd,0,SEEK_CUR)>=14)return -5;
    return write(fd,p,n>7?7:n);
}
int sys_fstat(int fd,struct x7_stat *out){
    struct stat st;if(fstat(fd,&st)<0||fault==7)return -5;
    out->st_size=st.st_size;return 0;
}
int sys_fsync(int fd){return fault==3?-5:fsync(fd);}
int sys_rename(const char *old,const char *new){
    char a[512],b[512];path(a,old);path(b,new);return fault==5?-5:rename(a,b);
}
static void unchanged(const unsigned char *expected){
    unsigned char loaded[256];fault=0;memset(loaded,0,sizeof(loaded));
    assert(!omega_nvram_load(loaded));assert(!memcmp(expected,loaded,256));
}
int main(int argc,char **argv){
    unsigned char first[256],next[256],loaded[256];char p[512];unsigned i;int fd;
    assert(argc==2);directory=argv[1];
    for(i=0;i<256;i++){first[i]=(unsigned char)(i*17);next[i]=(unsigned char)(i*31+3);}
    memset(loaded,0xa5,256);assert(omega_nvram_load(loaded)==1);
    for(i=0;i<256;i++)assert(loaded[i]==0xa5);
    assert(!omega_nvram_save(first));unchanged(first);
    for(i=1;i<=5;i++){fault=(int)i;assert(omega_nvram_save(next)==-1);unchanged(first);}
    for(i=6;i<=7;i++){
        fault=(int)i;memset(loaded,0xa5,256);assert(omega_nvram_load(loaded)==-1);
        for(unsigned j=0;j<256;j++)assert(loaded[j]==0xa5);
    }
    fault=0;
    /* Invalid magic, size, CRC and payload all leave caller memory intact. */
    for(i=0;i<4;i++){
        static const unsigned offsets[]={0,8,12,32};unsigned char byte;
        assert(!omega_nvram_save(first));path(p,OMEGA_NVRAM_PATH);fd=open(p,O_RDWR);assert(fd>=0);
        assert(lseek(fd,offsets[i],SEEK_SET)>=0&&read(fd,&byte,1)==1);byte^=1;
        assert(lseek(fd,offsets[i],SEEK_SET)>=0&&write(fd,&byte,1)==1);assert(!close(fd));
        memset(loaded,0xa5,256);assert(omega_nvram_load(loaded)==-1);
        for(unsigned j=0;j<256;j++)assert(loaded[j]==0xa5);
    }
    for(i=0;i<2;i++){
        assert(!omega_nvram_save(first));path(p,OMEGA_NVRAM_PATH);
        assert(!truncate(p,i?273:100));assert(omega_nvram_load(loaded)==-1);
    }
    assert(!omega_nvram_save(next));unchanged(next);
    puts("NVRAM round-trip, partial I/O, corrupt files and failed atomic commits passed.");return 0;
}
