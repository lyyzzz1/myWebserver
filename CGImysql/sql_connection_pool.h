#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

class connection_pool
{
private:
    connection_pool();
    ~connection_pool();

    int m_MaxConn;//最大连接数
    int m_CurConn;//目前连接数
    int m_FreeConn;//空闲连接数
    locker lock;
    list<MYSQL *> connList;//连接池
    sem reserve;
public:
    MYSQL *GetConnection();
    bool ReleaseConnection();
    int GetFreeConn();
    void DestroyPool();

    static connection_pool* GetInstance();

    void init(string url,string User,string PassWord,string DatabaseName,int Port,int MaxConn,int close_log);

public:
    string m_url;
    string m_Port;
    string m_User;
    string m_PassWord;
    string m_DatabaseName;
    int m_close_log;
};

connection_pool::connection_pool(/* args */)
{
}

connection_pool::~connection_pool()
{
}


#endif