#ifndef BUDPSERVER_H
#define BUDPSERVER_H

#include <vector>
#include <map>
#include <sys/types.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#include <netinet/in.h>
#include <netinet/ip.h>
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
//#include <concepts>

#define MAX_BUF 65536
#define MAX_EVENTS 1024
#define EPOLL_SIZE 10000

/*
ss - server socket
cs - client socket
*/

typedef struct sockInfo
{
    int socket;
    struct sockaddr_in saddr;

}sockInfo;

/*template <typename T> concept cSockPtr =
  requires(T m, int i) {
	{ m.get_fd() } -> std::convertible_to<int>;
	m.set_fd(i);
};
*/

class sockPtr
{
public:
    sockPtr(): fd(0), data_ptr(0), onRead_cb(NULL) {}
    explicit sockPtr(int& a): fd(a), data_ptr(0), onRead_cb(NULL) {}
    explicit sockPtr(const int& a): fd(a), data_ptr(0), onRead_cb(NULL) {}

    operator int() const {
        return fd;
    }

    sockPtr& operator=(const int& rv) {
        fd = rv;
        return *this;
    }

    int get_fd(){return fd;}
    void set_fd(int a){fd = a;}

    void onRead(const struct sockaddr_in *s, char *buf, int len)
    {
        std::lock_guard<std::mutex> g(onRead_mutex);
        if(onRead_cb)
            onRead_cb(this, s, buf, len);
    }

    void set_onRead(std::function<void(sockPtr*, const struct sockaddr_in *, char *buf, int len)> f)
    {
        std::lock_guard<std::mutex> g(onRead_mutex);
        onRead_cb = f;
    }

    void *get_dataPtr()
    {
        std::lock_guard<std::mutex> g(dataPtr_mutex);
        return data_ptr;
    }

    void set_dataPtr(void* ptr)
    {
        std::lock_guard<std::mutex> g(dataPtr_mutex);
        data_ptr = ptr;
    }

private:
    int fd;
    std::mutex onRead_mutex;
    std::mutex dataPtr_mutex;
    void *data_ptr;

    std::function<void(sockPtr*, const struct sockaddr_in *, char *buf, int len)> onRead_cb;

};


/**
* Это очень урезанный класс из моего коммерческого проекта.
* Позволяет открывать множество портов как UDP так и TCP и
* обрабатывать их в одном потоке с epoll
*/
template<class T>
class bUDPServer
{
public:

    bUDPServer(): stop(false),sock_error(-1) {};
    virtual ~bUDPServer()
    {
        for(auto &a:sock_to_ptr)
        {
            delete (T*)a.second;
        }
    }

    virtual void set_default_onRead(std::function<void(T*, const struct sockaddr_in *, char *buf, int len)> onRead_cb)
    {
        default_onRead = onRead_cb;
    }

    virtual auto get_default_onRead() -> std::function<void(T*, const struct sockaddr_in *, char *buf, int len)>
    {
        return default_onRead;
    }

    virtual bool init(/*mServerCallbacks handler, */ int _backlog)
    {
        std::lock_guard<std::mutex> guard(mutex);
        backlog = _backlog;
        //handlers = handler;

        /*FD_ZERO(&ss_set);
        FD_ZERO(&cs_set);
        FD_ZERO(&tcp_set);*/

        epfd = epoll_create(EPOLL_SIZE);
        if (epfd < 0)
        {
            perror("epoll create");
            return false;
        }

        return true;
    }

    //virtual int add_ss(int type, unsigned port);
    virtual T* add_ss(/*int type, */unsigned port, void * ptr = NULL)
    {
        int ss;
        /*switch(type)
        {
        case SOCK_STREAM:
            ss = create_tcp_ss(port);
            break;
        case SOCK_DGRAM:
            ss= create_upd_ss(port);
            break;
        default:
            return -1;
        }*/

        ss= create_udp_ss(port);

        if (ss < 0)
        {
            perror("listen or bind");
            return &sock_error;
        }

        T *obj = new T();
        *obj = ss;

        struct epoll_event ev = {0,0};
        ev.events = EPOLLIN;

        sock_to_ptr[ss] = obj;

        if(ptr)
            obj->set_dataPtr(ptr);

        if(get_default_onRead())
            obj->set_onRead(get_default_onRead());

        ev.data.ptr = obj;

        int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, ss, &ev);
        if (ret < 0)
        {
            perror("epoll_ctl");
            return &sock_error;
        }

