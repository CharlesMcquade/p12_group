#include "libc.h"

int main(int argc, char** argv) {
	char* fileName = argv[1];
	char** filePath = &argv[2];

	long success = mkdir(fileName, filePath);
	return success;
}
