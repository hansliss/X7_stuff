#include "x7_runtime.h"
#include "../syscalls.h"

/* Small compatibility layer. Engine handles are indices, firmware handles
 * remain opaque and are never tested as Linux descriptors. */
static int handles[32];
static unsigned char occupied[32];
static FILE files[8];
static FILE console={-1};
FILE *stdin=&console,*stdout=&console,*stderr=&console;
typedef struct allocation {
    struct allocation *next,*prev;
    unsigned size,magic;
} allocation;
static allocation *allocations;
static unsigned live_bytes,peak_bytes,alloc_count;

void *memcpy(void *d,const void *s,size_t n){unsigned char *p=d;const unsigned char *q=s;while(n--)*p++=*q++;return d;}
void *memmove(void *d,const void *s,size_t n){unsigned char *p=d;const unsigned char *q=s;if(p<q)return memcpy(d,s,n);while(n){--n;p[n]=q[n];}return d;}
void *memset(void *d,int c,size_t n){unsigned char *p=d;while(n--)*p++=(unsigned char)c;return d;}
int memcmp(const void *a,const void *b,size_t n){const unsigned char *p=a,*q=b;while(n--){if(*p!=*q)return *p-*q;p++;q++;}return 0;}
size_t strlen(const char *s){const char *p=s;while(*p)p++;return p-s;}
char *strcpy(char *d,const char *s){char *p=d;while((*p++=*s++));return d;}
char *strncpy(char *d,const char *s,size_t n){char *p=d;while(n&&*s){*p++=*s++;n--;}while(n--)*p++=0;return d;}
char *strcat(char *d,const char *s){strcpy(d+strlen(d),s);return d;}
int strcmp(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return (unsigned char)*a-(unsigned char)*b;}
int strncmp(const char *a,const char *b,size_t n){while(n--){if(*a!=*b)return (unsigned char)*a-(unsigned char)*b;if(!*a)return 0;a++;b++;}return 0;}
int toupper(int c){return c>='a'&&c<='z'?c-32:c;}
int tolower(int c){return c>='A'&&c<='Z'?c+32:c;}
int isdigit(int c){return c>='0'&&c<='9';}
int isspace(int c){return c==' '||c=='\t'||c=='\n'||c=='\r';}
int strncasecmp(const char *a,const char *b,size_t n){while(n--){int d=toupper((unsigned char)*a)-toupper((unsigned char)*b);if(d||!*a)return d;a++;b++;}return 0;}
int strcasecmp(const char *a,const char *b){return strncasecmp(a,b,(size_t)-1);}
int abs(int a){return a<0?(int)(0u-(unsigned)a):a;}
int atoi(const char *s){unsigned n=0;int sign=1;while(isspace(*s))s++;if(*s=='-'||*s=='+'){if(*s=='-')sign=-1;s++;}while(isdigit(*s))n=n*10+(*s++-'0');return sign<0?(int)(0u-n):(int)n;}

void *malloc(size_t n){
    allocation *p;
    if(!n)n=1;
    if(n>0x1000000u-sizeof(*p))return 0;
    p=x7_sys_kmalloc(n+sizeof(*p),X7_GFP_KERNEL);
    if(!p)return 0;
    p->size=n;p->magic=0x5847444d;p->prev=0;p->next=allocations;
    if(allocations)allocations->prev=p;
    allocations=p;live_bytes+=n;alloc_count++;
    if(live_bytes>peak_bytes)peak_bytes=live_bytes;
    return p+1;
}
void free(void *ptr){
    allocation *p;
    if(!ptr)return;
    p=(allocation *)ptr-1;
    if(p->magic!=0x5847444d)exit(2);
    if(p->prev)p->prev->next=p->next;else allocations=p->next;
    if(p->next)p->next->prev=p->prev;
    live_bytes-=p->size;alloc_count--;p->magic=0;x7_sys_kfree(p);
}
void *calloc(size_t n,size_t s){void *p;if(s&&n>(size_t)-1/s)return 0;p=malloc(n*s);if(p)memset(p,0,n*s);return p;}
void *realloc(void *old,size_t n){void *p;unsigned s;if(!old)return malloc(n);if(!n){free(old);return 0;}s=((allocation *)old-1)->size;p=malloc(n);if(!p)return 0;memcpy(p,old,s<n?s:n);free(old);return p;}

