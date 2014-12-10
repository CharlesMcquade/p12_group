#include "libc.h"
#include "sys.h"

static char hexDigits[] = { '0', '1', '2', '3', '4', '5', '6', '7',
                            '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

void puts(char* p) {
    char c;
    while ((c = *p++) != 0) putchar(c); 
}

long strlen(char* str) {
	char c;
	long len = 0;
	while((c = *str++) != 0) len++;
	return len;
}

char* gets() {
    long sz = 0;
    char *p = 0;
    long i = 0;

    while (1) {
        if (i >= sz) {
            sz += 10;
            p = realloc(p,sz+1);
            if (p == 0) return 0;
        }
        char c = getchar();
        putchar(c);
        if (c == 13) {
            puts("\n");
            p[i] = 0;
            return p;
        }
        p[i++] = c;        
    }
}
        
void puthex(long v) {
    for (int i=0; i<sizeof(long)*2; i++) {
          char c = hexDigits[(v & 0xf0000000) >> 28];
          putchar(c);
          v = v << 4;
    }
}

void putdec(unsigned long v) {
    if (v >= 10) {
        putdec(v / 10);
    }
    putchar(hexDigits[v % 10]);
}

long readFully(long fd, void* buf, long length) {
    long togo = length;
    char* p = (char*) buf;
    while(togo) {
        long cnt = read(fd,p,togo);
        if (cnt < 0) return cnt;
        if (cnt == 0) return length - togo;
        p += cnt;
        togo -= cnt;
    }
    return length;
}



char** delimitBy(char* in, char delimiter) {
	long idx = 0;
	long numArgs = 1;
	char** args = malloc(sizeof(char*) * numArgs);
	memset(args, 0, sizeof(char*) * numArgs);
	long argNum = 0;
	char setArg = 0;
	long argIdx = 0;
	while((in[idx] != 0 && in[idx] != '\r')) {
		if(in[idx] != delimiter) {
			if(!setArg) {
				if(argNum == numArgs) {
					long newsz = numArgs + 2;
					char** temp = malloc(sizeof(char*) * newsz);
					memset(temp, 0, sizeof(char*) * newsz);
					memcpy(temp, args, sizeof(char*) * numArgs);
					numArgs = newsz;
					free(args);
					args = temp;
				}
				args[argNum] = malloc(sizeof(char) * 12);
				memset(args[argNum], 0, sizeof(char) * 12);
				setArg = 1;
			}
			args[argNum][argIdx++] = in[idx];
		}
		else {
			if(setArg) {
				setArg = 0;
				argIdx = 0;
				++argNum;
			}
		}
		++idx;
	}
	return args;
}
