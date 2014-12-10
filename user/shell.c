#include "libc.h"
#include "stdint.h"

#define NUM_PROGS (6)
#define MAX_PROG_SZ (12)


typedef struct
{
	struct dir* prevDir;
	uint32_t start; //start block
	char* currDir;
	struct dir* nextDir;
} dir;


dir* root;
dir* currDir;



char** delimitBy(char* str, char delimiter) {

}
void pwd() {
	puts("/");
	dir* thisDir = currDir;
	while(thisDir->prevDir != 0)
		thisDir = (dir*)thisDir->prevDir;

	if(thisDir) {
		char* directory = thisDir->currDir;
		if(directory)
			puts(directory);


		while(thisDir->nextDir != 0) {
			if(directory) puts("/");
			thisDir = (dir*)thisDir->nextDir;
			directory = thisDir->currDir;
			if(directory)
				puts(directory);
		}
	}
}

void notFound(char* cmd) {
    puts(cmd);
    puts(": command not found\n");
}


/*
 * Checks program and returns the number of arguments it accepts.
 * -1 => the program doesnt exist
 * 0 => the program doesn't accept arguments
 * 1...n => the program accepts n arguments
 * -2 => the program accepts a variable number of arguments
 * -3 => nothing needs to be done, already handled in checkProg
 * -4 => cd
 */
long checkProg(char* prog) {
	//long i = 0;

	if(prog[0] == 'p' && prog[1] == 'w' && prog[2] == 'd' && prog[3] == 0) {
		pwd();
		putchar('\n');
		return -3;
	}
	else if(prog[0] == 'c' && prog[1] == 'd' == prog[2] == 0) {
		return -4;
	}
	long fd = open(prog);
	if(fd <= 0) return -1;

	int magic = 0;
    readFully(fd,&magic,4);

    if (magic == 0x464c457f) {
        close(fd);
        return -2;
    } else {
        /* write it */
        seek(fd,0);
        char buf[100];
        while (1) {
            int n = read(fd,buf,100);
            if (n <= 0) break;
            for (int j=0; j<n; j++) {
                putchar(buf[j]);
            }
        }
        close(fd);
    }
	return -3;
}

void runProg(char* in) {
	long idx = 0;
	while(in[idx] == ' ') ++idx;
	char* prog = malloc(sizeof(char) * MAX_PROG_SZ);
	memset(prog, 0, 12);
	long progIdx = 0;
	while(in[idx] != ' ' && in[idx] != '\r' && in[idx] != 0) prog[progIdx++] = in[idx++];
	long numArgs = checkProg(prog);
	if(numArgs == -4) {
		while(in[idx] != 0 && in[idx] != '\r') ++idx;
		if(in[idx] == '.')
	}
	else if(numArgs == -1) {
		if(prog[0] != 0)
			notFound(prog);
		free(prog);
		return;
	}
	else if(numArgs == -3) {
		free(prog);
		return;
	}
	else {
		long varArg = 0;
		if(numArgs == -2) {
			numArgs = 1;
			varArg = 1;
		}
		char** args = malloc(sizeof(char*) * numArgs);
		memset(args, 0, sizeof(char*) * numArgs);
		long argNum = 0;
		char setArg = 0;
		long argIdx = 0;
		while((in[idx] != 0 && in[idx] != '\r')) {
			if(in[idx] != ' ') {
				if(!setArg) {
					if(argNum == numArgs) {
						if(!varArg)
							break;
						long newsz = numArgs + 2;
						char** temp = malloc(sizeof(char*) * newsz);
						memset(temp, 0, sizeof(char*) * newsz);
						memcpy(temp, args, sizeof(char*) * numArgs);
						numArgs = newsz;
						free(args);
						args = temp;
					}
					args[argNum] = malloc(sizeof(char) * MAX_PROG_SZ);
					memset(args[argNum], 0, sizeof(char) * MAX_PROG_SZ);
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
		execv(prog, args);

		//freeing pointers inside
		for(argNum = 0; argNum < numArgs; argNum++) {
			if(args[argNum] != 0)
				free(args[argNum]);
		}
		//freeing args & prog
		free(args);
		free(prog);
		return;
	}

}





int main() {

	root = malloc(sizeof(dir));
	root->prevDir = 0;
	root->nextDir = 0;
	//root is char* = 0;
	root->currDir = 0;

	currDir = root;


	while (1) {
        puts("shell:");
        pwd();
        puts("$ ");
        char* in = gets();
        long id = fork();
        if(id == 0) {
        	runProg(in);
        	exit(0);
        }
        else
        	join(id);
        if (in) free(in);
    }
    return 0;
}
