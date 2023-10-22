#ifndef _CONNECTION_POOL_H
#define _CONNECTION_POOL_H

#include <mysql/mysql.h>
#include <list>
#include <string>
#include <string.h>
#include "../lock/locker.h"
#include "../log/log.h"

class SqlConnectionPool {
public:
    static SqlConnectionPool *getInstance() {
        static SqlConnectionPool sql_pool;
        return &sql_pool;
    }

    MYSQL *getConnection();
    bool ReleaseConnection(MYSQL *);
    int getFreeConnNum();
    void destroyConnPool();

    void init(string url, int port, string user, string password, string dbName, int max_conn_num, int is_close_log);

private:
    SqlConnectionPool();
    ~SqlConnectionPool();


    int curConnNum;//当前已使用的连接数
    int freeConnNum;
    int maxConnNum;
    Sem reserve;
    Locker locker;
    list<MYSQL *> connList;
public:
    string m_url;
    int m_port;
    string m_user;
    string m_password;
    string m_dbName;
    int is_close_log;
};

class ConnRAII {
public:
    ConnRAII(MYSQL **, SqlConnectionPool *conn_pool);
    ~ConnRAII();

private:
    MYSQL *connRAII;
    SqlConnectionPool *poolRAII;
};

#endif