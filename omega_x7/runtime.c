#include <stddef.h>
void *memset(void *dst,int value,size_t n){unsigned char *p=dst;while(n--)*p++=(unsigned char)value;return dst;}
void *memcpy(void *dst,const void *src,size_t n){unsigned char *p=dst;const unsigned char *q=src;while(n--)*p++=*q++;return dst;}
