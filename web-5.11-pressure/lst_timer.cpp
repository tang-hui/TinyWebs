#include "lst_timer.h"


sort_timer_lst::~sort_timer_lst(){
    util_timer * temp = head;
    while( temp ){
        head = temp->next;
        delete temp;
        temp = head;
    }
}

void sort_timer_lst::add_timer( util_timer * timer ){
    if( !timer ) return;
    
    if( !head){
        head = tail =timer;
    }

    if(timer->expire < head->expire){
        timer->next = head;
        head->prev = timer;
        head = timer;
        return ;
    }
    add_timer(timer, head);
}

void sort_timer_lst::del_timer( util_timer * timer ){
    if(!timer) return;
    if((timer == head) && (timer == tail)){
        head = NULL;
        tail = NULL;
        delete timer;
        return;
    }
    if(timer == head){
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if(timer == tail){
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}


void sort_timer_lst::adjust_timer( util_timer * timer ){
    if(!timer)return;
    util_timer * temp = timer->next;
    if(!timer && (timer->expire < temp->expire))return;
    if(timer == head){
        head = head->next;
        timer->next = NULL;
        head->prev = NULL;
        add_timer(timer,head);
    }
    else{
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer,timer->next);
    }
}


void sort_timer_lst::tick(){
    if(!head) return;
    printf("timer tick\n");
    time_t cur_time = time( NULL );
    util_timer * temp = head;
    while( temp ){
        if( temp->expire > cur_time){
            break;
        }
        temp->user_data->close_conn();    //关闭socketfd
        head = head->next;  
        if(head){
            head->prev = NULL;            //从超时时间链表中删除该节点
        }
        delete temp;            
        temp = head;
    }
}


void sort_timer_lst::add_timer( util_timer * timer, util_timer * lst_head ){
    util_timer * pre_node = lst_head;
    util_timer * next_node = lst_head->next;
    while(next_node){
        if(timer->expire < next_node->expire){
            pre_node->next = timer;
            timer->prev = pre_node;
            next_node->prev = timer;
            timer->next = next_node;
            break;
        }
        pre_node = next_node;
        next_node = next_node->next;
    }
    if(!next_node){
        pre_node->next = timer;
        timer->prev = pre_node;
        timer->next = NULL;
        tail = timer;
    }
}