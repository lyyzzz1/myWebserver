#ifndef LST_TIMER
#define LST_TIMER

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../log/log.h"
#include <time.h>

class util_timer;

//连接资源结构体
struct client_data {
    sockaddr_in address;
    int sockfd;
    util_timer* timer;
};

//定时器类
class util_timer {
  public:
    util_timer() : prev(NULL), next(NULL) {}
    ~util_timer();

  public:
    time_t expire; //过期时间

    void (*cb_func)(client_data*); //回调函数
    client_data* user_data;        //连接资源
    util_timer* prev;              //前向定时器
    util_timer* next;              //后继定时器
};

//定时器容器类
class sort_timer_lst {
  private:
    //该函数的作用是将timer插入到lst当中，从head以后开始遍历
    //即最多会出现head->next==timer的情况，timer不会在head前出现
    void add_timer(util_timer* timer, util_timer* head);

  public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer* timer);
    /*调整定时器，任务发生变化时，调整定时器在链表中的位置
    任务发生变化时会更新expir，所以只会将expire变大，重新插入的位置肯定在当前位置后
    */
    void adjust_timer(util_timer* timer);
    void del_timer(util_timer* timer);
    /*这个函数的作用是从head开始遍历，如果碰到time<expire的情况就返回
    否则就将该定时器对应的任务执行，然后将该节点移除，将下一个节点设为头结点*/
    void tick();

  private:
    util_timer* head;
    util_timer* tail;
};

class Utils {
  public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    //将文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启ONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数，将收到的信号发送给主循环以提高效率
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void (*handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发sigalrm信号
    void timer_handler();

    void show_error(int connfd, const char* info);

  public:
    static int* u_pipefd;
    sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

//定时器的回调函数，当定时器时间到了则会执行该函数
void cb_func(client_data* uesr_data);

#endif