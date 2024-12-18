#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#define HEADER_SIZE           4

#define SMALL_BUFFER_SIZE     64
#define MAX_MESSAGE_SIZE      4096

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

static inline int32_t read_full(const int fd, char* buf, size_t n)
{
    while (n > 0) {
        const ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }

    return 0;
}

static inline int32_t write_all(const int fd, const char* buf, size_t n)
{
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }

    return 0;
}

static inline int32_t one_request(const int connfd)
{
    char rbuf[HEADER_SIZE + MAX_MESSAGE_SIZE + 1];
    errno = 0;
    int32_t err = read_full(connfd, rbuf, HEADER_SIZE);
    if (err) {
        if (0 == errno) {
            msg("EOF");
        } else {
            msg("read() error");
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, HEADER_SIZE); // little endian
    if (len > MAX_MESSAGE_SIZE) {
        msg("too long");
        return -1;
    }

    err = read_full(connfd, &rbuf[HEADER_SIZE], len);
    if (err) {
        msg("read() error");
        return -1;
    }
    rbuf[HEADER_SIZE + len] = '\0';
    printf("client says: %s\n", &rbuf[HEADER_SIZE]);

    // Reply
    const char reply[] = "world";
    char wbuf[HEADER_SIZE + sizeof(reply)];
    len = (uint32_t)strlen(reply);
    memcpy(wbuf, &len, HEADER_SIZE);
    memcpy(&wbuf[HEADER_SIZE], reply, len);
    return write_all(connfd, wbuf, HEADER_SIZE + len);
}

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0);

    int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (0 != rv) {
        die("bind()");
    }

    rv = listen(fd, SOMAXCONN);
    if (0 != rv) {
        die("listen()");
    }

    while (true) {
        struct sockaddr_in client_addr = {};
        socklen_t socklen = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
        if (connfd < 0) {
            continue;
        }

        while (true) {
            int32_t err = one_request(connfd);
            if (err) {
                break;
            }
        }
        close(connfd);
    }

    return 0;
}

