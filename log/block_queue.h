#ifndef _BLOCK_QUEUE_H
#define _BLOCK_QUEUE_H

#include <iostream>
#include <queue>
#include <sys/time.h>
#include "../lock/locker.h"

using namespace std;

template <typename T>
class BlockQueue {
public:
    BlockQueue(int maxCapacity = 1000) : capacity(maxCapacity), q(), locker() , cond() {}

    ~BlockQueue() = default;

    void clear() {
        locker.lock();
        q.clear();
        locker.unlock();
    }

    int getSize() {
        int tem = 0;

        locker.lock();
        tem = q.size();
        locker.unlock();

        return tem;
    }

    int getCapacity() {
        int tem = 0;

        locker.lock();
        tem = capacity;
        locker.unlock();

        return tem;
    }

    bool front(T &value) {
        locker.lock();

        if (q.size() == 0) {
            locker.unlock();
            return false;
        }

        value = q.front();
        locker.unlock();

        return true;
    }

    bool back(T &value) {
        locker.lock();

        if (q.size() == 0) {
            locker.unlock();
            return false;
        }

        value = q.back();
        locker.unlock();

        return true;
    }

    bool empty() {
        int t = 0;

        locker.lock();
        t = q.size();
        locker.unlock();

        return t == 0;
    }

    bool full() {
        int t = 0;

        locker.lock();
        t = q.size();
        locker.unlock();

        return t == capacity;
    }

    bool push(const T &value) {
        locker.lock();

        if (q.size() == capacity) {
            cond.broadcast();
            locker.unlock();
            return false;
        }

        q.push(value);
        cond.broadcast();
        locker.unlock();

        return true;
    }

    bool pop(T &value) {
        locker.lock();

        while (q.size() <= 0) {
            if (!cond.wait(locker.get())) {
                locker.unlock();
                return false;
            }
        }

        value = q.front();
        q.pop();
        locker.unlock();

        return true;
    }

    bool pop(T &value, int msecond) {
        struct timeval now = {0, 0};
        struct timespec t = {0, 0};
        gettimeofday(&now, NULL);

        locker.lock();
        if (q.size() <= 0) {
            t.tv_sec = msecond / 1000;
            t.tv_nsec = (msecond % 1000) * 1000;

            if (!cond.timewait(locker.get(), t)) {
                locker.unlock();
                return false;
            }
        }

        if (q.size() <= 0) {
            locker.unlock();
            return false;
        }

        value = q.front();
        q.pop();

        locker.unlock();
        return true;
    }

private:
    queue<T> q;
    int capacity;
    Locker locker;
    Cond cond;
};

#endif