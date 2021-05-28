#ifndef SIGNALSSET_H
#define SIGNALSSET_H

#include <string.h>
#include <signal.h>
#include <assert.h>

void addsig(int sig, void( handler )(int), int flag)   //将sig信号的处理函数设置位handler，并将sa_flags与flag做与‘|’操作
{
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = handler;
    sa.sa_flags |= flag;
    sigfillset( &sa.sa_mask );    //在处理该信号时，临时阻塞其他信号
    assert( sigaction( sig, &sa, NULL ) != -1 );
}

/*
void addsig_ReStart( int sig )
{
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = sig_handler_pipe;
    sa.sa_flags |= SA_RESTART;
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );
}

void addsig(int sig, void( handler )(int)){
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = handler;
    sigfillset( &sa.sa_mask );    //在处理该信号时，临时阻塞其他信号
    assert( sigaction( sig, &sa, NULL ) != -1 );
}
*/


#endif