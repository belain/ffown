#include "detail/acceptor_impl.h"
#include "detail/epoll_impl.h"
#include "utility/thread.h"

#include <iostream>
using namespace std;

#include<pthread.h>
#include<stdio.h>

void *thr_fn(void *arg)
{
    epoll_impl_t* p = (epoll_impl_t*)arg;
    p->event_loop();
    return NULL;
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        cout <<argv[0] <<" useage listen:host port\n";
        return 1;
    }
    char buff[128];
    snprintf(buff, sizeof(buff), "tcp://%s:%s", argv[1], argv[2]);
    pthread_t ntid;

    int ret = 0;
    epoll_impl_t epoll;

    ret = pthread_create(&ntid, NULL, thr_fn, &epoll);
    
    acceptor_impl_t acceptor(&epoll);
    ret = acceptor.open(string(buff));

    if (ret)
    {
        cout <<"acceptor open failed:" << buff <<"\n";
        return 1;
    }
    else
    {
        cout <<"acceptor open ok, wait to listen\n";
    }

    pthread_join(ntid, NULL);
    
	return 0;
}

