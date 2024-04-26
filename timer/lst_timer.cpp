#include "lst_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst() : head(NULL), tail(NULL) {}

sort_timer_lst::~sort_timer_lst() {
    //析构函数中将链表释放，即遍历链表全部delete
    util_timer* tmp = head;
    while (tmp) {
        head = head->next;
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer* timer) {
    if (!timer)
        return;
    if (!head) { //当head为空时，添加该节点即将该节点作为头结点
        head = tail = timer;
        return;
    }
    if (timer->expire < head->expire) {
        //当新插入的节点过期时间比头结点短时则将该节点作为头结点
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer, head);
}

void sort_timer_lst::adjust_timer(util_timer* timer) {
    if (!timer)
        return;
    util_timer* tmp = timer->next;
    //如果是队尾或者过期时间小于下一个节点，则不调整
    if (!tmp || (timer->expire < tmp->expire)) {
        return;
    }
    //如果是头结点，那么拿出来后重新插入
    if (timer == head) {
        head = head->next;
        head->prev = nullptr;
        timer->next = nullptr;
        add_timer(timer, head);
    } else {
        timer->prev->next = tmp;
        tmp->prev = timer->prev;
        add_timer(timer, tmp);
    }
}

void sort_timer_lst::add_timer(util_timer* timer, util_timer* head) {
    util_timer* pre = head;
    util_timer* tmp = head->next;
    while (tmp) {
        if (timer->expire <
            tmp->expire) { //需要插入的节点比当前遍历到的节点过期时间短
            pre->next = timer;
            timer->prev = pre;
            timer->next = tmp;
            tmp->prev = timer;
            return;
        }
        pre = tmp;
        tmp = tmp->next;
    }
    //如果退出循环则证明tmp到了nullptr，pre为尾结点
    pre->next = timer;
    timer->prev = pre;
    timer->next = nullptr;
    tail = timer;
}

void sort_timer_lst::del_timer(util_timer* timer) {
    if (!timer)
        return;
    if ((timer == head) && (timer == tail)) {
        head = nullptr;
        tail = nullptr;
        delete timer;
        return;
    }
    if (timer == head) {
        head = head->next;
        head->prev = nullptr;
        delete timer;
        return;
    }
    if (timer == tail) {
        tail = tail->prev;
        tail->next = nullptr;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void sort_timer_lst::tick() {
    if (!head)
        return;

    time_t cur = time(NULL);
    util_timer* tmp = head;
    while (tmp) {
        if (cur < tmp->expire) {
            return;
        }
        tmp->cb_func(tmp->user_data);
        head = tmp->next;
        if (head) {
            head->prev = nullptr;
        }
        delete tmp;
        tmp = head;
    }
}

void Utils::init(int timeslot) { m_TIMESLOT = timeslot; }

int Utils::setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
    epoll_event event;
    event.data.fd = fd;

    if (TRIGMode == 1)
        event.events =
            EPOLLIN | EPOLLET |
            EPOLLRDHUP; //当一个套接字连接被对方关闭，或者对方关闭了写操作（即我们无法再读取到数据）时，如果你设置了EPOLLRDHUP标志，那么epoll就会返回这个事件。
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void Utils::sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void Utils::addsig(int sig, void (*handler)(int), bool restart) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    //当设置了这个标志后，如果一个系统调用由于接收到此信号而被中断，系统会自动重新启动该系统调用，而不是返回一个错误。
    if (restart)
        sa.sa_flags |= SA_RESTART;
    //表示当处理该信号时，阻塞所有的信号
    sigfillset(&sa.sa_mask);
    // assert为一个调试宏，若表达式内为假则会中断程序的运行
    // sigaction为改变系统对特定信号的处理方式的函数，sig为信号，第二个参数为新的处理方式
    //第三个函数可以为空，若不为空将旧的处理方式存入到其中
    assert(sigaction(sig, &sa, NULL) != -1);
}

void Utils::timer_handler() {
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char* info) {
    //第三个参数不可以使用sizeof(info)因为只会获取info指针的大小而不是字符串的长度
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int* Utils::u_pipefd = nullptr;
int Utils::u_epollfd = 0;

void cb_func(client_data* user_data) {
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}