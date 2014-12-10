#include "init.h"
#include "elf.h"
#include "machine.h"
#include "fs.h"
#include "libk.h"

Init::Init() : Process("init",nullptr) {
}

long Init::run() {
    SimpleQueue<const char*> argv;
    argv.addTail(K::strdup("shell"));


    resources->addWorkingDir(FileSystem::rootfs->rootdir);
    //never delete root dir.
    Resource::ref(FileSystem::rootfs->rootdir);

    execv("shell",&argv,1);

    Debug::shutdown("What?");
    return 0;
}
