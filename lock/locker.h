#ifndef _LOCKER_H
#define _LOCKER_H

#include <pthread.h>
#include <semaphore.h>
#include <exception>


class Sem {
public: 
    Sem() {
        if (sem_init(&m_sem, 0, 0) != 0) {
           throw std::exception();
        }
    }

    Sem(int num) {
        if (sem_init(&m_sem, 0, num) != 0) {
            throw std::exception();
        }
    }

    ~Sem() {
        sem_destroy(&m_sem);
    }

    bool wait() {
        return sem_wait(&m_sem) == 0;
    }

    bool post() {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};

class Locker {
public:
    Locker() {
        if (pthread_mutex_init(&m_mutex, NULL) != 0) {
            throw std::exception();
        }
    }

    ~Locker() {
        pthread_mutex_destroy(&m_mutex);
    }

    bool lock() {
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    bool unlock() {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t *get() {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

class Cond {
public:
    Cond() {
        if (pthread_cond_init(&m_cond, NULL) != 0) {
            throw std::exception();
        }
    }

    ~Cond() {
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t *mutex) {
        return pthread_cond_wait(&m_cond, mutex) == 0;
    }

    bool timewait(pthread_mutex_t *mutex, struct timespec t) {
        return pthread_cond_timedwait(&m_cond, mutex, &t) == 0;
    }

    bool signal() {
        return pthread_cond_signal(&m_cond) == 0;
    }

    bool broadcast() {
        return pthread_cond_broadcast(&m_cond) == 0;
    }
    
private:
    pthread_cond_t m_cond;
};

#endif
