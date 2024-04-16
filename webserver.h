#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <arpa/inet.h>
#include <cassert>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "./http/http_conn.h"
#include "./threadpool/threadpool.h"
#include "./timer/lst_timer.h"

const int MAX_FD = 65536;
const int MAX_EVENT_NUMBER = 10000;
const int TIMESLOT = 5;

class WebServer {
public:
    WebServer();
    ~WebServer();

    void init(int port, string user, string passWord, string databaseName,
              int log_write, int opt_linger, int trigmode, int sql_num,
              int thread_num, int close_log, int actor_model);
    //初始化线程池，申请资源
    void thread_pool();
    //初始化数据库连接池
    void sql_pool();
    
    void log_write();
    void trig_mode();
    void eventListen();
    void eventLoop();
    //新创建一个定时器，加入链表，将定时器与http连接绑定
    void timer(int connfd, struct sockaddr_in client_address);
    //更新过期时间，并调整timer在链表中的位置
    void adjust_timer(util_timer* timer);
    //调用回调函数，将删除epoll中的sockfd并关闭连接，删除链表中的timer
    void deal_timer(util_timer* timer, int sockfd);
    bool dealclientdata();
    bool dealwithsignal(bool& timeout, bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

public:
    int m_port;
    char* m_root;
    int m_log_write;//日志写入方式，1为异步，0为同步
    int m_close_log;
    int m_actormodel;

    int m_pipefd[2];
    int m_epollfd;
    http_conn* users;

    //数据库相关
    connection_pool* m_connPool;
    string m_user;
    string m_passWord;
    string m_databaseName;
    int m_sql_num;

    //线程池相关
    threadpool<http_conn>* m_pool;
    int m_thread_num;

    // epoll_event
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode;
    int m_LISTENTrigmode;
    int m_CONNTrigmode;

    //定时器
    client_data* users_timer;
    Utils utils;
};

#endif