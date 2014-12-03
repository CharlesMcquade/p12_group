#include "fs.h"
#include "machine.h"
#include "process.h"
#include "stdint.h"
#include "err.h"

/**************/
/* FileSystem */
/**************/

FileSystem* FileSystem::rootfs = 0;

void FileSystem::init(FileSystem* rfs) {
    rootfs = rfs;
}

/*************************/
/* Fat439 implementation */
/*************************/

static uint32_t min(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}

/* An open Fat439 file, one per file, per system */
class OpenFile : public Resource {
public:
    uint32_t start;
    struct {
        uint32_t type;
        uint32_t length;
    } metaData;
    Fat439 *fs;

    OpenFile(Fat439 *fs, uint32_t start) : Resource(ResourceType::OTHER) {
        this->start = start;
        this->fs = fs;
        fs->dev->readFully(start * 512, &metaData, sizeof(metaData));
    }

    virtual ~OpenFile() {
    }

    uint32_t getLength() { return  metaData.length; }
    uint32_t getType() { return metaData.type; }

    int32_t read(uint32_t offset, void* buf, uint32_t length) {
        if (offset > metaData.length) {
            return ERR_TOO_LONG;
        }
        uint32_t len = min(length,metaData.length - offset);

        uint32_t actualOffset = offset + 8;
        uint32_t blockInFile = actualOffset / 512;
        uint32_t offsetInBlock = actualOffset % 512;
        /*Debug::printf("actualOffset = %d\n blockInFile = %d\n offsetInBlock = %d\n",
        		actualOffset, blockInFile, offsetInBlock);*/

        uint32_t blockNumber = start;

        for (uint32_t i=0; i<blockInFile; i++) {
            blockNumber = fs->fat[blockNumber];
        }

        int32_t count = fs->dev->read(blockNumber * 512 + offsetInBlock, buf, len);
        return count;
    }

    int32_t readFully(uint32_t offset, void* buf, uint32_t length) {
        char* p = (char*) buf;
        uint32_t togo = length;
        while (togo) {
            int32_t cnt = read(offset,p,togo);
            if (cnt < 0) return cnt;
            if (cnt == 0) return length - togo;
            p += cnt;
            togo -= cnt;
        }
        return length;
    }
};

class Fat439File : public File {
public:
    OpenFile *openFile;

    Fat439File(OpenFile* openFile) : File(openFile->fs) {
        this->openFile = openFile;
    }

    virtual ~Fat439File() {
        openFile->fs->closeFile(openFile);
    }

    virtual Fat439File* forkMe() {
        Resource::ref(openFile);
        Fat439File *other = new Fat439File(openFile);
        other->offset = offset;
        other->count.set(count.get());
        return other;
    }

    virtual uint32_t getLength() { return openFile->getLength(); }
    virtual uint32_t getType() { return openFile->getType(); }
    virtual int32_t read(void* buf, uint32_t length) {
        long cnt = openFile->read(offset,buf,length);
        if (cnt > 0) offset += cnt;
        return cnt;
    }
};

class Fat439Directory : public Directory {
    uint32_t start;
    OpenFile *content;
    uint32_t entries;
    Mutex mutex;
public:
    Fat439Directory(Fat439* fs, uint32_t start) : Directory(fs), start(start) {
        content = fs->openFile(start);
        entries = content->getLength() / 16;        
    }

    void refresh() {
    	Fat439* rootfs = ((Fat439*)FileSystem::rootfs);
    	uint32_t blockSize = rootfs->dev->blockSize;
        //Debug::printf("OpenFile metaData content length before = %d\n)", content->metaData.length);
    	rootfs->dev->read(start * blockSize, &content->metaData, sizeof(content->metaData));
       // Debug::printf("OpenFile metaData content length after = %d\n)", content->metaData.length);
    	entries = content->getLength() / 16;
    }

    void addEntry() {
    	Fat439* rootfs = ((Fat439*)FileSystem::rootfs);
    	uint32_t blockSize = rootfs->dev->blockSize;
    	struct {
    		uint32_t type;
    		uint32_t length;
    	} metaData;
    	metaData.type = TYPE_DIR;
    	metaData.length += DIR_ENTRY_SZ;
    	rootfs->dev->write(start * blockSize, &metaData, sizeof(metaData));
    	refresh();
    }


