#ifndef _HTTP_CONN_H
#define _HTTP_CONN_H

#include <unistd.h>
#include <string>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <mysql/mysql.h>
#include <sys/uio.h>
#include "../lock/locker.h"

using namespace std;

class HttpConn {
public:
static const int FILENAME_LEN = 200;

static const int READ_BUFFER_SIZE = 2048;

static const  int  WRITE_BUFFER_SIZE = 1024;

enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH };

enum CHECK_STATE { CHECK_STATE_REQUESTLINE, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };

enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, 
                FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION};

enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN};

public:
    HttpConn() = default;
    ~HttpConn() = default;

public:
    void init(int sockfd, const sockaddr_in & addr, char *root, int TRIGMode, int close_log, string user, string passwa, string sqlname);

    void close_conn(bool real_close = true);

    void process();

    bool read_once();

    bool write();
    
    sockaddr_in *get_address() {
        return &m_address;
    }
    //void initmysql_result(connection_pool *connPool); 改成查询操作
    int timer_flag;
    int improv;

private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);

    //下面一组函数被process_read调用以分析HTTP请求
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() {return m_read_buf + m_start_line;}
    LINE_STATUS parse_line();

    //下面一组函数被priocess_write调用填充HTTP应答
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_content_type();
    bool add_linger();
    bool add_blank_line();

    bool selectName(MYSQL *mysql, char *name);
    bool selectAccount(MYSQL *mysql, char *name, char *passwd);

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL *mysql;
    int m_state;

private:
    int m_sockfd;
    sockaddr_in m_address;

    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_idx;
    int m_checked_idx;
    int m_start_line;
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;

    CHECK_STATE m_check_state;
    METHOD m_method;
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;

    char *m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;

    int cgi;
    char *m_string;
    int bytes_to_send;
    int bytes_have_send;
    char *doc_root;

    int m_TRIGMode;
    int is_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];

    static Locker locker;
};


#endif