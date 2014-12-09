#include "libc.h"

int main(int argc, char** argv) {
	char* fileName = argv[1];
	char** filePath = &argv[2];

	long code = mkdir(fileName, filePath);
	//TODO: handle errors
	if(code) {
		switch(code) {
		case ERR_DEVWRITE : { puts("Error writing to device.\n"); break;}
		case ERR_NO_ROOT_SPACE : { puts("Error: Maximum allotted space for root directory reached.\n"); break;}
		case ERR_NOT_DIR : { puts(" Error: Is not a directory.\n"); break;}
		case ERR_NAMELEN : { puts("Error: File name is too long.\n"); break;}
		default : { puts("Error: "); putdec(code); puts(" \n"); break; }
		}
	}
	return 0;
}