    long mkdir(const char* name, Directory* parentDir) {

    	//Debug::printf("in mkdir..\n");
    	Fat439* rootfs = ((Fat439*)FileSystem::rootfs);
    	uint32_t blockSize = rootfs->dev->blockSize;

    	uint32_t blockToUse = rootfs->remNextAvail();
    	if(!blockToUse)
    		return ERR_NO_SPACE;
    	else {
    	    struct {
    	        uint32_t type;
    	        uint32_t length;
    	    } metaData;
		    metaData.type = TYPE_DIR;
		    metaData.length = 0;
    	    rootfs->dev->write(blockToUse * blockSize, &metaData, sizeof(metaData));
    	}



    	if(((Fat439Directory*)parentDir)->content->getType() != TYPE_DIR)
    		return ERR_NOT_DIR;

    	//Debug::printf("    parentDir is a Directory\n");


    	uint32_t offsetInBlk = ((Fat439Directory*)parentDir)->content->getLength() + HEADER_SZ;
    	uint32_t prevBlk = 0;
    	uint32_t blkToGet = start;
    	/*Debug::printf("    Seeking...\n");
    	Debug::printf("        offsetInBlk = %d\n "
    			"        prevBlk = %d\n "
    			"        blkToGet = %d\n", offsetInBlk, prevBlk, blkToGet);*/
    	while(offsetInBlk >= FileSystem::rootfs->dev->blockSize && blkToGet != 0) {
    		offsetInBlk -= FileSystem::rootfs->dev->blockSize;
    		prevBlk = blkToGet;
    		blkToGet = rootfs->nextBlk(blkToGet);
        	/*Debug::printf("        offsetInBlk = %d\n "
        			"        prevBlk = %d\n "
        			"        blkToGet = %d\n", offsetInBlk, prevBlk, blkToGet);*/
    	} if(blkToGet == 0) {
    		//Debug::printf(" Reached end of directory, need to find a new place...\n");
    		uint32_t newBlk = rootfs->remNextAvail();
    		rootfs->setNextBlk(prevBlk, newBlk);
    		blkToGet = newBlk;
    	}

    	struct{
    		char name[12];
    		uint32_t start;
    	}entry;
    	uint32_t ni = 0;
    	while(name[ni] != 0 && ni < 12) {
    		entry.name[ni] = name[ni];
    		++ni;
    	} if(ni < 12) entry.name[ni] = 0;
    	entry.start = blockToUse;

    	//Debug::printf("    entry.name = %s .... entry.start = %d \n", entry.name, entry.start);
    	uint32_t amWritten = rootfs->dev->write(blkToGet * blockSize + offsetInBlk, &entry, sizeof(entry));
/*    	Debug::printf("    wrote amWritten = %d, blkToGet + offsetInBlk = %d, sizeof(entry) = %d\n",
    			amWritten, blkToGet + offsetInBlk, sizeof(entry));*/
    	if(amWritten < sizeof(entry)) {
    		//Debug::printf("    need to write in a new block... \n");
    		prevBlk = blkToGet;
    		uint32_t nextBlkToGet = rootfs->remNextAvail();
    		if(!nextBlkToGet)
    			return ERR_NO_SPACE;
    		rootfs->setNextBlk(prevBlk, nextBlkToGet);
    		rootfs->dev->write(nextBlkToGet * blockSize,
    				(void*)((uint32_t*)&entry + amWritten/sizeof(uint32_t*)),
    				sizeof(entry) - amWritten);
    	}

    	/*Debug::printf("    double checking the name and stuff were copied correctly...\n");
    	entry.name[0] = 'T';
    	entry.name[1] = 'F';
    	entry.start = 0;
    	rootfs->dev->read(blkToGet * blockSize + offsetInBlk, &entry, sizeof(entry));
    	Debug::printf("        entry.name = %s\n"
    			      "        entry.start = %d\n", entry.name, entry.start);

    	Debug::printf("    done\n");*/

    	addEntry();
    	return 0;
    }

    uint32_t lookup(const char* name) {
        if ((name[0] == '.') && (name[1] == 0)) {
            return start;
        }
        struct {
            char name[12];
            uint32_t start;
        } entry;
        mutex.lock();
        uint32_t offset = 0;
        for (uint32_t i=0; i<entries; i++) {
            content->readFully(offset,&entry, 16);
            offset += 16;
            for (int i=0; i<12; i++) {
                char x = entry.name[i];
                if (x != name[i]) break;
                if (x == 0) {
                    mutex.unlock();
                    return entry.start;
                }
            }
        }
        mutex.unlock();
        return 0;
    }

    void listFiles() {
    	struct {
			char name[12];
			uint32_t start;
		} entry;
		mutex.lock();
		uint32_t offset = 0;
		for (uint32_t i=0; i<entries; i++) {
			content->readFully(offset,&entry, 16);
			offset += 16;
			Debug::printf("%s\n", entry.name);
		}
		mutex.unlock();
    }

    File* lookupFile(const char* name) {
        uint32_t idx = lookup(name);
        if (idx == 0) return nullptr;

        return new Fat439File(content->fs->openFile(idx));
    }

    Directory* lookupDirectory(const char* name) {
        uint32_t idx = lookup(name);
        if (idx == 0) return nullptr;

        return new Fat439Directory(content->fs,idx);
    }
};

Fat439::Fat439(BlockDevice *dev) : FileSystem(dev) {
    dev->readFully(0, &super, sizeof(super));
    const uint32_t expectedMagic = 0x39333446;
    uint32_t magic;
    memcpy(&magic,super.magic,4);
    if (magic != 0x39333446) {
        Debug::panic("bad magic %x != %x",magic, expectedMagic);
    }

    fat = new uint32_t[super.nBlocks];
    openFiles = new OpenFilePtr[super.nBlocks]();
    dev->readFully(512,fat,super.nBlocks * sizeof(uint32_t));
    rootdir = new Fat439Directory(this,super.root);
}

OpenFile* Fat439::openFile(uint32_t start) {
    openFilesMutex.lock();
    OpenFile* p = openFiles[start];
    if (p == nullptr) {
        p = new OpenFile(this,start);
        openFiles[start] = p;
    }
    Resource::ref(p);
    openFilesMutex.unlock();
    return p;
}


void Fat439::closeFile(OpenFile* p) {
    openFilesMutex.lock();
    uint32_t start = p->start;
    openFiles[start] = (OpenFile*) Resource::unref(openFiles[start]);
    openFilesMutex.unlock();
}

uint32_t Fat439::nextAvailBlk() {
	return super.avail;
}

uint32_t Fat439::remNextAvail() {
	uint32_t nextAvail = super.avail;
	if(nextAvail == 0)
		return 0;
	super.avail = fat[super.avail];
	fat[super.avail] = 0;
	return nextAvail;
}

uint32_t Fat439::nextBlk(uint32_t blk) {
	return fat[blk];
}

void Fat439::setNextBlk(uint32_t blk, uint32_t nextBlk) {
	fat[blk] = fat[nextBlk];
}
