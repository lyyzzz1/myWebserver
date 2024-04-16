#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "../CGImysql/sql_connection_pool.h"
#include "../lock/locker.h"
#include <cstdio>
#include <exception>
#include <list>
#include <pthread.h>

template <typename T> class threadpool {
  private:
    int m_thread_number;       //线程数
    int m_max_requests;        //请求队列中允许的最大请求数
    pthread_t* m_threads;      //线程池数组,size = m_thread_number
    std::list<T*> m_workqueue; //请求队列
    locker m_queuelocker;      //对于请求队列的互斥锁
    sem m_queuestat;
    connection_pool* m_connPool; //数据库
    int m_actor_model;           //模型
  public:
    threadpool(int actor_model, connection_pool* connPool,
               int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    //两个函数为向请求队列中添加请求
    bool append(T* request, int state); // state为http类中的成员，0为读，1为写
    bool append_p(T* request);

  private:
    static void* worker(void* arg);
    void run();
};

template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool* connPool,
                          int thread_number, int max_requests)
    : m_thread_number(thread_number), m_max_requests(max_requests),
      m_actor_model(actor_model),m_connPool(connPool),m_threads(NULL) {
    if (thread_number <= 0 || max_requests <= 0) {
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads) {
        throw std::exception();
    }
    for (int i = 0; i < m_thread_number; ++i) { //循环创建number个线程
        if (pthread_create(&m_threads[i], NULL, worker, this) != 0) {
            delete[] m_threads;
            throw std::exception();
        }else{
            cout<<"the threadpool create thread "<<m_threads[i]<<" success"<<endl; //输出线程创建成功的信息
        }
        if (pthread_detach(m_threads[i])) { //将子线程与主线程进行分离
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T> threadpool<T>::~threadpool() { delete[] m_threads; }

template <typename T>
bool threadpool<T>::append_p(T* request) { //向任务队列中加入对应任务
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post(); //信号量+1
    return true;
}

template <typename T> bool threadpool<T>::append(T* request, int state) {
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post(); //信号量+1
    return true;
}

template <typename T> void* threadpool<T>::worker(void* arg) {
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template <typename T> void threadpool<T>::run() {
    while (true) {
        m_queuestat.wait();
        m_queuelocker.lock();
        cout << "我是线程" << pthread_self() << "，我开始工作了，我获取的http连接的sockfd是："
             << m_workqueue.front()->get_sockfd() << endl;

        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request) {
            continue;
        }
        if (m_actor_model == 1) {        // 1为reactor
            if (request->m_state == 0) { //读
                if (request->read_once()) {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                } else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            } else {
                if (request->write()) {
                    request->improv = 1;
                } else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        } else { // 0为Proactor
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }
    }
}

#endif