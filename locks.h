#define LOCKED(x) lock_wait(); x; lock_unlock()
#define LOCK	lock_wait();
#define UNLOCK	lock_unlock();

extern void lock_wait();
extern void lock_unlock();
