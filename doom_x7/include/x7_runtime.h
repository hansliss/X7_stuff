#ifndef DOOM_X7_RUNTIME_H
#define DOOM_X7_RUNTIME_H
#include <stddef.h>
#include <stdarg.h>
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0x200
#define O_TRUNC 0x400
#define O_BINARY 0
#define R_OK 4
#define F_OK 0
#define MAXINT 2147483647
#define MININT (-2147483647-1)
#define MAXLONG MAXINT
#define MINLONG MININT
#define MAXSHORT 32767
#define MINSHORT (-32768)
#define MAXCHAR 127
#define MINCHAR (-128)
typedef int off_t;
struct stat { int st_size; };
typedef struct { int fd; } FILE;
extern FILE *stdin, *stdout, *stderr;
void *malloc(size_t);
void *calloc(size_t,size_t);
void *realloc(void *,size_t);
void free(void *);
void *memcpy(void *,const void *,size_t);
void *memmove(void *,const void *,size_t);
void *memset(void *,int,size_t);
int memcmp(const void *,const void *,size_t);
size_t strlen(const char *);
char *strcpy(char *,const char *);
char *strncpy(char *,const char *,size_t);
char *strcat(char *,const char *);
int strcmp(const char *,const char *);
int strncmp(const char *,const char *,size_t);
int strcasecmp(const char *,const char *);
int strncasecmp(const char *,const char *,size_t);
int toupper(int);
int tolower(int);
int isdigit(int);
int isspace(int);
int abs(int);
int atoi(const char *);
int open(const char *,int,...);
int close(int);
int read(int,void *,unsigned);
int write(int,const void *,unsigned);
int lseek(int,int,int);
int fstat(int,struct stat *);
int access(const char *,int);
int mkdir(const char *,int);
FILE *fopen(const char *,const char *);
int fclose(FILE *);
size_t fread(void *,size_t,size_t,FILE *);
size_t fwrite(const void *,size_t,size_t,FILE *);
int fseek(FILE *,long,int);
long ftell(FILE *);
int printf(const char *,...);
int fprintf(FILE *,const char *,...);
int sprintf(char *,const char *,...);
int snprintf(char *,size_t,const char *,...);
int vsnprintf(char *,size_t,const char *,va_list);
int puts(const char *);
int putchar(int);
int getchar(void);
int fflush(FILE *);
void setbuf(FILE *,char *);
void exit(int) __attribute__((noreturn));
void x7_log_text(const char *);
int x7_record(const char *,unsigned long long);
void x7_runtime_close(void);
void x7_runtime_stats(void);
void x7_run_loop(void);
void x7_benchmark_done(void);
void x7_benchmark_begin(void);
void x7_loading_checkpoint(void);
#endif
