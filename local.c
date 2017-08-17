/*
*   local.c
*   Author: mengyaoss77
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <string.h>

#include "local.h"

#define MAXSIZE 1500

void forward(int clientfd, int serverfd)
{
    char crbuf[MAXSIZE], csbuf[MAXSIZE];
    char srbuf[MAXSIZE], ssbuf[MAXSIZE];
    int maxfd;
    int readn, sendn;
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(clientfd, &rset);
    FD_SET(serverfd, &rset);
    if (clientfd > serverfd) {
        maxfd = clientfd;
    } else {
        maxfd = serverfd;
    }
    while (1) {
        select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (FD_ISSET(clientfd, &rset)) {
            while ((readn = recv(clientfd, crbuf, MAXSIZE, MSG_DONTWAIT)) > 0) {
                sendn = send(serverfd, crbuf, readn, 0);
                printf("read %d from client, send %d to server\n", readn, sendn);
            }
        }
        if (FD_ISSET(serverfd, &rset)) {
            while((readn = recv(serverfd, srbuf, MAXSIZE, MSG_DONTWAIT)) > 0) {
                sendn = send(clientfd, srbuf, readn, 0);
                printf("read %d from server, send %d to client\n", readn, sendn);
            }
        }
        if(readn == 0) break;

        FD_ZERO(&rset);
        FD_SET(clientfd, &rset);
        FD_SET(serverfd, &rset);
        
    }
}

void *s_connect(void *skfd)
{
    int fd = *(int *)skfd;
    char rbuf[MAXSIZE], sbuf[MAXSIZE], *p;
    uint8_t *s_ver;
    uint8_t *s_nmethods;
    char *methods;
    struct sockaddr_in sa_in;
    struct socksrequest request;
    struct socksresponse response;

    int i;
    //hand shake
    if (recv(fd, rbuf, MAXSIZE, 0) <= 0) {
        printf("recv error\n");
        close(fd);
    }
    s_ver = (uint8_t *)rbuf;
    s_nmethods = s_ver + 1;
    methods = (char *)(s_nmethods + 1);
    sbuf[0] = 0x05;
    sbuf[1] = 0x00;
    if (send(fd, sbuf, 2, 0) == 2) {
        printf("hand shake success\n");
    }
    //request and response
    int readn = recv(fd, rbuf, MAXSIZE, 0);
    
    request.s_ver = rbuf[0];
    request.s_cmd = rbuf[1];
    if (request.s_cmd != CMD_CONNECT) {
        printf("UNSUPPORT PROTOCOL, THREAD EXIT\n");
        pthread_exit((void *)1);
    }
    request.s_rsv = 0x00;
    request.s_atyp = rbuf[3];
    if (request.s_atyp == ATYP_4) {
        printf("type is ipv4\n");
        request.addr = rbuf + 4;
        request.s_port = *(uint16_t *)(rbuf + 8);
        sa_in.sin_family = AF_INET;
        sa_in.sin_port = request.s_port;
        sa_in.sin_addr = *(struct in_addr *)request.addr;
    } else {
        printf("type is name\n");
        int namelen = rbuf[4];
        request.s_port = *(uint16_t *)(rbuf + 4 + namelen);
        request.addr = (char *)malloc(namelen);
        memcpy(request.addr, (char *)(rbuf + 5), namelen);
        struct addrinfo hint = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
            .ai_protocol = IPPROTO_TCP,
        };
        struct addrinfo *res, *aip;
        int err = getaddrinfo(request.addr, NULL, &hint, &res);
        for (aip = res; aip != NULL; aip = res->ai_next) {
            sa_in = *(struct sockaddr_in *)aip->ai_addr;
        }
    }
    //connect to destinaton server and response client.
    int localfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    response.s_ver = VER_SOCKS5;
    response.s_rsv = RSV;
    response.s_atyp = ATYP_4;
    memcpy(response.addr, (char *)&sa_in.sin_addr, 4); //ADDR
    response.s_port = sa_in.sin_port;

    if (connect(localfd, (struct sockaddr *)&sa_in, sizeof(struct sockaddr)) < 0) {
        printf("connect dst server failed, close connection.\n");
        response.s_rep = REP_CONNECTFAIL;
        memcpy(sbuf, (char *)&response, sizeof(struct socksresponse));
        send(fd, sbuf, sizeof(struct socksresponse), 0);
        close(fd);
        close(localfd);
        pthread_exit((void *)0);
    }else {
        response.s_rep = REP_SUCCESSS; //REP
        memcpy(sbuf, (char *)&response, sizeof(struct socksresponse));
        send(fd, sbuf, sizeof(struct socksresponse), 0);
    }
    // Connect to dst server successly.
    // NOW: client fd is 'fd' , dst fd is 'localfd'
    
    printf("Connect to dst server successly. \n");
    forward(fd, localfd);
    close(fd);
    close(localfd);
    printf("Forward over, thread exit.\n");
}

int main(int argc, char *argv)
{
    printf("Start local socks5 server...\n");
    struct addrinfo hint = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
    };
    struct addrinfo *res, *aip;

    struct sockaddr *sa, *client;
    socklen_t slen;
    int listenfd;

    int err = getaddrinfo("127.0.0.1", "1080", &hint, &res);

    if (err) {
        const char *errstr = gai_strerror(err);
        printf("GETADDRINFO ERROR: %s", errstr);
        exit(1);
    }
    for (aip = res; aip != NULL; aip = res->ai_next) {
        //printf("%d %d\n", aip->ai_family, aip->ai_protocol);
        sa = aip->ai_addr;
        listenfd = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
        int yes = 1;
        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            printf("SETSOCKETOPT ERROR\n");
            exit(1);
        }
    }
    if (bind(listenfd,  sa, sizeof(struct sockaddr))) {
        printf("BIND ERROR\n");
        close(listenfd);
        exit(1);
    }
    
    if (listen(listenfd, 10)) {
        printf("LISTEN ERROR\n");
        close(listenfd);
        exit(1);
    }

    while (1) {
        printf("WAITING FOR SOCKS CLIENT...\n");
        int newfd = accept(listenfd, client, &slen);
        if (newfd == -1) {
            printf("ACCEPT ERROR\n");
            close(listenfd);
            exit(1);
        }
        pthread_t tid;
        if (pthread_create(&tid, NULL, s_connect, (void *)&newfd) != 0) {
            printf("THREAD CREATE ERROR:\n");
            close(newfd);
            close(listenfd);
            exit(1);
        }
        printf("Accept success, into Thread: %d\n", tid);
    }

}