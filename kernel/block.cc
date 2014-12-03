#include "block.h"
#include "stdint.h"
#include "machine.h"

/***************/
/* BlockDevice */
/***************/

static uint32_t min(uint32_t a, uint32_t b) {
    if (a < b) return a; else return b;
}

uint32_t BlockDevice::read(uint32_t offset, void* buf, uint32_t n) {
    uint32_t sector = offset/blockSize;
    char *data = new char[blockSize];
    readBlock(sector,data);
    uint32_t dataOffset = offset - (sector * blockSize);
    uint32_t m = min(n,blockSize-dataOffset);
    memcpy(buf,&data[dataOffset],m);
    delete[] data;
    return m;  
}

uint32_t BlockDevice::write(uint32_t offset, void* buf, uint32_t n) {
    uint32_t sector = offset/blockSize;
    char *data = new char[blockSize];
    readBlock(sector,data);
    uint32_t dataOffset = offset - (sector * blockSize);
    //need to write from offset to n.
    uint32_t m = min(n,blockSize-dataOffset);
    memcpy(&data[dataOffset], buf,m);
    writeBlock(sector, data);
    delete[] data;
    return m;
}

void BlockDevice::readFully(uint32_t offset, void* buf, uint32_t n) {
    uint32_t togo = n;
    char* ptr = (char*) buf;

    while (togo > 0) {
        uint32_t c = read(offset,ptr,togo);
        togo -= c;
        ptr += c;
        offset += c;
    }
}

void BlockDevice::writeFully(uint32_t offset, void* buf, uint32_t n) {
    uint32_t togo = n;
    char* ptr = (char*) buf;

    while (togo > 0) {
        uint32_t c = write(offset,ptr,togo);
        togo -= c;
        ptr += c;
        offset += c;
    }
}

