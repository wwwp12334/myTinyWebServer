#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

#include <string>
#include "./threadpool/threadpool.h"
#include "./timer/heap_timer.h"
#include "./http/http_conn.h"

const int MAX_FD = 65536;
const int MAX_EVENT_NUM = 10000;
const int TIMESLOT = 5;

class WebServer {
public:
    WebServer();
    ~WebServer();

    void init(int port, string user, string passwd, string databaseName,
            int log_write, int opt_linger, int trigmode, int sql_num, 
            int thread_num, int close_log, int actor_model);

    void sql_pool();
    void thread_pool();
    void log_write();
    void trig_mode();
    void eventListen();
    void eventLoop();
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(HeapTimer *timer);
    void deal_timer(HeapTimer *timer, int sockfd);
    bool dealclinetdata();
    bool dealwithsignal(bool& timeout, bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

public:
    int m_port;
    char *m_root;
    int m_log_write; 
    int is_close_log;
    int m_actormodel;

    int m_pipefd[2];
    int m_epollfd;
    HttpConn *user;

    SqlConnectionPool *m_connpool;
    string m_user;
    string m_passwd;
    string m_databaseName;
    int m_sql_num;

    ThreadPool<HttpConn> *m_pool;
    int m_thread_num;

    epoll_event events[MAX_EVENT_NUM];

    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode;
    int m_LISTENTrigmode;
    int m_CONNTrigmode;

    client_data *user_timer;
    Utils utils;

};



#endif