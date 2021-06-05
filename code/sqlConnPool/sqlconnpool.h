#ifndef SQL_CONN_POOL_H
#define SQL_CONN_POOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <thread>
#include "../lock/locker.h"
#include <mutex>
#include <semaphore.h>
#include "../log/log.h"

using namespace std;

class SqlConnPool
{
public:
    void init(const char* host, int port,
              const char* user, const char* pwd,
              const char* dbName, int maxConnCnt);
    
    void destroy();

    static SqlConnPool *instance();

    MYSQL *getConn();
    void freeConn(MYSQL *conn);
    int getFreeConnCnt();

private:
    SqlConnPool();
    ~SqlConnPool();

    int useConnCnt;  // 最大的连接数
    int freeConnCnt; // 空闲的用户数

    queue<MYSQL*> connQue;
    mtx *mtxPool; // 互斥锁
    sem *semFree; // 信号量
};


class SqlConnect
{
public:
    SqlConnect(MYSQL** sql, SqlConnPool *sqlConnPool);
    ~SqlConnect();

private:
    MYSQL *sql;
    SqlConnPool *sqlConnPool;
};

#endif