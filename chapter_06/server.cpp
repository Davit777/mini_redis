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
#include <string>
#include <vector>

#include "hashtable.h"

#define HEADER_SIZE           4

#define SMALL_BUFFER_SIZE     64
#define MAX_MESSAGE_SIZE      4096

#define CONTAINER_OF(ptr, type, member) ({ \
    const typeof( ((type*)0)->member )* __mptr = (ptr); \
    (type *) ( (char*)__mptr - offsetof(type, member) ); })

enum
{
    STATE_REQ = 0,
    STATE_RES = 1,
    STATE_END = 2,
};

enum
{
    RES_OK  = 0,
    RES_ERR = 1,
    RES_NX  = 2,
};

enum
{
    ERR_UNKNOWN = 1,
    ERR_2BIG    = 2
};

enum
{
    SER_NIL = 0,
    SER_ERR = 1,
    SER_STR = 2,
    SER_INT = 3,
    SER_ARR = 4,
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

static struct
{
    Hash_Map db;
} g_data;

struct Entry
{
    struct Hash_Node node;
    std::string key;
    std::string value;
};

static bool entry_eq(Hash_Node* lhs, Hash_Node* rhs)
{
    struct Entry* le = CONTAINER_OF(lhs, struct Entry, node);
    struct Entry* re = CONTAINER_OF(rhs, struct Entry, node);
    return le->key == re->key;
}

static uint64_t str_hash(const uint8_t* data, const size_t len)
{
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; ++i) {
        h = (h + data[i]) * 0x01000193;
    }
    return h;
}

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

static inline void fd_set_nb(const int fd)
{
    errno = 0;
    const int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        die("fcntl error");
    }
    errno = 0;
    (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (errno) {
        die("fcntl error");
    }
}

static inline void out_nil(std::string& out)
{
    out.push_back(SER_NIL);
}

static inline void out_str(std::string& out, const std::string& value)
{
    out.push_back(SER_STR);
    const uint32_t len = (uint32_t)value.size();
    out.append((char*)&len, HEADER_SIZE);
    out.append(value);
}

static inline void out_int(std::string& out, const int64_t value)
{
    out.push_back(SER_INT);
    out.append((char*)value, HEADER_SIZE);
}

static inline void out_err(std::string& out, const int32_t code, const std::string& msg)
{
    out.push_back(SER_ERR);
    out.append((char*)&code, HEADER_SIZE);
    const uint32_t len = (uint32_t)msg.size();
    out.append((char*)&len, HEADER_SIZE);
    out.append(msg);
}

static inline void out_arr(std::string& out, const uint32_t n)
{
    out.push_back(SER_ARR);
    out.append((char*)&n, HEADER_SIZE);
}

static inline void connection_put(std::vector<Connection*>& fd2connection, struct Connection* connection)
{
    if (fd2connection.size() <= (size_t)connection->fd) {
        fd2connection.resize(connection->fd + 1);
    }
    fd2connection[connection->fd] = connection;
}

static inline size_t accept_new_connection(std::vector<Connection*>& fd2connection, const int fd)
{
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);
    const int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
    if (connfd < 0) {
        msg("accept() error");
        return -1;
    }
    fd_set_nb(connfd);
    struct Connection* connection = (struct Connection*)malloc(sizeof(struct Connection));
    if (NULL == connection) {
        close(connfd);
        return -1;
    }
    connection->fd = connfd;
    connection->state = STATE_REQ;
    connection->rbuf_size = 0;
    connection->wbuf_size = 0;
    connection->wbuf_sent = 0;
    connection_put(fd2connection, connection);
    return 0;
}

static inline void state_req(Connection* connection);
static inline void state_res(Connection* connection);

static int32_t parse_req(const uint8_t* data, size_t len, std::vector<std::string>& out)
{
    if (len < HEADER_SIZE) {
        return -1;
    }
    uint32_t n = 0;
    memcpy(&n, &data[0], HEADER_SIZE);
    if (n > MAX_MESSAGE_SIZE) {
        return -1;
    }

    size_t pos = HEADER_SIZE;
    while (n--) {
        if (pos + HEADER_SIZE > len) {
            return -1;
        }
        uint32_t sz = 0;
        memcpy(&sz, &data[pos], HEADER_SIZE);
        if (pos + HEADER_SIZE + sz > len) {
            return -1;
        }
        out.push_back(std::string((char*)&data[pos + HEADER_SIZE], sz));
        pos += HEADER_SIZE + sz;
    }
    if (pos != len) {
        return -1;
    }
    return 0;
}

static inline void do_get(std::vector<std::string>& cmd, std::string& out)
{
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    Hash_Node* node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (NULL == node) {
        return out_nil(out);
    }

    const std::string& value = CONTAINER_OF(node, struct Entry, node)->value;
    out_str(out, value);
}

