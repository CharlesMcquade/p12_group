#include "libc.h"
#include "stdint.h"

#define NUM_PROGS (6)
#define MAX_PROG_SZ (12)

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

/*	if(prog[0] == 'p' && prog[1] == 'w' && prog[2] == 'd' && prog[3] == 0) {
		putchar('\n');
		return -3;
	}*/
	if(prog[0] == 'c' && prog[1] == 'd' && prog[2] == 0) {
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

int main() {
	while (1) {
        puts("shell:");
        //pwd();
        puts("$ ");
        char* in = gets();
    	long idx = 0;
    	while(in[idx] == ' ') ++idx;
    	char* prog = malloc(sizeof(char) * MAX_PROG_SZ);
    	memset(prog, 0, 12);
    	long progIdx = 0;
    	while(in[idx] != ' ' && in[idx] != '\r' && in[idx] != 0) prog[progIdx++] = in[idx++];
    	long numArgs = checkProg(prog);
    	char** args = 0;
		if(numArgs == -4) {
			while(in[idx] != 0 && in[idx] != '\r' && in[idx] == ' ') ++idx;
			args = delimitBy((char*)&in[idx], '/');
			long result = cd(args);
			if(result == ERR_NOT_FOUND) puts("Error: directory not found.\n");
			goto done;
		}
		else if(numArgs == -1) {
    		if(prog[0] != 0)
    			notFound(prog);
    		goto done;
    	}
    	else if(numArgs == -3) {
    		goto done;
    	} else {
    		args = delimitBy((char*)&in[idx], ' ');
			long id = fork();
			if(id == 0) {
				execv(prog, args);
				exit(0);
			}
			else
				join(id);
		}
    	uint32_t argNum = 0;
    	if(args)
			for(argNum = 0; argNum < numArgs; argNum++) {
				if(args[argNum] != 0)
					free(args[argNum]);
				else break;
			}
		//freeing args & prog
		if(args) free(args);
done:
		if(prog) free(prog);
        if (in) free(in);
    }
    return 0;
}
