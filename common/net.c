#include "net.h"
#include <errno.h>
#include <string.h>
#ifndef _WIN32
#include <fcntl.h>
#include <signal.h>
#endif
int net_init(void) {
#ifdef _WIN32
    WSADATA data;
    return WSAStartup(MAKEWORD(2,2), &data);
#else
    signal(SIGPIPE, SIG_IGN);
    return 0;
#endif
}
void net_close(socket_t s) {
    if (s == NET_INVALID) return;
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}
int net_nonblock(socket_t s) {
#ifdef _WIN32
    u_long value = 1;
    return ioctlsocket(s, FIONBIO, &value);
#else
    int flags = fcntl(s, F_GETFL, 0);
    return flags < 0 ? -1 : fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}
int net_would_block(void) {
#ifdef _WIN32
    int e = WSAGetLastError();
    return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS || e == WSAEINTR;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == EINPROGRESS;
#endif
}
int net_poll(net_pollfd *fds, unsigned count, int ms) {
#ifdef _WIN32
    return WSAPoll(fds, count, ms);
#else
    return poll(fds, count, ms);
#endif
}
int net_send(socket_t s, const void *p, size_t n) { return (int)send(s, (const char *)p, (int)n, 0); }
int net_recv(socket_t s, void *p, size_t n) { return (int)recv(s, (char *)p, (int)n, 0); }
static int address(struct sockaddr_in *a, const char *host, unsigned short port) {
    memset(a, 0, sizeof(*a)); a->sin_family = AF_INET; a->sin_port = htons(port);
    return inet_pton(AF_INET, host, &a->sin_addr) == 1 ? 0 : -1;
}
socket_t net_listen(const char *host, unsigned short port) {
    struct sockaddr_in a;
    if (address(&a, host, port)) return NET_INVALID;
    socket_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == NET_INVALID) return s;
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) || listen(s, 32) || net_nonblock(s)) {
        net_close(s); return NET_INVALID;
    }
    return s;
}
socket_t net_connect(const char *host, unsigned short port, int timeout_ms) {
    struct sockaddr_in a;
    if (address(&a, host, port)) return NET_INVALID;
    socket_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == NET_INVALID) return s;
    if (net_nonblock(s)) { net_close(s); return NET_INVALID; }
    if (connect(s, (struct sockaddr *)&a, sizeof(a)) != 0) {
        if (!net_would_block()) { net_close(s); return NET_INVALID; }
        net_pollfd p = {0}; p.fd = s; p.events = POLLOUT;
        if (net_poll(&p, 1, timeout_ms) <= 0) { net_close(s); return NET_INVALID; }
        int error = 0;
#ifdef _WIN32
        int len = sizeof(error);
#else
        socklen_t len = sizeof(error);
#endif
        if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&error, &len) || error) {
            net_close(s); return NET_INVALID;
        }
    }
    return s;
}
