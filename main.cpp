#include "config.h"

int main(int argc, char* argv[]) {
    string user = "lyy";
    string passwd = "123456";
    string databasename = "webserver";

    Config config;
    config.parse_arg(argc, argv);

    WebServer server;

    server.init(config.PORT, user, passwd, databasename, config.LOGWrite,
                config.OPT_LINGER, config.TRIGMode, config.sql_num,
                config.thread_num, config.close_log, config.actor_model);

    //初始化日志
    server.log_write();

    //初始化数据库连接
    server.sql_pool();

    //初始化线程池
    server.thread_pool();

    //初始化触发模式
    server.trig_mode();

    //初始化监听socket和epoll
    server.eventListen();
    cout << "监听设置成功" << endl;
    //运行
    server.eventLoop();

    return 0;
}