#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#define SMALL_BUFFER_SIZE 64

static inline void msg(const char* msg)
{
    fprintf(stderr, "%s\n", msg);
    fflush(stderr);
}

static inline void die(const char* msg)
{
    const int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    fflush(stderr);
    abort();
}

static inline void do_something(const int connfd)
{
    char rbuf[SMALL_BUFFER_SIZE] = {};
    const ssize_t N = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (N < 0) {
        msg("read() error");
        return;
    }
    printf("client says: %s\n", rbuf);

    const char wbuf[] = "world";
    write(connfd, wbuf, sizeof(wbuf));
}

int main()
{
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0);
    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
    if (0 != rv) {
        die("bind()");
    }

    rv = listen(fd, SOMAXCONN);
    if (0 != rv) {
        die("listen");
    }

    while (true) {
        struct sockaddr_in client_addr = {};
        socklen_t socklen = sizeof(client_addr);
        const int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
        if (connfd < 0) {
            continue;
        }
        do_something(connfd);
        close(connfd);
    }

    return 0;
}

