#include "common.h"

#ifndef USE_LWP_LOCK

void __attribute__ ((weak)) _NFS_lock_init(mutex_t *mutex)
{
	return;
}

void __attribute__ ((weak)) _NFS_lock_deinit(mutex_t *mutex)
{
	return;
}

void __attribute__ ((weak)) _NFS_lock(mutex_t *mutex)
{
	return;
}

void __attribute__ ((weak)) _NFS_unlock(mutex_t *mutex)
{
	return;
}

#endif // USE_LWP_LOCK