static inline void do_set(std::vector<std::string>& cmd, std::string& out)
{
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t*)key.key.data(), key.key.size());
    Hash_Node* node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (NULL != node) {
        CONTAINER_OF(node, struct Entry, node)->value.swap(cmd[2]);
    } else {
        Entry* entry = new Entry;
        entry->key.swap(key.key);
        entry->node.hcode = key.node.hcode;
        entry->value.swap(cmd[2]);
        hm_insert(&g_data.db, &entry->node);
    }
    return out_nil(out);
}

static void do_del(std::vector<std::string>& cmd, std::string& out)
{
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t*)key.key.data(), key.key.size());
    Hash_Node* node = hm_pop(&g_data.db, &key.node, &entry_eq);
    if (NULL != node) {
        delete CONTAINER_OF(node, struct Entry, node);
    }
    return out_int(out, node ? 1 : 0);
}

static inline void h_scan(Hash_Table* table, void (*f)(Hash_Node*, void*), void* arg)
{
    if (table->size == 0) {
        return;
    }
    for (size_t i = 0; i < table->mask + 1; ++i) {
        Hash_Node* node = table->table[i];
        while (node != NULL) {
            f(node, arg);
            node = node->next;
        }
    }
}

static inline void cb_scan(Hash_Node* node, void* arg)
{
    std::string& out = *(std::string *) arg;
    out_str(out, CONTAINER_OF(node, struct Entry, node)->key);
}

static inline void do_keys(std::vector<std::string>& cmd, std::string& out)
{
    (void)cmd;
    out_arr(out, (uint32_t)hm_size(&g_data.db));
    h_scan(&g_data.db.table1, &cb_scan, &out);
    h_scan(&g_data.db.table2, &cb_scan, &out);
}

static inline bool cmd_is(const std::string& word, const char* cmd)
{
    return 0 == strcasecmp(word.c_str(), cmd);
}

static void do_request(std::vector<std::string>& cmd, std::string& out)
{
    if (cmd.size() == 1 && cmd_is(cmd[0], "keys")) {
        do_keys(cmd, out);
    } else if (cmd.size() == 2 && cmd_is(cmd[0], "get")) {
        do_get(cmd, out);
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "set")) {
        do_set(cmd, out);
    } else if(cmd.size() == 2 && cmd_is(cmd[0], "del")) {
        do_del(cmd, out);
    } else {
        out_err(out, ERR_UNKNOWN, "Unknown cmd");
    }
}

static bool try_one_request(Connection* connection)
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

    std::vector<std::string> cmd;
    if (0 != parse_req(&connection->rbuf[HEADER_SIZE], len, cmd)) {
        msg("bad req");
        connection->state = STATE_END;
        return false;
    }
    
    std::string out;
    do_request(cmd, out);
    if (HEADER_SIZE + out.size() > MAX_MESSAGE_SIZE) {
        out.clear();
        out_err(out, ERR_2BIG, "response is too big");
    }

    const uint32_t wlen = (uint32_t)out.size();
    memcpy(&connection->wbuf[0], &wlen, HEADER_SIZE);
    memcpy(&connection->wbuf[HEADER_SIZE], out.data(), out.size());
    connection->wbuf_size = HEADER_SIZE + wlen;

    size_t remain = connection->rbuf_size - HEADER_SIZE - len;
    if (remain) {
        memmove(connection->rbuf, &connection->rbuf[HEADER_SIZE + len], remain);
    }
    connection->rbuf_size = remain;
    connection->state = STATE_RES;
    state_res(connection);
    return (connection->state = STATE_REQ);
}

static inline bool try_full_buffer(Connection* connection)
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
    while (try_full_buffer(connection));
}

static inline bool try_flush_buffer(Connection* connection)
{
    ssize_t rv = 0;
    do {
        const size_t remain = connection->wbuf_size - connection->wbuf_sent;
        rv = write(connection->fd, &connection->wbuf[connection->wbuf_sent], remain);
    } while (rv < 0 && errno == EINTR);
    if (rv < 0 && errno == EAGAIN) {
        return false;
    }
    if (rv < 0){
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
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    const int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0);
    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("bind()");
    }

    rv = listen(fd, SOMAXCONN);
    if (rv) {
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
            pfd.events = pfd.events | POLLERR;
            poll_args.push_back(pfd);
        }

        const int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), 1000);
        if (rv < 0) {
            die("poll");
        }

        for (size_t i = 1; i < poll_args.size(); ++i) {
            if (poll_args[i].revents) {
                Connection* connection = fd2connection[poll_args[i].fd];
                connection_io(connection);
                if (connection->state == STATE_END) {
                    fd2connection[connection->fd] = NULL;
                    (void)close(connection->fd);
                    free(connection);
                }
            }
        }

        if (poll_args[0].revents) {
            (void)accept_new_connection(fd2connection, fd);
        }
    }

    return 0;
}
