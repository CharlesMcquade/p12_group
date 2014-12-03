#include "libc.h"

int main(int argc, char** argv) {
    /*
     * p9 test 1 has code that will do this
     */

	long fNum = 1;

	while(fNum < argc) {
		long file = open(argv[fNum]);
		if(file <= 0) {
			puts("cat: "); puts(argv[fNum]); puts(": No such file or directory\n");
		}
		else {
			char buf[100];

			while(1) {
				long n = read(file,buf,100);
				if (n <= 0) break;
				for (long i=0; i<n; i++) {
					putchar(buf[i]);
				}
			}
		}
	++fNum;
	}

	return 0;
}

/*
 * argv[0] = fileName
 * argv[1] = file
 * if argc != 2, don't care, only use argv[1]
 *
 *
 * argv is at 8(%esp), *argv is elsewhere, and argv[1] is contiguous
 */
