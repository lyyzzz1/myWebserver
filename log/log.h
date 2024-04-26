#ifndef _LOG_
#define _LOG_

#include "block_queue.h"
#include <iostream>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>

using namespace std;

class Log {
  public:
    static Log* getInstance() {
        static Log instance;
        return &instance;
    }

    static void* flush_log_thread(void* args) {
        Log::getInstance()->async_write_log();
        return nullptr;
    }

    bool init(const char* file_name, int close_log, int log_buf_size = 8192,
              int split_lines = 5000000, int max_queue_size = 0);

    void write_log(int level, const char* format, ...);

    void flush(void);

  private:
    Log();
    virtual ~Log();
    //异步写日志
    void* async_write_log() {
        string single_log;
        while (m_log_queue->pop(single_log)) {
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
        return nullptr;
    }

  private:
    char dir_name[128]; //路径名
    char log_name[128]; //文件名
    int m_split_lines;  //日志最大行数
    int m_log_buf_size; //日志缓冲区大小
    long long m_count;  //日志行数记录
    int m_today;        //记录今天是哪一天
    FILE* m_fp;         //文件指针
    char* m_buf;
    block_queue<string>* m_log_queue; //阻塞队列，生产者/消费者模型
    bool m_is_async;                  //标志位，判断同步异步
    locker m_mutex;
    int m_close_log; //关闭日志
};

#define LOG_DEBUG(format, ...)                                                 \
    if (m_close_log == 0) {                                                    \
        Log::getInstance()->write_log(0, format, ##__VA_ARGS__);               \
        Log::getInstance()->flush();                                           \
    }
#define LOG_INFO(format, ...)                                                  \
    if (m_close_log == 0) {                                                    \
        Log::getInstance()->write_log(1, format, ##__VA_ARGS__);               \
        Log::getInstance()->flush();                                           \
    }
#define LOG_WARN(format, ...)                                                  \
    if (m_close_log == 0) {                                                    \
        Log::getInstance()->write_log(2, format, ##__VA_ARGS__);               \
        Log::getInstance()->flush();                                           \
    }
#define LOG_ERROR(format, ...)                                                 \
    if (m_close_log == 0) {                                                    \
        Log::getInstance()->write_log(3, format, ##__VA_ARGS__);               \
        Log::getInstance()->flush();                                           \
    }

#endif