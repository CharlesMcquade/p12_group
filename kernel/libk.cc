#include "libk.h"
#include "debug.h"
#include "process.h"
#include "machine.h"

void K::bzero(void* p, size_t sz) {
    memset(p,0,sz);
}

long K::strlen(const char* str) {
    long n = 0;
    while (*str++ != 0) n++;
    return n;
}

int K::contains(const char* str, char item) {
	long n = 0;
	while(str[n] != 0 && str[n] != item) n++;
	if(str[n] == item)
		return 1;
	return 0;
}

//null terminate a string, return a new pointer to that string
char* K::strdup(const char* str) {
    if (str == nullptr) return nullptr;

    long len = strlen(str);
    /* how long is too long? */

    char* out = new char[len+1];
    memcpy(out,str,len);
    out[len] = 0;
    return out;
}
    
int K::isdigit(int c) {
    return (c >= '0') && (c <= '9');
}

template <typename T> T&& move(T& t) {
    return (T&&)t;
}

extern "C" void __cxa_pure_virtual() {
    Debug::panic("__cxa_pure_virtual called\n");
}
