#include "webserver.h"

WebServer::WebServer() {
    //创建相应个http连接类
    users = new http_conn[MAX_FD];

    // root文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char*)malloc(strlen(server_path) + strlen(root));
    strcpy(m_root, server_path);
    strcat(m_root, root);

    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer() {
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

void WebServer::init(int port, string user, string passWord,
                     string databaseName, int log_write, int opt_linger,
                     int trigmode, int sql_num, int thread_num, int close_log,
                     int actor_mode) {
    m_port = port;
    m_user = user;
    m_passWord = passWord;
    m_databaseName = databaseName;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_close_log = close_log;
    m_actormodel = actor_mode;
}

void WebServer::log_write() {
    if (m_close_log == 0) {
        if (m_log_write == 1) { //开启异步写入
            Log::getInstance()->init("./ServerLog", m_close_log, 2000, 800000,
                                     800);
            cout << "log_write()已开启异步写入..." << endl;
        } else {
            Log::getInstance()->init("./ServerLog", m_close_log, 2000, 800000,
                                     0);
            cout << "log_write()已开启同步写入..." << endl;
        }
    }
}

void WebServer::sql_pool() {
    m_connPool = connection_pool::GetInstance();
    cout << "获取实例成功..." << endl;
    m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306,
                     m_sql_num, m_close_log);
    cout << "初始化成功..." << endl;
    //初始化数据库读取表
    users->initmysql_result(m_connPool);
    cout << "sql_pool()成功运行..." << endl;
}

void WebServer::thread_pool() {
    m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
    cout << "thread_pool()成功运行..." << endl;
}

void WebServer::trig_mode() {
    if (m_TRIGMode == 0) { // LT + LT
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    } else if (m_TRIGMode == 1) { // LT + ET
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    } else if (m_TRIGMode == 2) { // ET + LT
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    } else if (m_TRIGMode == 3) { // ET + ET
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
    cout << "trig_mode()成功..." << endl;
}

void WebServer::eventListen() {
    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);
    /*
    struct linger 结构体用于设置 SO_LINGER 选项，其定义如下：
    struct linger {
    int l_onoff;    linger active
    int l_linger;   how many seconds to linger for
    };
    l_onoff：是否启用 linger 选项。
    l_linger：延迟的时间，单位为秒。
    在你的代码中，当 m_OPT_LINGER 为 0 时，设置 SO_LINGER 选项为 {0,
    1}，表示关闭linger 选项。 当 m_OPT_LINGER 为 1 时，设置 SO_LINGER 选项为 {1,
    1}，表示启用linger 选项，并延迟 1 秒关闭套接字。
    */
    if (m_OPT_LINGER == 0) {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    } else {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    //允许地址重用：设置 SO_REUSEADDR 选项后，即使之前的套接字仍处于 TIME_WAIT
    //状态（等待一段时间后关闭），新的套接字也可以立即使用相同的地址和端口。
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (sockaddr*)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    utils.init(TIMESLOT);

    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);             //写端非阻塞
    utils.addfd(m_epollfd, m_pipefd[0], false, 0); //读端注册到epoll

    // SIG_IGN是一个信号处理函数，表示忽略信号
    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);

    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

void WebServer::timer(int connfd, struct sockaddr_in client_addr) {
    users[connfd].init(connfd, client_addr, m_root, m_CONNTrigmode, m_close_log,
                       m_user, m_passWord, m_databaseName);
    users_timer[connfd].address = client_addr;
    users_timer[connfd].sockfd = connfd;
    util_timer* timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer);
    cout << "timer()成功..." << endl;
}

void WebServer::adjust_timer(util_timer* timer) {
    time_t cur = time(NULL);
    timer->expire = cur + TIMESLOT * 3;
    utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer once");
}

void WebServer::deal_timer(util_timer* timer, int sockfd) {
    timer->cb_func(&users_timer[sockfd]);
    if (timer) {
        utils.m_timer_lst.del_timer(timer);
    }

    LOG_INFO("close fd:%d", users_timer[sockfd].sockfd);
}

