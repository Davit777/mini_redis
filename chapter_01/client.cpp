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

static void die(const char* msg)
{
    const int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    fflush(stderr);
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
    const int rv = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
    if (0 != rv) {
        die("connect()");
    }

    sleep(10);
    const char msg[] = "hello";
    write(fd, msg, strlen(msg));

    char rbuf[SMALL_BUFFER_SIZE];
    const ssize_t N = read(fd, rbuf, sizeof(rbuf) - 1);
    if (N < 0) {
        die("read");
    }
    printf("server says: %s\n", rbuf);

    return 0;
}

