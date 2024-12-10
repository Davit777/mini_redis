#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
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
    fprintf(stderr, "%s", msg);
    fflush(stderr);
}

static inline void die(const char* msg)
{
    const int err = errno;
    fprintf(stderr, "[%d] %s", err, msg);
    fflush(stderr);
    abort();
}

static inline int32_t read_full(const int fd, char* buf, size_t n)
{
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static inline int32_t write_all(const int fd, char* buf, size_t n)
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

static inline int32_t send_req(const int fd, const char* text)
{
    const uint32_t len = (uint32_t)strlen(text);
    if (len > MAX_MESSAGE_SIZE) {
        return -1;
    }
    char wbuf[HEADER_SIZE + MAX_MESSAGE_SIZE];
    memcpy(wbuf, &len, HEADER_SIZE);
    memcpy(&wbuf[HEADER_SIZE], text, len);
    return write_all(fd, wbuf, HEADER_SIZE + len);
}

static inline int32_t read_res(const int fd)
{
    char rbuf[HEADER_SIZE + MAX_MESSAGE_SIZE + 1];
    errno = 0;
    int32_t err = read_full(fd, rbuf, HEADER_SIZE);
    if (0 != err) {
        if (0 == errno) {
            msg("EOF");
        } else {
            msg("read() error");
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, HEADER_SIZE);
    if (len > MAX_MESSAGE_SIZE) {
        msg("too long");
        return -1;
    }

    err = read_full(fd, &rbuf[HEADER_SIZE], len);
    if (err) {
        msg("read() error");
        return err;
    }

    rbuf[HEADER_SIZE + len] = '\0';
    printf("server says: %s\n", &rbuf[HEADER_SIZE]);
    return 0;
}

int main()
{
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);
    int rv = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
    if (rv) {
        die("connect ()");
    }

    const char* query_list[] = {"hello1", "hello2", "hello3"};
    const size_t N = sizeof(query_list) / sizeof(query_list[0]);
    for (size_t i = 0; i < N; ++i) {
        const int32_t err = send_req(fd, query_list[i]);
        if (0 != err) {
            close(fd);
            return 0;
        }
    }
    for (size_t i = 0; i < N; ++i) {
        const int32_t err = read_res(fd);
        if (0 != err) {
            close(fd);
            return 0;
        }
    }

    return 0;
}

