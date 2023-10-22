#include <assert.h>
#include "webserver.h"

WebServer::WebServer() {
    user = new HttpConn[MAX_FD];
    user_timer = new client_data[MAX_FD];

    char server_path[200] = {'\0'};
    getcwd(server_path, 199);
    char root[6] = "/root";
    m_root = new char[strlen(root) + strlen(server_path) + 1];
    strcpy(m_root, server_path);
    strcat(m_root, root);
}

WebServer::~WebServer() {
    close(m_epollfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
    close(m_listenfd);
    delete[] user;
    delete[] user_timer;
    delete m_pool;
}

void WebServer::init(int port, string user, string passwd, string databaseName,
        int log_write, int opt_linger, int trigmode, int sql_num, 
        int thread_num, int close_log, int actor_model) {

    m_port = port;
    m_user = user;
    m_passwd = passwd;
    m_databaseName = databaseName;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    is_close_log = close_log;
    m_actormodel = actor_model;
}

 void WebServer::trig_mode() {
        //LT + LT
    if (0 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }
    //LT + ET
    else if (1 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    //ET + LT
    else if (2 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    //ET + ET
    else if (3 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
 }

 void WebServer::log_write() {
    if (0 == is_close_log)
    {
        //初始化日志
        if (1 == m_log_write)//是同步还是异步标志
            Log::getInstance()->init("./ServerLog", is_close_log, 800000, 8192, 800);
        else
            Log::getInstance()->init("./ServerLog", is_close_log, 800000, 8192, 0);
    }
 }

 void WebServer::sql_pool() {
    m_connpool = SqlConnectionPool::getInstance();
    m_connpool->init("localhost", 3306, m_user, m_passwd, m_databaseName, m_sql_num, is_close_log);
 }

 void WebServer::thread_pool() {
    m_pool = new ThreadPool<HttpConn>(m_actormodel, m_connpool, m_thread_num);
 }

 void WebServer::eventListen() {
    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    //优雅关闭连接
    if (0 == m_OPT_LINGER)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (1 == m_OPT_LINGER)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    utils.init(TIMESLOT);

    epoll_event events[MAX_EVENT_NUM];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    HttpConn::m_epollfd = m_epollfd;

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);

    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);

    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

void WebServer::timer(int connfd, struct sockaddr_in client_address) {
    user[connfd].init(connfd, client_address, m_root, m_CONNTrigmode, is_close_log, m_user, m_passwd, m_databaseName);

    user_timer[connfd].address = client_address;
    user_timer[connfd].sockfd = connfd;
    HeapTimer *timer = new HeapTimer();
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    timer->user_data = &user_timer[connfd];
    user_timer[connfd].timer = timer;

    utils.m_timer_heap.add_timer(timer);
}

void WebServer::adjust_timer(HeapTimer *timer) {
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_heap.adjust_timer(timer);

    LOG_INFO("%s", "adjust time once");
}

void WebServer::deal_timer(HeapTimer *timer, int sockfd) {

    timer->cb_func(&user_timer[sockfd]);
    if (timer) {
        utils.m_timer_heap.del_timer(timer);
    }

    LOG_INFO("close fd %d", sockfd);
}

bool WebServer::dealclinetdata() {
    sockaddr_in client_address;
    socklen_t address_len = sizeof(client_address);

    if (m_LISTENTrigmode == 0) {
        int connfd = accept(m_listenfd, (sockaddr *)&client_address, &address_len);
        if (connfd < 0) {
            LOG_ERROR("%s:erno is :%d", "accept error", errno);
            return false;
        }

        if (HttpConn::m_user_count >= MAX_FD) {
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }

        timer(connfd, client_address);
    } else {
        while (1) {
            int connfd = accept(m_listenfd, (sockaddr *)&client_address, &address_len);
            if (connfd < 0) {
                LOG_ERROR("%s:errno is :%d", "accept error", errno);
                break;
            }

            if (HttpConn ::m_user_count >= MAX_FD) {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Interal server busy");
                break;
            }
            timer(connfd, client_address);
        }

        return false;
    }

    return true;
}

bool WebServer::dealwithsignal(bool& timeout, bool& stop_server) {
    int ret = 0;
    int sig;
    char sigals[1024] = {'\0'};
    ret = recv(m_pipefd[0], sigals, 1023, 0);
    if (ret == -1) {
        return false;
    } else if (ret == 0) {
        return false;
    } else {
        for (int i = 0; i < ret; ++i) {
            int sig = sigals[i];
            switch(sig) {
                case SIGALRM: {
                    timeout = true;
                    break;
                }
                case SIGTERM: {
                    stop_server = true;
                    break;
                }
            }
        }
    }

    return true;
}

void WebServer::dealwithread(int sockfd) {
    HeapTimer *timer = user_timer[sockfd].timer;

    if (m_actormodel == 1) {
        if (timer) {
            adjust_timer(timer);
        }

        m_pool->append(user + sockfd, 0);

        while (true) {
            if (1 == user[sockfd].improv) {
                if (1 == user[sockfd].timer_flag) {
                    deal_timer(timer, sockfd);
                    user[sockfd].timer_flag = 0;
                }

                user[sockfd].improv = 0;
                break;
            }
        }

    } else {
        if (user[sockfd].read_once()) {
            LOG_INFO("deal with the client(%s)", inet_ntoa(user[sockfd].get_address()->sin_addr));

            m_pool->append_p(user + sockfd);

            if (timer) {
                adjust_timer(timer);
            }
        } else {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd) {
    HeapTimer *timer = user_timer[sockfd].timer;

    if (m_actormodel == 1) {
        if (timer) {
            adjust_timer(timer);
        }

        m_pool->append(user + sockfd, 1);

        while (true) {
            if (user[sockfd].improv == 1) {
                if (user[sockfd].timer_flag == 1) {
                    deal_timer(timer, sockfd);
                    user[sockfd].timer_flag = 0;
                }

                user[sockfd].improv = 0;
                break;
            }
        }

    } else {
        if (user[sockfd].write()) {
            LOG_INFO("send data to the client(%s)", inet_ntoa(user[sockfd].get_address()->sin_addr));

            if (timer) {
                adjust_timer(timer);
            }
        } else {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::eventLoop() {
    bool stop_server = false;
    bool time_out = false;

    while (!stop_server) {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUM, -1);
        if (number < 0 && errno != EINTR) {
            LOG_ERROR("%s", "epoll_wait error");
            break;
        }

        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;

            if (sockfd == m_listenfd) {
                bool flag = dealclinetdata();
                if (flag == false)
                    continue;

            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                HeapTimer *timer = user_timer[sockfd].timer;
                deal_timer(timer, sockfd);

            } else if ( (sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)) {
                bool flag = dealwithsignal(time_out, stop_server);
                if (flag == false)
                    LOG_ERROR("%s", "dealwithsignal error");

            } else if (events[i].events & EPOLLIN) {
                dealwithread(sockfd);

            } else if (events[i].events & EPOLLOUT) {
                dealwithwrite(sockfd);
            }
        }

        if (time_out) {
            utils.timer_handler();

            LOG_INFO("%s", "time tick");

            time_out = false;
        }
    }
}
