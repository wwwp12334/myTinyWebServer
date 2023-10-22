#include "heap_timer.h"
#include "../http/http_conn.h"
#include <exception>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

TimeHeap::TimeHeap(int cap) : capacity(cap), cur_size(0) {
    array = new HeapTimer*[capacity];
    if (!array) {
        throw std::exception();
    }

    for (int i = 0; i < capacity; ++i) {
        array[i] = NULL;
    }
}

TimeHeap::~TimeHeap() {
    for (int i = 0; i < cur_size; ++i) {
        delete array[i];
    }

    delete[] array;
}

void TimeHeap::add_timer(HeapTimer *timer) {
    if (!timer) {
        return ;
    }

    if (cur_size >= capacity) {
        resize();
    }

    int hole = cur_size++;
    int parent = 0;
    for (; hole > 0; hole = parent) {
        parent = (hole - 1) / 2;
        if (array[parent]->expire <= timer->expire) {
            break;
        }

        array[parent] = array[hole];
    }

    array[hole] = timer;
}

void TimeHeap::del_timer(HeapTimer *timer) {
    if (!timer) {
        return ;
    }

    timer->cb_func = NULL;
}

HeapTimer *TimeHeap::top() const  {
    if (empty()) {
        return NULL;
    }

    return array[0];
}

void TimeHeap::pop_timer() {
    if (empty()) {
        return ;
    }

    if (array[0]) {
        delete array[0];
        array[0] = array[--cur_size];
        percolate_down(0);
    }
}

void TimeHeap::tick() {
    HeapTimer *tmp = array[0];
    time_t cur = time(NULL);

    while (!empty()) {
        if (!tmp) {
            break;
        }

        if (tmp->expire > cur) {
            break;
        }

        if (tmp->cb_func) {
            tmp->cb_func(tmp->user_data);
        }

        pop_timer();
        tmp = array[0];
    }
}

bool TimeHeap::empty() const {
    return cur_size == 0;
}

void TimeHeap::percolate_down(int hole) {
    HeapTimer *temp = array[hole];
    int child = 0;
    for (; ((hole * 2 + 1) <= cur_size - 1); hole = child) {
        child = hole * 2 + 1;
        if ((child < (cur_size - 1)) && array[child + 1]->expire < array[child]->expire) {
            ++child;
        }

        if (array[child]->expire < temp->expire) {
            array[hole] = array[child];
        } else {
            break;
        }
    }

    array[hole] = temp;
}

void TimeHeap::resize() {
    HeapTimer **temp = new HeapTimer* [2 * capacity];
    for (int i = 0; i < capacity; ++i) {
        temp[i] = NULL;
    }

    capacity = 2 *capacity;
    for (int i = 0; i < cur_size; ++i) {
        temp[i] = array[i];
    }

    delete[] array;
    array = temp;
}

void TimeHeap::adjust_timer(HeapTimer *timer) {
    for (int i = 0; i < cur_size; ++i) {
        if (array[i] == timer) {
            percolate_down(i);
            break;
        }
    }
}

void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

//对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void Utils::sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

//设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_timer_heap.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
void cb_func(client_data *user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    HttpConn::m_user_count--;
}