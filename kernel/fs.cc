#include "fs.h"
#include "machine.h"
#include "process.h"
#include "stdint.h"
#include "err.h"
#include "libk.h"

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
        uint32_t parentDir;
        uint32_t currentBlk;
    } metaData;
    Fat439 *fs;

    OpenFile(Fat439 *fs, uint32_t start) : Resource(ResourceType::OTHER) {
        this->start = start;
        this->fs = fs;
        fs->dev->readFully(start * BLOCK_SZ, &metaData, sizeof(metaData));
    }

    virtual ~OpenFile() {
    }

    uint32_t getParentBlk() { return metaData.parentDir; }
    uint32_t getCurrBlk() { return metaData.currentBlk; }
    uint32_t getLength() { return  metaData.length; }
    uint32_t getType() { return metaData.type; }

    int32_t read(uint32_t offset, void* buf, uint32_t length) {
        if (offset > metaData.length) {
            return ERR_TOO_LONG;
        }
        uint32_t len = min(length,metaData.length - offset);

        uint32_t actualOffset = offset + HEADER_SZ;
        uint32_t blockInFile = actualOffset / BLOCK_SZ;
        uint32_t offsetInBlock = actualOffset % BLOCK_SZ;
        /*Debug::printf("actualOffset = %d\n blockInFile = %d\n offsetInBlock = %d\n",
        		actualOffset, blockInFile, offsetInBlock);*/

        uint32_t blockNumber = start;

        for (uint32_t i=0; i<blockInFile; i++) {
            blockNumber = fs->fat[blockNumber];
        }

        int32_t count = fs->dev->read(blockNumber * BLOCK_SZ + offsetInBlock, buf, len);
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

    int32_t writeFully(uint32_t offset, void* buf, uint32_t length) {
        char* p = (char*) buf;
        uint32_t togo = length;
        while (togo) {
            int32_t cnt = write(offset,p,togo);
            if (cnt < 0) return cnt;
            if (cnt == 0) return length - togo;
            p += cnt;
            togo -= cnt;
        }
        return length;
    }

    void resize(uint32_t newsz) {
    	metaData.length = newsz;
    	fs->dev->write(start * BLOCK_SZ, &metaData, sizeof(metaData));
    }


    long remFromOffset(long entryOffset, uint32_t numBytes) {
    	uint32_t fileSz = metaData.length;
    	if(numBytes > fileSz) {
    		resize(0);
    		fs->openFilesMutex.unlock();
    		return 0;
    	} else if(numBytes + entryOffset >= getLength()) {
    		resize(entryOffset);
    		fs->openFilesMutex.unlock();
    		return 0;
    	}
    	uint32_t newsz = fileSz- numBytes;
    	Debug::printf("offset = %d\nlen = %d\nfileSz = %s\nnewsz = %d\nstart* BLOCK_SZ + offset + len = %d\n start * BLOCK_SZ * offset = %d\n",
    			entryOffset, numBytes, fileSz, newsz, start * BLOCK_SZ + entryOffset + numBytes, start * BLOCK_SZ * entryOffset);
    	char* buf = new char[newsz - entryOffset];
    	fs->dev->readFully(start * BLOCK_SZ + entryOffset + numBytes, buf, fileSz - (entryOffset + numBytes));
    	fs->dev->writeFully(start * BLOCK_SZ + entryOffset, buf, fileSz - (entryOffset + numBytes));
    	resize(newsz);

		return 0;
    }


    int32_t append(void* buf, uint32_t length) {
    	return write(metaData.length, buf, length);
	}

	int32_t write(uint32_t offset, void* buf, uint32_t length) {
		//Debug::printf("in mutex lock\n");
		uint32_t actualOffset = offset + HEADER_SZ;
		uint32_t len = min(length, fs->dev->blockSize - actualOffset);
		uint32_t blockInFile = actualOffset / BLOCK_SZ;
		uint32_t offsetInBlock = actualOffset % BLOCK_SZ;

		uint32_t blockNumber = start;

		for(uint32_t i = 0; i<blockInFile; i++) {
			blockNumber = fs->fat[blockNumber];
		}

		//Debug::printf("block to write at = %d, offset in block = %d\n", blockNumber, offsetInBlock);
		uint32_t totalWritten = 0;
		uint32_t written = fs->dev->write(blockNumber * fs->dev->blockSize + offsetInBlock, buf, len);
		totalWritten += written;
		length -= written;
		//if length != 0, need to go into a new block
		while(length > 0) {
			uint32_t nextBlk = fs->fat[blockNumber];
			if(!nextBlk) {
				nextBlk = fs->remNextAvail();
				fs->setNextBlk(blockNumber, nextBlk);
			}
			if(!nextBlk) { //resize and return amount written if no more space
				resize(metaData.length + totalWritten);
				fs->openFilesMutex.unlock();
				return ERR_NO_SPACE;
			}
			blockNumber = nextBlk;
			char* byteBuf = (char*)buf;
			len = min(length, fs->dev->blockSize);
			written = fs->dev->write(blockNumber * fs->dev->blockSize, &byteBuf[totalWritten], len);
			length -= written;
			totalWritten += written;
		}
		resize(metaData.length + totalWritten);
		//Debug::printf("unlocked mutex\n");
		return 0;
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
        entries = content->getLength() / DIR_ENTRY_SZ;
    }

    void refresh() {
    	entries = content->getLength() / DIR_ENTRY_SZ;
    }


    Directory* getPrevDirectory() {
    	uint32_t idx = 0;
    	fs->dev->read(start + 2*sizeof(uint32_t), &idx, sizeof(uint32_t));
    	if(idx == 0)
    		return this;
    	return new Fat439Directory((Fat439*)fs, idx);
    }

    long remove(const char* name) {
    	uint32_t blk = lookup(name);
    	if(!blk) return ERR_NOT_FOUND;
    	OpenFile* obj = ((Fat439*)fs)->openFile(blk);
    	if(obj->getType() == TYPE_DIR) {
    		if(obj->getLength() != 0)
    			return ERR_IS_DIR;
    	}
    	long entryOffset = lookupEntry(name) * DIR_ENTRY_SZ + HEADER_SZ;
    	if(entryOffset < 0) return ERR_NOT_POSSIBLE;
    	Debug::printf("About to call remFromOffset %s:%d\n", __FILE__, __LINE__);
    	long code = content->remFromOffset(entryOffset, DIR_ENTRY_SZ);
    	if(code >= 0) {
    	Debug::printf("Here\n");
    	obj->resize(0);
    	((Fat439*)fs)->addAvailBlk(blk);
    	fs->sync();
    	refresh();
    	}
    	return code;
    }

    long mkdir(const char* name) {

    	if(((Fat439Directory*)this)->content->getType() != TYPE_DIR) return ERR_NOT_DIR;
    	if(K::strlen(name) >= 12) return ERR_FL_NAMELEN;
    	if(K::contains(name, '/')) return ERR_INVL_CHAR;
    	if(lookup(name))return ERR_FL_EXIST;


    	Fat439* rootfs = ((Fat439*)FileSystem::rootfs);
    	uint32_t blockSize = rootfs->dev->blockSize;

    	if(start == rootfs->super.root && (content->getLength() + HEADER_SZ + DIR_ENTRY_SZ >= rootfs->dev->blockSize))
    		return ERR_NO_ROOT_SPACE;

    	uint32_t blockToUse = rootfs->remNextAvail();
    	if(!blockToUse)
    		return ERR_NO_SPACE;
    	else {
    	    struct {
    	        uint32_t type;
    	        uint32_t length;
    	        uint32_t parentDir;
    	        uint32_t currentDir;
    	    } metaData;


		    metaData.type = TYPE_DIR;
		    metaData.length = 0;
		    metaData.parentDir = this->start;
		    metaData.currentDir = blockToUse;
    	    rootfs->dev->write(blockToUse * blockSize, &metaData, sizeof(metaData));
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

    	uint32_t success = content->append(&entry, sizeof(entry));
    	if(success != 0) {
    		return ERR_DEVWRITE;
    	}
    	entries += 1;
    	return 0;
    }

    uint32_t lookup(const char* name) {
        if ((name[0] == '.') && (name[1] == 0)) {
            return start;
        }
        if( name[0] == '.' && name[1] == '.' && name [2] == 0)
        	return content->getParentBlk();
        struct {
            char name[12];
            uint32_t start;
        } entry;
        mutex.lock();
        uint32_t offset = 0;
        for (uint32_t i=0; i<entries; i++) {
            content->readFully(offset,&entry, DIR_ENTRY_SZ);
            offset += DIR_ENTRY_SZ;
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

    long lookupEntry(const char* name) {
        if ((name[0] == '.') && (name[1] == 0)) {
            return 12; //0 = type, 4 = length, 8 = parent... 12 = current
        }
        if( name[0] == '.' && name[1] == '.' && name [2] == 0)
        	return 8; //parent location offset in block
        struct {
            char name[12];
            uint32_t start;
        } entry;
        mutex.lock();
        uint32_t offset = 0;
        for (uint32_t i=0; i<entries; i++) {
            content->readFully(offset,&entry, DIR_ENTRY_SZ);
            for (int i=0; i<12; i++) {
                char x = entry.name[i];
                if (x != name[i]) break;
                if (x == 0) {
                    mutex.unlock();
                    return i;
                }
            }
            offset += DIR_ENTRY_SZ;
        }
        mutex.unlock();
        return -1;
    }

    void listFiles() {
    	struct {
			char name[12];
			uint32_t start;
		} entry;
		mutex.lock();
		uint32_t offset = 0;
		for (uint32_t i=0; i<entries; i++) {
			content->readFully(offset,&entry, DIR_ENTRY_SZ);
			offset += DIR_ENTRY_SZ;
			Debug::printf("%s\n", entry.name);
		}
		mutex.unlock();
    }

    File* lookupFile(const char* name) {
    	if(!name) return nullptr;
    	uint32_t idx = lookup(name);
        if (idx == 0) return nullptr;

        return new Fat439File(content->fs->openFile(idx));
    }

    Directory* lookupDirectory(const char* name) {
        if(!name) return nullptr;
		uint32_t idx = lookup(name);
        if (idx == 0) return nullptr;

        return new Fat439Directory(content->fs,idx);
    }

    Directory* lookupDirectory(const char** path) {
    	uint32_t pathNum = 0;
    	uint32_t idx = 0;
    	if(path)
    		if(path[pathNum])
				do {
					idx = lookup(path[pathNum]);
					if(idx) ++pathNum;
				} while((long)path[pathNum] && idx > 0);
			if(idx)
    		return new Fat439Directory(content->fs, idx);
    	return nullptr;
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
    dev->readFully(BLOCK_SZ,fat,super.nBlocks * sizeof(uint32_t));
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

void Fat439::addAvailBlk(uint32_t newAvail) {
	uint32_t firstInBlockSeries = newAvail;
	//traverse to end of block tree
	while(fat[newAvail]) newAvail = fat[newAvail];
	//make first avail in super end of that chain
	fat[newAvail] = super.avail;
	//make super avail the first in that series.
	super.avail = firstInBlockSeries;
}

uint32_t Fat439::remNextAvail() {
	uint32_t nextAvail = super.avail;
	if(nextAvail == 0)
		return 0;
	super.avail = fat[super.avail];
	fat[nextAvail] = 0;
	return nextAvail;
}

uint32_t Fat439::nextBlk(uint32_t blk) {
	return fat[blk];
}

void Fat439::setNextBlk(uint32_t blk, uint32_t nextBlk) {
	fat[blk] = fat[nextBlk];
}


void Fat439::syncSuper() {
   dev->write(0, &super, sizeof(super));
}

void Fat439::syncFAT() {
	dev->writeFully(BLOCK_SZ,fat,super.nBlocks * sizeof(uint32_t));
}




