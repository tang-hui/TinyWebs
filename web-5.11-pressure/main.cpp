#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"
#include "lst_timer.h"
#include "signals_set.h"


#define MAX_FD 65536            // 最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000  // 监听的最大的事件数量
#define TIMESLOT 5              //定时器时长，定期关闭长时间没有消息传输的连接


static int pipefd[2];
static sort_timer_lst timer_lst;  


// 添加文件描述符
extern void addfd( int epollfd, int fd, bool one_shot );
extern void removefd( int epollfd, int fd );
extern int setnoblocking(int fd);


//信号操作函数：将触发该函数的信号输入到管道中
void sig_handler_pipe( int sig )    
{
    int save_errno = errno;
    int msg = sig;
    send( pipefd[1], ( char* )&msg, 1, 0 );
    errno = save_errno;
}

//定时器处理函数
void timer_handler()        
{
    // 定时处理任务，实际上就是调用tick()函数
    timer_lst.tick();
    // 因为一次 alarm 调用只会引起一次SIGALARM 信号，所以我们要重新定时，以不断触发 SIGALARM信号。
    alarm(TIMESLOT);
}


int main( int argc, char* argv[] ) {
    
    if( argc <= 1 ) {
        printf( "usage: %s port_number\n", basename(argv[0]));
        return 1;
    }

    int port = atoi( argv[1] );
    addsig( SIGPIPE, SIG_IGN, 0);  //当服务器向一个关闭的客户端发送信息就会产生SIGPIPE信号，该信号的默认处理方式是关闭本进程

    threadpool< http_conn >* pool = NULL;
    try {
        pool = new threadpool<http_conn>;
    } catch( ... ) {
        return 1;
    }

    http_conn* users = new http_conn[ MAX_FD ];

    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );

    // 端口复用
    int reuse = 1;
    setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );

    struct sockaddr_in address;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_family = AF_INET;
    address.sin_port = htons( port );

    int ret = 0;
    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    if(ret == -1){
        perror("bind");
        exit(-1);
    }
    ret = listen( listenfd, 5 );
    if(ret == -1){
        perror("listen");
        exit(-1);
    }

    // 创建epoll对象，和事件数组，添加
    epoll_event events[ MAX_EVENT_NUMBER ];
    int epollfd = epoll_create( 5 );
    // 添加到epoll对象中
    addfd( epollfd, listenfd, false );
    http_conn::m_epollfd = epollfd;


    
    //创建管道,定时器周期性的产生信号，将信号写入管道，触发epoll事件
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert( ret != -1 );
    setnoblocking( pipefd[1] );
    addfd( epollfd, pipefd[0], false );

    addsig(SIGALRM, sig_handler_pipe, SA_RESTART);
    bool timeout = false;
    //开启定时器
    alarm(TIMESLOT);
    

    while(true) {
        
        int number = epoll_wait( epollfd, events, MAX_EVENT_NUMBER, -1 );
        
        if ( ( number < 0 ) && ( errno != EINTR ) ) {
            printf( "epoll failure\n" );
            break;
        }

        for ( int i = 0; i < number; i++ ) {
            
            int sockfd = events[i].data.fd;
            
            if( sockfd == listenfd ) {
                
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof( client_address );
                int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );
                
                if ( connfd < 0 ) {
                    printf( "errno is: %d\n", errno );
                    continue;
                } 
                if( http_conn::m_user_count >= MAX_FD ) {
                    close(connfd);
                    continue;
                }
                
                users[connfd].init( connfd, client_address);

                util_timer * timer = new util_timer;
                timer->user_data = &users[connfd];
                //timer->cb_func = cb_function;
                time_t cur_time = time(NULL);
                timer->expire = cur_time + 3*TIMESLOT;
                users[connfd].timer = timer;
                timer_lst.add_timer( timer );

            } else if( events[i].events & ( EPOLLRDHUP | EPOLLHUP | EPOLLERR ) ) { 
				//EPOLLRDHUP: 流套接字对端关闭连接，或关闭写入一半连接。
                //EPOLLHUP： 套接字文件描述符关闭
                //EPOLLERR： 套接字文件描述符错误
                //printf("error fd close!\n");
                timer_lst.del_timer(users[sockfd].timer);   //从超时时间链表中删除该节点
                users[sockfd].close_conn();                 //关闭对应的节点

            } else if((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)){            
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if(ret <= 0){
                    continue;
                }
                else{
                    for(int i = 0; i < ret; ++i){
                        switch( signals[i] ){
                            case SIGALRM:
                            {
                                timeout = true;
                                break;
                            }
                            case SIGTERM:
                            {
                                //stop_server = true;
                            }
                        }
                    }
                }
            }  
            else if(events[i].events & EPOLLIN) {

                if(users[sockfd].read()) {
                    pool->append(users + sockfd);
                } else {
                    timer_lst.del_timer(users[sockfd].timer);   //从超时时间链表中删除该节点
                    users[sockfd].close_conn();
                }

            }  else if( events[i].events & EPOLLOUT ) {

                if( !users[sockfd].write() ) {
                    timer_lst.del_timer(users[sockfd].timer);   //从超时时间链表中删除该节点
                    users[sockfd].close_conn();
                }

            }
        }
        
        if( timeout ){
            timer_handler();
            timeout = false;
        }
        
    }
    
    close( epollfd );
    close( listenfd );
    delete [] users;
    delete pool;
    return 0;
}