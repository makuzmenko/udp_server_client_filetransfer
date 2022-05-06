#ifndef MCLIENT_H
#define MCLIENT_H

#include <vector>
#include <map>
#include <sys/types.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#include <netinet/in.h>
#include <sys/epoll.h>
#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <algorithm>
#include <fcntl.h>
#include <cstring>
#include <arpa/inet.h>
#include <streambuf>
#include <iostream>
#include <sstream>
#include <functional>
#include <mutex>

#define EPOLL_SIZE 10000
#define BUF_SIZE 2048

struct udp_client_callbacks
{
    std::function<void(int, struct sockaddr_in*)> on_connect = NULL;
    std::function<void(char* buf, int len)> on_read = NULL;
    std::function<void(int)> on_disconnect = NULL;
};

class udp_client
{
public:
    udp_client(std::string addr, uint16_t port, int type) : fd(0) {
        if(type != SOCK_DGRAM) throw std::runtime_error("TCP does not support\n");

        fd = socket(AF_INET,type,0);
        if(fd<0){
            throw std::runtime_error("cannot open socket\n");
        }

        bzero(&to,sizeof(to));
        to.sin_family = AF_INET;
        to.sin_addr.s_addr = inet_addr(addr.c_str());
        to.sin_port = htons(port);

        fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0)|O_NONBLOCK);

        ev.events = EPOLLIN;
        ev.data.fd = fd;

        if((epfd = epoll_create(EPOLL_SIZE)) < 0)
        {
            throw std::runtime_error("cannot create epoll\n");
        }
        if(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0)
        {
            throw std::runtime_error("cannot add socket to epool\n");
        }
        //ctor
    }

    virtual ~udp_client(){
        close(fd);
        //dtor
    }

    void set_cb(udp_client_callbacks h){
        handler = h;
    }

    //virtual void delete_ss(unsigned port);
    virtual bool do_cs(){
        char message[BUF_SIZE];
        struct epoll_event events[2];
        int epoll_events_count = epoll_wait(epfd, events, 2, 100);
        for(int i = 0; i < epoll_events_count ; i++)
        {
            if(events[i].data.fd == fd)
            {
                bzero(&message, BUF_SIZE);
                struct sockaddr_in si_other;
                socklen_t slen = sizeof(si_other);
                //int res = recv(events[i].data.fd, message, BUF_SIZE, 0);
                int res = recvfrom(events[i].data.fd, message, BUF_SIZE, 0, (struct sockaddr *) &si_other, &slen);
                if(res < 0){}
                else if (res == 0){}
                else
                {
                    std::lock_guard<std::mutex> guard(mutex);

                    if(handler.on_read != NULL)
                    {
                        handler.on_read(&message[0], res);
                    }
                }
            }
        }
        return true;
    }
    virtual bool send(const char* msg, int len){
        if (sendto(fd, msg, len, 0, (sockaddr*)&to, sizeof(to)) < 0)
        {
            perror("cannot send message");
            return false;
        }
        return true;
    }

    virtual void flush_callBacks(){
        std::lock_guard<std::mutex> guard(mutex);
        handler = udp_client_callbacks();
    }


protected:

private:
    struct sockaddr_in to;
    struct epoll_event ev = {0,0};
    int epfd;
    int fd;
    udp_client_callbacks handler;
    std::mutex mutex;
};
#endif // MCLIENT_H
