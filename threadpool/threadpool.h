#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <list>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

using namespace std;

template<typename T>
class  ThreadPool {
public:
    ThreadPool(int actor_model, SqlConnectionPool *connPool, int thread_num = 8, int max_task_num = 1000);
    ~ThreadPool();

    bool append(T *request, int state);
    bool append_p(T *request);

private:
    static void *work(void *arg);
    void run();

private:
    int threadNum;
    int maxTaskNum;
    list<T *> taskList;
    pthread_t *threads;
    Locker locker;
    Sem taskSem;
    int m_actor_model;
    SqlConnectionPool *m_connPool;
};

template<typename T>
ThreadPool<T>::ThreadPool(int actor_model, SqlConnectionPool *connPool, int thread_num, int max_task_num) : m_actor_model(actor_model), 
                                        threadNum(thread_num), maxTaskNum(max_task_num), m_connPool(connPool), threads(NULL) {
    if (thread_num <= 0 || max_task_num <= 0) {
        throw exception();
    }

    threads = new pthread_t[thread_num];

    for (int i = 0; i < thread_num; ++i) {
        if (pthread_create(threads + i, NULL, work, this) != 0) {
            delete[] threads;
            throw exception();
        }

        if (pthread_detach(threads[i]) != 0) {
            delete[] threads;
            throw exception();
        }
    }
}

template<typename T>
ThreadPool<T>::~ThreadPool() {
    delete[] threads;
}

template<typename T>
bool ThreadPool<T>::append(T *request, int  state) {
    locker.lock();
    
    if (taskList.size() >= maxTaskNum) {
        locker.unlock();
        return false;
    }

    request->m_state = state;
    taskList.push_back(request);

    locker.unlock();
    taskSem.post();

    return true;
}

template<typename T>
bool ThreadPool<T>::append_p(T *request) {
    locker.lock();
    
    if (taskList.size() >= maxTaskNum) {
        locker.unlock();
        return false;
    }

    taskList.push_back(request);

    locker.unlock();
    taskSem.post();

    return true;
}

template<typename T>
void *ThreadPool<T>::work(void *arg) {
    ThreadPool *pool = (ThreadPool *)arg;
    pool->run();

    return pool;
}

template<typename T>
void ThreadPool<T>::run() {

    while (true) {
        taskSem.wait();
        locker.lock();

        if (taskList.empty()) { //这种情况应该不可能存在,保险起见写了
            locker.unlock();
            continue;
        }

        T *request = taskList.front();
        taskList.pop_front();

        locker.unlock();

        if (request == NULL) {
            continue;
        }

        if (m_actor_model == 1) {   //reactor

            if (request->m_state == 0) {

                if (request->read_once()) {
                    request->improv = 1;
                    ConnRAII connRAII(&request->mysql, m_connPool);
                    request->process();
                } else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }

            } else {
                if (request->write()) {
                    request->improv = 1;
                } else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }

        } else {
            ConnRAII connRAII(&request->mysql, m_connPool);
            request->process();
        }

    }
} 

#endif