        return obj;
    }

    virtual void close_ss(T* a)
    {
        if(!a) return;

        int ss = *a;

        sock_to_ptr.erase(ss);

        struct epoll_event ev;
        //ev.data.fd = ss;
        ev.data.ptr = a;
        epoll_ctl(epfd, EPOLL_CTL_DEL, ss, &ev);
        close(ss);

        delete a;
    }

    virtual void close_ss(int ss)
    {
        close_ss((T*)sock_to_ptr[ss]);
    }

    //virtual void delete_ss(unsigned port);
    virtual bool do_ss()
    {
        int ready;
        struct epoll_event events[EPOLL_SIZE];

        ready = epoll_wait(epfd, events, EPOLL_SIZE, 100);
        if (ready < 0)
        {
            perror("epoll_wait");
            return false;
        }
        else if (ready == 0)
        {
            return !stop;
        }
        else
        {
            for (int i = 0; i < ready; i++)
            {

                recv_udp_data(events[i]);

            }
        }
        return !stop;
    }

    virtual bool send_udp(int fd, const struct sockaddr_in *claddr, char* buf, int len)
    {
        if (sendto(fd, buf, len, 0,
               (sockaddr*)claddr, sizeof(sockaddr_in)) < 0)
        {
            perror("cannot send message");
            return false;
        }
        return true;
    }

protected:
    //virtual int create_tcp_ss(unsigned port);
    virtual int create_udp_ss(unsigned port)
    {
        return create_abs_ss(SOCK_DGRAM,port);
    }
    //virtual void accept_cs(int ss);
    //virtual void close_cs(int cs);
    //virtual void recv_tcp_data(struct epoll_event &);
    virtual void recv_udp_data(struct epoll_event &ee)
    {
        std::lock_guard<std::mutex> guard(mutex);

        T *a = (T*)ee.data.ptr;

        //int cs = ee.data.fd;
        int cs = *a;
        char recvbuf[MAX_BUF] = {0};
        int len;

        struct sockaddr_in clientaddr = sockaddr_in();
        socklen_t clilen = sizeof(struct sockaddr);

        len = recvfrom(cs, recvbuf, sizeof(recvbuf), 0, (struct sockaddr*)&clientaddr, &clilen);
        if(len>0)
        {
                a->onRead(&clientaddr, &recvbuf[0], len);
        }
    }
    int epfd = 0;
    int backlog = 0;
    std::atomic_bool stop;
    T sock_error;

private:
    virtual int create_abs_ss(int type, unsigned port)
    {
        int ret, ss;
        struct sockaddr_in seraddr;

        std::memset(&seraddr, 0, sizeof(struct sockaddr_in));
        seraddr.sin_family = AF_INET;
        seraddr.sin_addr.s_addr = INADDR_ANY;
        seraddr.sin_port = htons(port);

        ss = socket(AF_INET, type, 0);
        if (ss < 0)
        {
            perror("socket error");
            return -1;
        }

        if (setnonblocking(ss) < 0)
        {
            perror("setnonblock error");
        }

        ret = bind(ss, (struct sockaddr*)&seraddr, sizeof(struct sockaddr));
        if (ret < 0)
        {
            perror("bind");
            return -1;
        }


        return ss;
    }

    int setnonblocking(int ss)
    {

        int flags;
        if((flags = fcntl(ss, F_GETFD, 0)) < 0)
        {
            perror("get falg error\n");
            return -1;
        }

        flags |= O_NONBLOCK;
        //flags |= SO_REUSEADDR;
        //flags |= SO_REUSEPORT;
        if (fcntl(ss, F_SETFL, flags) < 0)
        {
            perror("set nonblock fail\n");
            return -1;
        }

        int enable = 1;
        if (setsockopt(ss, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        {
            perror("setsockopt(SO_REUSEADDR) failed\n");
            return -1;
        }

        int iptos = IPTOS_DSCP_EF;
        if(setsockopt(ss, IPPROTO_IP, IP_TOS, &iptos, sizeof(iptos)) < 0)
        {
            perror("set TOS fail\n");
        }


        return 0;
    }

    std::map<int, void*> sock_to_ptr;
    std::mutex mutex;
    std::function<void(T*, const struct sockaddr_in *, char *buf, int len)> default_onRead;

};

typedef bUDPServer<sockPtr> udp_server;

#endif // BUDPSERVER_H
