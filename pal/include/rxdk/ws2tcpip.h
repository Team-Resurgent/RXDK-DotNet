/* Minimal stand-in for the Vista ws2tcpip.h Mono includes. Numeric helpers are in xbox_net.c. */
#ifndef RXDK_WS2TCPIP_H
#define RXDK_WS2TCPIP_H
#include <winsock2.h>
#include <stddef.h>
#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif
#ifndef EAI_NONAME
#define EAI_NONAME 8
#endif
int inet_pton(int af, const char *src, void *dst);
int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                char *host, size_t hostlen, char *serv, size_t servlen, int flags);
#endif