int open(const char *name,int flags,...){
    unsigned i;int fd;
    for(i=0;i<32;i++)if(!occupied[i])break;
    if(i==32)return -1;
    fd=sys_open(name,flags,0600);if(fd<0)return -1;
    handles[i]=fd;occupied[i]=1;return (int)i;
}
static int native_fd(int f){return f>=0&&f<32&&occupied[f]?handles[f]:-1;}
int close(int f){int fd=native_fd(f),r;if(fd<0)return -1;r=sys_close(fd);occupied[f]=0;return r<0?-1:0;}
int read(int f,void *p,unsigned n){unsigned done=0;int fd=native_fd(f);if(fd<0)return -1;while(done<n){int r=sys_read(fd,(char *)p+done,n-done);if(r<0)return -1;if(!r)break;if((unsigned)r>n-done)return -1;done+=r;}x7_loading_checkpoint();return done;}
int write(int f,const void *p,unsigned n){unsigned done=0;int fd=native_fd(f);if(fd<0)return -1;while(done<n){int r=sys_write(fd,(const char *)p+done,n-done);if(r<=0||(unsigned)r>n-done)return -1;done+=r;}return done;}
int lseek(int f,int offset,int whence){x7_s64 r;int fd=native_fd(f);if(fd<0)return -1;r=sys_lseek(fd,offset,whence);return r<0||r>MAXINT?-1:(int)r;}
int fstat(int f,struct stat *s){struct x7_stat st;int fd=native_fd(f);memset(&st,0,sizeof(st));if(fd<0||sys_fstat(fd,&st)<0||st.st_size<0||st.st_size>MAXINT)return -1;s->st_size=(int)st.st_size;return 0;}
int access(const char *name,int mode){int fd;(void)mode;fd=open(name,O_RDONLY);if(fd<0)return -1;return close(fd);}
int mkdir(const char *n,int mode){(void)n;(void)mode;return -1;}
FILE *fopen(const char *name,const char *mode){unsigned i;int fd;for(i=0;i<8;i++)if(!files[i].fd)break;if(i==8)return 0;fd=open(name,*mode=='w'?O_WRONLY|O_CREAT|O_TRUNC:O_RDONLY);if(fd<0)return 0;files[i].fd=fd+1;return &files[i];}
int fclose(FILE *f){int r;if(!f||f==&console)return -1;r=close(f->fd-1);f->fd=0;return r;}
size_t fread(void *p,size_t s,size_t n,FILE *f){int r;if(!s||n>(size_t)MAXINT/s||!f||f==&console)return 0;r=read(f->fd-1,p,s*n);return r<0?0:(size_t)r/s;}
size_t fwrite(const void *p,size_t s,size_t n,FILE *f){int r;if(!s||n>(size_t)MAXINT/s||!f)return 0;if(f==&console)return 0;r=write(f->fd-1,p,s*n);return r<0?0:(size_t)r/s;}
int fseek(FILE *f,long o,int w){return !f||f==&console||lseek(f->fd-1,o,w)<0?-1:0;}
long ftell(FILE *f){return !f||f==&console?-1:lseek(f->fd-1,0,SEEK_CUR);}
void setbuf(FILE *f,char *b){(void)f;(void)b;}
int fflush(FILE *f){(void)f;return 0;}
int getchar(void){return '\n';}

