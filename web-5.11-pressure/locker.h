#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

//线程同步机制类

//互斥锁类
class locker{
public:
    locker();                   //初始化锁
    ~locker();                  //释放锁资源
    bool lock();                //加锁
    bool unlock();              //解锁
    pthread_mutex_t * get();    //获取锁
private:
    pthread_mutex_t m_mutex;
};

//条件变量类
class cond{
public:
    cond();
    ~cond();
    bool wait(pthread_mutex_t * m_mutex);                           //阻塞等待变量，等待时释放锁，唤醒时，获取锁
    bool timewait(pthread_mutex_t * m_mutex, struct timespec t);    //限定时间等待变量
    bool signal();                                                  //唤醒等待变量
    bool broadcast();                                               //唤醒所有等待变量
private:
    pthread_cond_t m_cond;
};

//信号量类
class sem{
public:
    sem();
    sem(int num);
    ~sem();
    bool wait();    //消耗信号量
    bool post();    //增加信号量
private:
    sem_t m_sem;
};

#endif