bool WebServer::dealclientdata() {
    struct sockaddr_in client_addr;
    socklen_t client_addrlength = sizeof(client_addr);
    if (m_LISTENTrigmode == 0) { // LT
        int connfd =
            accept(m_listenfd, (sockaddr*)&client_addr, &client_addrlength);
        if (connfd < 0) {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if (http_conn::m_user_count >= MAX_FD) {
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_addr);
    } else { // ET
        while (1) {
            int connfd =
                accept(m_listenfd, (sockaddr*)&client_addr, &client_addrlength);
            if (connfd < 0) {
                if (errno == EAGAIN) {
                    return true;
                } else {
                    LOG_ERROR("%s:errno is:%d", "accept error", errno);
                    return false;
                }
            }
            if (http_conn::m_user_count >= MAX_FD) {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_addr);
        }
        return false;
    }
    return true;
}

bool WebServer::dealwithsignal(bool& timeout, bool& stop_server) {
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
        return false;
    else if (ret == 0)
        return false;
    else {
        for (int i = 0; i < ret; i++) {
            switch (signals[i]) {
            case SIGALRM:
                timeout = true;
                cout << "signal timeout" <<endl;
                break;
            case SIGTERM:
                stop_server = true;
                cout << "signal stop" <<endl;
                break;
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd) {
    util_timer* timer = users_timer[sockfd].timer;
    cout << "m_actormodel: " << m_actormodel << endl;
    if (m_actormodel == 1) { // reactor
        if (timer)
            adjust_timer(timer);

        //将任务加入到任务队列中
        m_pool->append(&users[sockfd], 0);

        while (true) {
            if (users[sockfd].improv == 1) {
                if (users[sockfd].timer_flag == 1) {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    } else { // proactor
        if (users[sockfd].read_once()) {
            LOG_INFO("deal with the client(%s)",
                     inet_ntoa(users[sockfd].get_address()->sin_addr));

            if(m_pool->append_p(&users[sockfd]))
                cout << "append_p()成功..." << endl;
            else
                cout << "append_p()失败..." << endl;
            if (timer)
                adjust_timer(timer);
        } else {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd) {
    util_timer* timer = users_timer[sockfd].timer;
    if (m_actormodel == 1) { // reactor
        if (timer)
            adjust_timer(timer);

        m_pool->append(&users[sockfd], 1);
        while (true) {
            if (users[sockfd].improv == 1) {
                if (users[sockfd].timer_flag == 1) {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    } else { // proactor
        if (users[sockfd].write()) {
            LOG_INFO("send data to the client(%s)",
                     inet_ntoa(users[sockfd].get_address()->sin_addr));

            if (timer)
                adjust_timer(timer);
        } else {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::eventLoop() {
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server) {
        //-1一直阻塞到有事件发生
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;

            if (sockfd == m_listenfd) {
                bool flag = dealclientdata();
                cout << "dealclientdata()成功..." << endl;
                if (flag == false)
                    continue;
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                /*这个条件的意思是events中是否包含EPOLLRDHUP|EPOLLHUP|EPOLLERR其中之一
                EPOLLRDHUP：表示对端的socket已经关闭，或者对端的写半部分已经关闭。这通常表示对端的socket已经关闭，或者对端已经调用了shutdown函数关闭了写半部分。
                EPOLLHUP：表示挂起。这通常表示对端的socket已经关闭，或者对端已经调用了shutdown函数关闭了写半部分。
                EPOLLERR：表示发生了错误。
                */
                util_timer* timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            } else if ((sockfd == m_pipefd[0]) &&
                       (events[i].events & EPOLLIN)) {
                /*有信号到来*/
                bool flag = dealwithsignal(timeout, stop_server);
                if (flag == false) {
                    LOG_ERROR("%s", "dealclientdata failure");
                }
            } else if (events[i].events & EPOLLIN) { //读事件就绪
                cout << "sockfd: " << sockfd << " EPOLLIN" << endl;
                dealwithread(sockfd);
            } else if (events[i].events & EPOLLOUT) { //写事件就绪
                cout << "sockfd: " << sockfd << " EPOLLOUT" << endl;
                dealwithwrite(sockfd);
            }
        }
        if (timeout) {
            utils.timer_handler();
            LOG_INFO("%s", "timer tick");
            timeout = false;
        }
    }
}