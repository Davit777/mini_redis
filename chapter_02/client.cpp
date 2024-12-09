#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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

static inline int32_t query(const int fd, const char* text)
{
    uint32_t len = (uint32_t)strlen(text);
    if (len > MAX_MESSAGE_SIZE) {
        return -1;
    }

    char wbuf[HEADER_SIZE + MAX_MESSAGE_SIZE];
    memcpy(wbuf, &len, HEADER_SIZE);
    memcpy(&wbuf[HEADER_SIZE], text, len);
    const int32_t err = write_all(fd, wbuf, HEADER_SIZE + len);
    if (0 != err) {
        return err;
    }

    char rbuf[HEADER_SIZE + MAX_MESSAGE_SIZE + 1];
    errno = 0;
    int32_t rc = read_full(fd, rbuf, HEADER_SIZE);
    if (0 != rc) {
        if (0 == errno) {
            msg("EOF");
        } else {
            msg("read() error");
        }
        return rc;
    }

    memcpy(&len, rbuf, HEADER_SIZE);
    if (len > MAX_MESSAGE_SIZE) {
        msg("too long");
        return -1;
    }

    rc = read_full(fd, &rbuf[HEADER_SIZE], len);
    if (0 != rc) {
        msg("read() error");
        return rc;
    }

    rbuf[HEADER_SIZE + len] = '\0';
    printf("server says: %s\n", &rbuf[HEADER_SIZE]);

    return 0;
}

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);
    int rv = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
    if (rv) {
        die("connect()");
    }

    int32_t err = query(fd, "hello1");
    if (0 != err) {
        close(fd);
        return 0;
    }

    err = query(fd, "hello2");
    if (0 != err) {
        close(fd);
        return 0;
    }

    err = query(fd, "hello3");
    if (0 != err) {
        close(fd);
        return 0;
    }

    return 0;
}

