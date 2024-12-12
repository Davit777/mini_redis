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
#include <string>
#include <vector>

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

static inline int32_t send_req(const int fd, const std::vector<std::string>& cmd)
{
    uint32_t len = HEADER_SIZE;
    for (size_t i = 0; i < cmd.size(); ++i) {
        len += HEADER_SIZE + cmd[i].size();
    }
    if (len > MAX_MESSAGE_SIZE) {
        return -1;
    }

    char wbuf[HEADER_SIZE + MAX_MESSAGE_SIZE];
    memcpy(&wbuf, &len, HEADER_SIZE);
    const uint32_t n = cmd.size();
    memcpy(&wbuf[HEADER_SIZE], &n, HEADER_SIZE);
    size_t cur = HEADER_SIZE + HEADER_SIZE;
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t p = cmd[i].size();
        memcpy(&wbuf[cur], &p, HEADER_SIZE);
        memcpy(&wbuf[cur + HEADER_SIZE], cmd[i].data(), cmd[i].size());
        cur += HEADER_SIZE + cmd[i].size();
    }
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
    if (0 != err) {
        msg("read() error");
    }

    uint32_t rescode = 0;
    if (len < HEADER_SIZE) {
        msg("bad response");
        return -1;
    }
    memcpy(&rescode, &rbuf[HEADER_SIZE], HEADER_SIZE);
    printf("server says: [%u] %.*s\n", rescode, len - 4, &rbuf[HEADER_SIZE + HEADER_SIZE]);

    return 0;
}

int main(int argc, char* argv[])
{
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);
    const int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (0 != rv) {
        die("connect()");
    }

    std::vector<std::string> cmd;
    for (int i = 1; i < argc; ++i) {
        cmd.push_back(argv[i]);
    }

    int32_t err = send_req(fd, cmd);
    if (0 != err) {
        close(fd);
        return 0;
    }

    err = read_res(fd);
    if (0 != err) {
        close(fd);
        return 0;
    }

    return 0;
}
