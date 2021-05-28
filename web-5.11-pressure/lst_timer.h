#ifndef LST_TIMER
#define LST_TIMER

#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>
#include "http_conn.h"

class http_conn;

class util_timer{
public:
    util_timer(): prev(NULL),next(NULL){} 
public:
    time_t expire;          //任务超时时间
    http_conn * user_data;  //指向本事件
    util_timer * prev;      //头节点
    util_timer * next;      //尾节点
};


class sort_timer_lst{
public:
    sort_timer_lst() : head(NULL),tail(NULL){}
    ~sort_timer_lst();
    void add_timer( util_timer * timer );
    void del_timer( util_timer * timer );
    void adjust_timer( util_timer * timer );
    void tick();        //SIGALARM信号每次被触发就在其信号处理函数中调用一次tick函数将任务链表中到期任务清除掉
private:
    void add_timer( util_timer * timer, util_timer * lst_head );    //将timer加到lst_head节点后面
private:
    util_timer * head;
    util_timer * tail;
};

#endif