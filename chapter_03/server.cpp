#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <vector>

#define HEADER_SIZE           4

#define SMALL_BUFFER_SIZE     64
#define MAX_MESSAGE_SIZE      4096

enum
{
    STATE_REQ = 0,
    STATE_RES = 1,
    STATE_END = 2,
};

struct Connection
{
    int fd;
    uint32_t state;
    size_t rbuf_size;
    uint8_t rbuf[HEADER_SIZE + MAX_MESSAGE_SIZE];
    size_t wbuf_size;
    size_t wbuf_sent;
    uint8_t wbuf[HEADER_SIZE + MAX_MESSAGE_SIZE];
};

static inline void msg(const char* msg)
{
    fprintf(stderr, "%s\n", msg);
    fflush(stderr);
}

static inline void die(const char* msg)
{
    const int err = errno;
    fprintf(stderr, "[%d] %s", err, msg);
    fflush(stderr);
    abort();
}

static inline void fd_set_nb(const int fd)
{
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (0 != errno) {
        die("fcntl error");
    }
    flags |= O_NONBLOCK;
    errno = 0;
    (void)fcntl(fd, F_SETFL, flags);
    if (0 != errno) {
        die("fcntl error");
    }
}

static inline void connection_put(std::vector<Connection*>& fd2connection, struct Connection* connection)
{
    if (fd2connection.size() <= (size_t)connection->fd) {
        fd2connection.resize(connection->fd + 1);
    }
    fd2connection[connection->fd] = connection;
}

static inline int32_t accept_new_connection(std::vector<Connection*>& fd2connection, const int fd)
{
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);
    int connectfd = accept(fd, (struct sockaddr*)&client_addr, &socklen);
    if (connectfd < 0) {
        msg("accept() error");
    }

    fd_set_nb(connectfd);

    struct Connection* connection = (struct Connection*)malloc(sizeof(struct Connection));
    if (NULL == connection) {
        close(connectfd);
    }
    connection->fd = connectfd;
    connection->state = STATE_REQ;
    connection->rbuf_size = 0;
    connection->wbuf_size = 0;
    connection->wbuf_sent = 0;
    connection_put(fd2connection, connection);
    return 0;
}

static inline void state_req(Connection* connection);
static inline void state_res(Connection* connection);

static inline bool try_one_request(Connection* connection)
{
    if (connection->rbuf_size < HEADER_SIZE) {
        return false;
    }

    uint32_t len = 0;
    memcpy(&len, &connection->rbuf[0], HEADER_SIZE);
    if (len > MAX_MESSAGE_SIZE) {
        msg("too long");
        connection->state = STATE_END;
        return false;
    }
    if (HEADER_SIZE + len > connection->rbuf_size) {
        return false;
    }

    printf("client says: %.*s\n", len, &connection->rbuf[4]);

    memcpy(&connection->wbuf[0], &len, HEADER_SIZE);
    memcpy(&connection->wbuf[HEADER_SIZE], &connection->rbuf[HEADER_SIZE], len);
    connection->wbuf_size = HEADER_SIZE + len;

    size_t remain = connection->rbuf_size - HEADER_SIZE - len;
    if (remain) {
        memmove(connection->rbuf, &connection->rbuf[HEADER_SIZE + len], remain);
    }
    connection->rbuf_size = remain;

    connection->state = STATE_RES;
    state_res(connection);
    return (connection->state == STATE_REQ);
}

static inline bool try_fill_buffer(Connection* connection)
{
    assert(connection->rbuf_size < sizeof(connection->rbuf));
    ssize_t rv = 0;
    do {
        const size_t capacity = sizeof(connection->rbuf) - connection->rbuf_size;
        rv = read(connection->fd, &connection->rbuf[connection->rbuf_size], capacity);
    } while (rv < 0 && errno == EINTR);

    if (rv < 0 && errno == EAGAIN) {
        return false;
    }
    if (rv < 0) {
        msg("read() error");
        connection->state = STATE_END;
        return false;
    }
    if (0 == rv) {
        if (connection->rbuf_size > 0) {
            msg("unexpected EOF");
        } else {
            msg("EOF");
        }
        connection->state = STATE_END;
        return false;
    }

    connection->rbuf_size += (size_t)rv;
    assert(connection->rbuf_size <= sizeof(connection->rbuf));

    while (try_one_request(connection)) {}
    return (connection->state == STATE_REQ);
}

static inline void state_req(Connection* connection)
{
    while (try_fill_buffer(connection));
}

static inline bool try_flush_buffer(Connection* connection)
{
    ssize_t rv = 0;
    do {
        const size_t remain = connection->wbuf_size - connection->wbuf_sent;
        rv = write(connection->fd, &connection->wbuf[connection->wbuf_sent], remain);
    } while (rv < 0 || errno == EINTR);
    if (rv < 0 && errno == EAGAIN) {
        return false;
    }
    if (rv < 0) {
        msg("write() error");
        connection->state = STATE_END;
        return false;
    }
    connection->wbuf_sent += (size_t)rv;
    assert(connection->wbuf_sent <= connection->wbuf_size);
    if (connection->wbuf_sent == connection->wbuf_size) {
        connection->state = STATE_REQ;
        connection->wbuf_sent = 0;
        connection->wbuf_size = 0;
        return false;
    }
    return true;
}

static inline void state_res(Connection* connection)
{
    while (try_flush_buffer(connection));
}

static inline void connection_io(Connection* connection)
{
    if (connection->state == STATE_REQ) {
        state_req(connection);
    } else if (connection->state == STATE_RES) {
        state_res(connection);
    } else {
        assert(false);
    }
}

int main()
{
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket");
    }

    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0);
    int rv = bind(fd, (const struct sockaddr*)&addr, sizeof(addr));
    if (0 != rv) {
        die("bind()");
    }

    rv = listen(fd, SOMAXCONN);
    if (0 != rv) {
        die("listen");
    }

    std::vector<Connection*> fd2connection;

    fd_set_nb(fd);

    std::vector<struct pollfd> poll_args;
    while (true) {
        poll_args.clear();
        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);
        for (size_t i = 0; i < fd2connection.size(); ++i) {
            Connection* connection = fd2connection[i];
            if (NULL == connection) continue;
            struct pollfd pfd = {};
            pfd.fd = connection->fd;
            pfd.events = (connection->state == STATE_REQ) ? POLLIN : POLLOUT;
            pfd.events |= POLLERR;
            poll_args.push_back(pfd);
        }
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), 1000);
        if (rv < 0) {
            die("poll()");
        }
        for (size_t i = 1; i < poll_args.size(); ++i) {
            if (!poll_args[i].revents) continue;
            Connection* connection = fd2connection[poll_args[i].fd];
            connection_io(connection);
            if (connection->state == STATE_END) {
                fd2connection[connection->fd] = NULL;
                close(connection->fd);
                free(connection);
            }
        }
        if (poll_args[0].revents) {
            accept_new_connection(fd2connection, fd);
        }
    }

    return 0;
}