/* Bounded formatter covers formats used by the engine. No floating point. */
static void emit(char *dst,size_t cap,size_t *pos,char c){if(*pos+1<cap)dst[*pos]=c;(*pos)++;}
int vsnprintf(char *dst,size_t cap,const char *fmt,va_list args){
    size_t pos=0;
    while(*fmt){
        unsigned width=0,precision=0;int zero=0,have_precision=0,islong=0;
        char buf[32],spec;const char *s;unsigned len=0,base=10,v;int negative=0;
        if(*fmt!='%'){emit(dst,cap,&pos,*fmt++);continue;}fmt++;
        if(*fmt=='%'){emit(dst,cap,&pos,*fmt++);continue;}
        if(*fmt=='0'){zero=1;fmt++;}
        while(isdigit(*fmt)){width=width*10+(*fmt++-'0');if(width>1024)width=1024;}
        if(*fmt=='.'){have_precision=1;fmt++;while(isdigit(*fmt)){precision=precision*10+(*fmt++-'0');if(precision>1024)precision=1024;}}
        if(*fmt=='l'){islong=1;fmt++;}
        spec=*fmt;if(*fmt)fmt++;
        if(spec=='s'){
            s=va_arg(args,const char *);if(!s)s="(null)";
            len=strlen(s);if(have_precision&&len>precision)len=precision;
            while(width>len){emit(dst,cap,&pos,' ');width--;}
            while(len--)emit(dst,cap,&pos,*s++);
            continue;
        }
        if(spec=='c'){emit(dst,cap,&pos,(char)va_arg(args,int));continue;}
        if(spec=='p'){v=(unsigned)(size_t)va_arg(args,void *);base=16;}
        else if(spec=='x'||spec=='X'){v=islong?(unsigned)va_arg(args,unsigned long):va_arg(args,unsigned);base=16;}
        else if(spec=='u')v=islong?(unsigned)va_arg(args,unsigned long):va_arg(args,unsigned);
        else if(spec=='d'||spec=='i'){int value=islong?(int)va_arg(args,long):va_arg(args,int);negative=value<0;v=negative?0u-(unsigned)value:(unsigned)value;}
        else {emit(dst,cap,&pos,'?');continue;}
        do{buf[len++]="0123456789abcdef"[v%base];v/=base;}while(v);
        if(have_precision&&precision>width)width=precision;
        if(negative&&zero){emit(dst,cap,&pos,'-');negative=0;if(width)width--;}
        while(width>len+(unsigned)negative){emit(dst,cap,&pos,zero||have_precision?'0':' ');width--;}
        if(negative)emit(dst,cap,&pos,'-');
        while(len)emit(dst,cap,&pos,buf[--len]);
    }
    if(cap)dst[pos<cap?pos:cap-1]=0;
    return (int)pos;
}
int snprintf(char *p,size_t n,const char *fmt,...){va_list a;int r;va_start(a,fmt);r=vsnprintf(p,n,fmt,a);va_end(a);return r;}
int sprintf(char *p,const char *fmt,...){va_list a;int r;va_start(a,fmt);r=vsnprintf(p,(size_t)-1,fmt,a);va_end(a);return r;}
int printf(const char *fmt,...){char line[512];va_list a;int r;va_start(a,fmt);r=vsnprintf(line,sizeof(line),fmt,a);va_end(a);x7_log_text(line);return r;}
int fprintf(FILE *f,const char *fmt,...){char line[512];va_list a;int r;va_start(a,fmt);r=vsnprintf(line,sizeof(line),fmt,a);va_end(a);if(f==&console)x7_log_text(line);else fwrite(line,1,strlen(line),f);return r;}
int putchar(int c){char s[2]={(char)c,0};x7_log_text(s);return c;}
int puts(const char *s){x7_log_text(s);x7_log_text("\n");return 0;}
void x7_runtime_stats(void){x7_record("heap peak requested bytes",peak_bytes);x7_record("heap live requested bytes",live_bytes);x7_record("heap live allocation count",alloc_count);}
void x7_runtime_close(void){unsigned i;for(i=0;i<32;i++)if(occupied[i])close(i);while(allocations)free(allocations+1);}
