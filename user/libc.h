#ifndef _LIBC_H_
#define _LIBC_H_

#include "sys.h"
#include "../kernel/err.h"


long strlen(char* str);
char** delimitBy(char* str, char delimiter);
extern void puts(char *p);
extern char* gets();
extern void* malloc(long size);
extern void free(void*);
extern void* realloc(void* ptr, long newSize);
extern void putdec(unsigned long v);
extern void puthex(long v);
extern long readFully(long fd, void* buf, long length);


void memset(void* p, int val, long sz);
void memcpy(void* dest, void* src, long n);

#endif
