#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <cstdio>
#include <list>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
private:
    int m_thread_number;    //线程数
    int m_max_requests;     //请求队列中允许的最大请求数
    pthread_t *m_threads;   //线程池数组,size = m_thread_number
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker;   //对于请求队列的互斥锁
    sem m_queuestat;
    connection_pool* m_connPool;    //数据库
    int m_actor_model;      //模型
public:
    threadpool(int actor_model,connection_pool* connPool,int thread_number = 8,int max_requests = 10000);
    ~threadpool();
    bool append(T* request,int state);
    bool append_p(T* request);
private:
    static void* worker(void* arg);
    void run();
};

template <typename T>
threadpool<T>::threadpool(int actor_model,connection_pool* connPool,int thread_number,int max_requests):m_thread_number(thread_number),m_max_requests(max_requests),m_actor_model(actor_model)
{
    if(thread_number<=0||max_requests<=0){
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw std::exception();
    }
    for(int i = 0;i < m_thread_number; ++i){//循环创建number个线程
        if(pthread_create(&m_threads[i],NULL,worker,this) != 0){
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])){//将子线程与主线程进行分离
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

template <typename T>
bool threadpool<T>::append_p(T* request){//向任务队列中加入对应任务
    m_queuelocker.lock();
    if(m_workqueue.size()>=m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();//信号量+1
    return true;
}

template <typename T>
bool threadpool<T>::append(T* request,int state){
    m_queuelocker.lock();
    if(m_workqueue.size()>=m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
        m_queuestat.post();//信号量+1
    return true;
}

template <typename T>
void* threadpool<T>::worker(void* arg){
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run(){

}

#endif