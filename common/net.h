#ifndef CHESS_NET_H
#define CHESS_NET_H
#include <stddef.h>
#include <stdint.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
typedef WSAPOLLFD net_pollfd;
#define NET_INVALID INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
typedef int socket_t;
typedef struct pollfd net_pollfd;
#define NET_INVALID (-1)
#endif
int net_init(void);
void net_close(socket_t s);
int net_nonblock(socket_t s);
int net_would_block(void);
int net_poll(net_pollfd *fds, unsigned count, int ms);
socket_t net_listen(const char *host, unsigned short port);
socket_t net_connect(const char *host, unsigned short port, int timeout_ms);
int net_send(socket_t s, const void *data, size_t size);
int net_recv(socket_t s, void *data, size_t size);
#endif
