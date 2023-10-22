#ifndef _HEAP_TIMER_H
#define _HEAP_TIMER_H

#include <time.h>
#include <netinet/in.h>

class HeapTimer;

struct client_data
{
    sockaddr_in address;
    int sockfd;
    HeapTimer *timer;
};

class HeapTimer {
public:
    HeapTimer() = default;

    time_t expire;
    void (*cb_func) (client_data *);
    client_data *user_data;
};

class TimeHeap {
public:
    TimeHeap(int cap = 1);
    ~TimeHeap();

    void add_timer(HeapTimer *timer);
    void adjust_timer(HeapTimer *timer);
    void del_timer(HeapTimer *timer);
    HeapTimer *top() const;
    void pop_timer();
    void tick();
    bool empty() const;

private:
    void percolate_down(int hole);
    void resize();

    HeapTimer **array;
    int capacity;
    int cur_size;
};

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    TimeHeap m_timer_heap;
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data *user_data);

#endif