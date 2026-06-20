#pragma once

#ifndef _SPINLOCK_H
#define _SPINLOCK_H

namespace spinlock
{
    bool try_lock(volatile long* lock);
    void lock(volatile long* lock);
    void unlock(volatile long* lock);
}

#endif // !_SPINLOCK_H
