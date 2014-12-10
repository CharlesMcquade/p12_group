#include "resource.h"
#include "err.h"
#include "process.h"

Table::Table(long n) : Resource(ResourceType::TABLE),
    n(n), array(new ResourcePtr[n]())
{
}

Table::~Table() {
    for (long i=0; i<n; i++) {
        Resource::unref(array[i]);
        array[i] = nullptr;
    }
    delete []array;
    array = nullptr;
}

long Table::addWorkingDir(Resource* p) {
	mutex.lock();
	if(p->type != ResourceType::DIRECTORY) {
		mutex.unlock();
		return ERR_NOT_DIR;
	}
	if(array[1] != nullptr)
		Resource::unref(array[1]);
	array[1] = Resource::ref(p);
	mutex.unlock();
	return 0;
}

long Table::closeWorkingDir() {
	mutex.lock();
		if(array[1]->type != ResourceType::DIRECTORY) {
			mutex.unlock();
			return ERR_NOT_DIR;
		}
		Resource::unref(array[1]);
		array[1] = nullptr;
		mutex.unlock();
	return 0;
}

long Table::open(Resource* p) {
    long i;
    mutex.lock();
    for (i=2; i<n; i++) {
        if (array[i] == nullptr) {
            array[i] = Resource::ref(p);
            goto done;
        }
    }
    i = ERR_NO_ID;
done:
    mutex.unlock();
    return i;
}

long Table::close(long i) {
    if (i < 0) return ERR_INVALID_ID;
    if (i >= n) return ERR_INVALID_ID;
    if (i == 1) return ERR_CLOSE_WD;
    mutex.lock();
    Resource* old = array[i];
    Resource::unref(old);
    array[i] = nullptr;
    mutex.unlock();
    return (old == nullptr) ? ERR_INVALID_ID : 0;
}

void Table::closeAll() {
    mutex.lock();
    for (int i=2; i<n; i++) {
        Resource::unref(array[i]);
        array[i] = nullptr;
    }
    mutex.unlock();
}
    

Table* Table::forkMe() {
    Table* tp = new Table(n);
    for (long i=0; i<n; i++) {
        if (this->array[i]) {
            tp->array[i] = this->array[i]->forkMe();
        } else {
            tp->array[i] = nullptr;
        }
    }
    return tp;
}

Resource* Table::get(long fd, ResourceType type) {
    if (fd < 0) return nullptr;
    if (fd >= n) return nullptr;
    Resource* res = array[fd];
    if (res->type != type) return nullptr;
    return res;
}
