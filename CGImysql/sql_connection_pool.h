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
    bool ReleaseConnection(MYSQL *con);
    int GetFreeConn();
    void DestroyPool();

    static connection_pool* GetInstance();

    void init(string url,string User,string PassWord,string DatabaseName,int Port,int MaxConn,int close_log);

public:
    string m_url;       //主机地址
    string m_Port;      //端口号
    string m_User;      //SQL用户名
    string m_PassWord;  //SQL密码
    string m_DatabaseName; //数据库名称
    int m_close_log;    //日志开关
};

class connectionRAII
{
private:
    MYSQL* conRAII;
    connection_pool* poolRAII;
public:
    connectionRAII(MYSQL** SQL,connection_pool* connPool);
    ~connectionRAII();
};
#endif