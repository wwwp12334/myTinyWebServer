#include "sql_connection_pool.h"

SqlConnectionPool::SqlConnectionPool() {
    curConnNum = 0;
    freeConnNum = 0;
}

void SqlConnectionPool::init(string url, int port, string user, string password, string dbName, int max_conn_num, int is_close_log) {
    m_url = url;
    m_port = port;
    m_user = user;
    m_password = password;
    m_dbName = dbName;
    maxConnNum = max_conn_num;
    this->is_close_log = is_close_log;

    for (int i = 0; i < maxConnNum; ++i) {
        MYSQL *conn = NULL;
        conn = mysql_init(conn);

        if (conn == NULL) {
            LOG_ERROR("Mysql error");
            exit(1);
        }  

        conn = mysql_real_connect(conn, url.c_str(), user.c_str(), password.c_str(), dbName.c_str(), port, NULL, 0);
        if (conn == NULL) {
            LOG_ERROR("Mysql error");
            exit(1);
        }

        connList.push_back(conn);
        ++freeConnNum;
    }

    reserve = Sem(freeConnNum);
}

MYSQL *SqlConnectionPool::getConnection() {
    if (connList.size() == 0)
        return NULL;
    
    MYSQL *conn = NULL;

    reserve.wait();
    locker.lock();

    conn = connList.front();
    connList.pop_front();

    ++curConnNum;
    --freeConnNum;

    locker.unlock();

    return conn;
}

bool SqlConnectionPool::ReleaseConnection(MYSQL *conn) {
    if (conn == NULL)
        return false;

    locker.lock();

    --curConnNum;
    ++freeConnNum;

    connList.push_back(conn);

    locker.unlock();

    reserve.post();

    return true;
}

int SqlConnectionPool::getFreeConnNum() {
    int t = 0;

    locker.lock();

    t = freeConnNum;

    locker.unlock();

    return t;
}

void SqlConnectionPool::destroyConnPool() {
    locker.lock();

    if (connList.size() > 0) {
        for (auto it = connList.begin(); it != connList.end(); ++it) {
            MYSQL *conn = *it;
            mysql_close(conn);
        }

        connList.clear();
        curConnNum = 0;
        freeConnNum = 0;
    }

    locker.unlock();
}

SqlConnectionPool::~SqlConnectionPool() {
    destroyConnPool();
}

ConnRAII::ConnRAII(MYSQL **sql, SqlConnectionPool *pool) {
    *sql = pool->getConnection();

    connRAII = *sql;
    poolRAII = pool;
}

ConnRAII::~ConnRAII() {
    poolRAII->ReleaseConnection(connRAII);
}