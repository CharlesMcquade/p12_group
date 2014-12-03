#include "libc.h"
#include "stdint.h"

#define NUM_PROGS (4)
#define MAX_PROG_SZ (12)


typedef struct
{
	struct dir* prevDir;
	char* currDir;
	struct dir* nextDir;
} dir;

dir* root;
dir* currDir;


typedef struct
{
  unsigned char	e_ident[16];	/* Magic number and other info */
  uint16_t	e_type;			/* Object file type */
  uint16_t	e_machine;		/* Architecture */
  uint32_t	e_version;		/* Object file version */
  uint32_t	e_entry;		/* Entry point virtual address */
  uint32_t	e_phoff;		/* Program header table file offset */
  uint32_t	e_shoff;		/* Section header table file offset */
  uint32_t	e_flags;		/* Processor-specific flags */
  uint16_t	e_ehsize;		/* ELF header size in bytes */
  uint16_t	e_phentsize;		/* Program header table entry size */
  uint16_t	e_phnum;		/* Program header table entry count */
  uint16_t	e_shentsize;		/* Section header table entry size */
  uint16_t	e_shnum;		/* Section header table entry count */
  uint16_t	e_shstrndx;		/* Section header string table index */
} Elf32_Ehdr;

void notFound(char* cmd) {
    puts(cmd);
    puts(": command not found\n");
}

long matches(char* prog, char* allProgs) {
	long i = 0;
	while(i < MAX_PROG_SZ && allProgs[i] != 0) {
		if(prog[i] != allProgs[i])
			return 0;
		++i;
	}
	if(prog[i] != allProgs[i])
		return 0;
	return 1;
}

long isElf(Elf32_Ehdr hdr) {
	return hdr.e_ident[0] == 0x7f && hdr.e_ident[1] == 'E' &&
			hdr.e_ident[2] == 'L' && hdr.e_ident[3] == 'F';
}
/*
 * Checks program and returns the number of arguments it accepts.
 * -1 => the program doesnt exist
 * 0 => the program doesn't accept arguments
 * 1...n => the program accepts n arguments
 * -2 => the program accepts a variable number of arguments
 * -3 => type isn't elf, CAT it
 */
long checkProg(char* prog, char** allProgs) {
	long i = 0;
	while(!matches(prog, allProgs[i]) && i < NUM_PROGS) ++i;
	if(i == NUM_PROGS) {
		long file = open(prog);
		if(file <= 0) return -1;
		Elf32_Ehdr hdr;
		readFully(file, &hdr, sizeof(Elf32_Ehdr));
		if(isElf(hdr))
			return -2;
		else
			return -3;
	} else {
		switch(i)
		{
		case 0 : return 0; // ls, shutdown: no arguments accepted
		default :  return -2; //variable args: cat, unknown, etc.
		}
		return -1; //prog doesn't exist
	}
}

void runProg(char* in, char** allProgs) {
	long idx = 0;
	while(in[idx] == ' ') ++idx;
	char* prog = malloc(sizeof(char) * MAX_PROG_SZ);
	memset(prog, 0, 12);
	long progIdx = 0;
	while(in[idx] != ' ' && in[idx] != '\r' && in[idx] != 0) prog[progIdx++] = in[idx++];
	long numArgs = checkProg(prog, allProgs);
	if(numArgs == -1) {
		if(prog[0] != 0)
			notFound(prog);
		free(prog);
		return;
	}
	else if(numArgs == 0) {
		execv(prog, 0);
		free(prog);
		return;
	}
	else {
		long varArg = 0;
		long makeCat = 0;
		if(numArgs == -2 || numArgs == -3) {
			if(numArgs == -3)
				makeCat = 1;
			numArgs = 10;
			varArg = 1;
		}
		char** args = malloc(sizeof(char*) * numArgs);
		memset(args, 0, sizeof(char*) * numArgs);
		long argNum = 0;
		if(makeCat) {
			args[argNum] = prog;
			++argNum;
			execv("cat", args);
		} else {
			char setArg = 0;
			long argIdx = 0;
			while((in[idx] != 0 && in[idx] != '\r')) {
				if(in[idx] != ' ') {
					if(!setArg) {
						if(argNum == numArgs) {
							if(!varArg)
								break;
							long newsz = numArgs + 10;
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
		}

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



void pwd(dir* currd) {
	puts("/");
	dir* thisDir = currd;
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

int main() {

	root = malloc(sizeof(dir));
	root->prevDir = 0;
	root->nextDir = 0;
	//root is char* = 0;
	root->currDir = 0;

	currDir = root;

	//debug code
	/*char** stoo = malloc(sizeof(char*) * 4);
	stoo[0] = "directory";
	stoo[1] = "hierarchy";
	stoo[2] = "systems";
	stoo[3] = "rock";

	dir* thisDir = currDir;
	long i;
	for(i = 0; i < 4; i++) {
		thisDir->nextDir = malloc(sizeof(dir));
		thisDir = (dir*)thisDir->nextDir;
		thisDir->currDir = stoo[i];
	}*/


	char** allProgs = malloc(sizeof(char*) * 6);
	allProgs[0] = "ls";
	allProgs[1] = "echo";
	allProgs[2] = "cat";
	allProgs[3] = "shutdown";
	allProgs[4] = "cd";
	allProgs[5] = "mkdir";

	while (1) {
        puts("shell:");
        pwd(currDir);
        puts("$ ");
        char* in = gets();
        long id = fork();
        if(id == 0) {
        	runProg(in, allProgs);
        	exit(0);
        }
        else
        	join(id);
        if (in) free(in);
    }
	free(allProgs);
    return 0;
}
