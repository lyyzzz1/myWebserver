#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool(){
    m_CurConn = 0;
    m_FreeConn = 0;
}

connection_pool* connection_pool::GetInstance(){
    static connection_pool connPool;//静态局部变量，懒汉模式
    return &connPool;
}

void connection_pool::init(string url,string User,string PassWord,string DatabaseName,int Port,int MaxConn,int close_log)
{
    m_url = url;
    m_User = User;
    m_Port = Port;
    m_PassWord = PassWord;
    m_DatabaseName = DatabaseName;
    m_close_log = close_log;

    for (int i = 0; i < MaxConn; i++)
    {
        MYSQL* con = NULL;
        con = mysql_init(con);

        if(con==NULL)
        {
            LOG_ERROR("MySQL ERROR");
            exit(1);
        }
        con = mysql_real_connect(con,url.c_str(),User.c_str(),PassWord.c_str(),DatabaseName.c_str(),Port,"/tmp/mysql.sock",0);
        cout << "连接成功，编号：" << i << endl;

        if(con==NULL)
        {
            LOG_ERROR("MySQL ERROR");
            exit(1);
        }
        connList.push_back(con);
        ++m_FreeConn;
    }
    
    reserve = sem(m_FreeConn);

    m_MaxConn = m_FreeConn;
}

MYSQL* connection_pool::GetConnection(){
    cout << "GetConnection()" << endl;
    MYSQL* con = NULL;

    if(connList.size() == 0)
    {
        return NULL;
    }

    reserve.wait();
    cout << "获取信号量成功" << endl;
    lock.lock();
    cout << "线程id:" << pthread_self() << "获取锁" << endl;
    LOG_INFO("%s%ld%s","线程id:",pthread_self(),"获取锁成功");
    con = connList.front();
    connList.pop_front();

    --m_FreeConn;
    ++m_CurConn;

    lock.unlock();
    cout << "线程id:" << pthread_self() << "释放锁" << endl;
    LOG_INFO("%s%ld%s","线程id:",pthread_self(),"释放锁成功");
    return con;
}

bool connection_pool::ReleaseConnection(MYSQL* con)
{
    if(con == NULL)
    {
        return false;
    }

    lock.lock();

    connList.push_back(con);
    ++m_FreeConn;
    --m_CurConn;

    lock.unlock();

    reserve.post();
    return true;
}

void connection_pool::DestroyPool()
{
    lock.lock();
    if(connList.size()>0)
    {
        list<MYSQL*>::iterator it;
        for(it = connList.begin();it!=connList.end();it++)
        {
            MYSQL* con = *it;
            mysql_close(con);
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();
    }

    lock.unlock();
}

int connection_pool::GetFreeConn()
{
    return this->m_FreeConn;
}

connection_pool::~connection_pool()
{
    DestroyPool();
}

connectionRAII::connectionRAII(MYSQL** SQL,connection_pool* connPool)
{
    *SQL = connPool->GetConnection();

    conRAII = *SQL;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII()
{
    poolRAII->ReleaseConnection(conRAII);
}