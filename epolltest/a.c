#include <sys/socket.h>  
#include <sys/epoll.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  
#include <fcntl.h>  
#include <unistd.h>  
#include <stdio.h>  
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/select.h>

#define proc_file "/proc/toskmsg"
#define proc_kmsg "/proc/kmsg"
#define user_file "/mnt/hgfs/share/bb/tmp.txt"
#define MAX_EVENTS 50

struct epoll_event events[50];
int main()
{
    char buf[2048];
    struct epoll_event epv = {0, {0}};

    int g_epollFd = epoll_create(MAX_EVENTS);
    if(g_epollFd <= 0) printf("create epoll failed.%d\n", g_epollFd);

    int fd = open(proc_file, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        printf("open file failed\n");
        return 0;
    }

    epv.events = EPOLLIN;
    epv.data.fd = fd;
    if(epoll_ctl(g_epollFd, EPOLL_CTL_ADD, fd, &epv) < 0)
        printf("epoll ctl fild!\n");

    while(1) {
        int nfds = epoll_wait(g_epollFd,events,20,0);
        for(int i=0;i<nfds;++i) {
            if(events[i].events & EPOLLIN) {
                memset(buf, 0, sizeof(buf));
                int ret = read(fd, buf, sizeof(buf) - 1);
                if(ret > 0)
                    printf("%s", buf);
                //lseek(fd, 0, SEEK_SET);
            }
            else 
                printf("22222\n");
        }
    }
    return 0;
}
