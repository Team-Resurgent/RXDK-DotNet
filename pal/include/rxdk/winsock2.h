/* Xbox has winsockx.h, not winsock2.h. Mono's HOST_WIN32 socket code includes this name. */
#ifndef RXDK_WINSOCK2_H
#define RXDK_WINSOCK2_H
#include <winsockx.h>
#ifndef _SOCKLEN_T_DEFINED
typedef int socklen_t;
#define _SOCKLEN_T_DEFINED
#endif
/* Constants Mono's socket icalls reference that the Xbox winsock header leaves out.
 * Numbers are the Win32 values. Unsupported options still fail at runtime. */
#ifndef AF_UNIX
#define AF_UNIX 1
#endif
#ifndef AF_IPX
#define AF_IPX 6
#endif
#ifndef AF_SNA
#define AF_SNA 11
#endif
#ifndef AF_DECnet
#define AF_DECnet 12
#endif
#ifndef AF_APPLETALK
#define AF_APPLETALK 16
#endif
#ifndef AF_INET6
#define AF_INET6 23
#endif
#ifndef PF_INET6
#define PF_INET6 AF_INET6
#endif
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif
#ifndef AF_IRDA
#define AF_IRDA 26
#endif
#ifndef SOCK_RAW
#define SOCK_RAW 3
#endif
#ifndef MSG_OOB
#define MSG_OOB 0x1
#endif
#ifndef MSG_PEEK
#define MSG_PEEK 0x2
#endif
#ifndef MSG_DONTROUTE
#define MSG_DONTROUTE 0x4
#endif
#ifndef SO_KEEPALIVE
#define SO_KEEPALIVE 0x0008
#endif
#ifndef SO_SNDLOWAT
#define SO_SNDLOWAT 0x1003
#endif
#ifndef SO_RCVLOWAT
#define SO_RCVLOWAT 0x1004
#endif
#ifndef SO_ERROR
#define SO_ERROR 0x1007
#endif
#ifndef IP_MULTICAST_IF
#define IP_MULTICAST_IF 9
#endif
#ifndef IP_MULTICAST_TTL
#define IP_MULTICAST_TTL 10
#endif
#ifndef IP_MULTICAST_LOOP
#define IP_MULTICAST_LOOP 11
#endif
#ifndef IP_ADD_MEMBERSHIP
#define IP_ADD_MEMBERSHIP 12
#endif
#ifndef IP_DROP_MEMBERSHIP
#define IP_DROP_MEMBERSHIP 13
#endif
#ifndef IP_PMTUDISC_DONT
#define IP_PMTUDISC_DONT 0
#define IP_PMTUDISC_WANT 1
#define IP_PMTUDISC_DO 2
#define IP_PMTUDISC_PROBE 3
#define IP_PMTUDISC_INTERFACE 4
#define IP_PMTUDISC_OMIT 5
#endif
#ifndef IPPROTO_IPV6
#define IPPROTO_IPV6 41
#endif
#ifndef IPV6_UNICAST_HOPS
#define IPV6_UNICAST_HOPS 4
#define IPV6_MULTICAST_IF 9
#define IPV6_MULTICAST_HOPS 10
#define IPV6_MULTICAST_LOOP 11
#define IPV6_ADD_MEMBERSHIP 12
#define IPV6_DROP_MEMBERSHIP 13
#define IPV6_JOIN_GROUP IPV6_ADD_MEMBERSHIP
#define IPV6_LEAVE_GROUP IPV6_DROP_MEMBERSHIP
#define IPV6_PKTINFO 19
#define IPV6_HOPLIMIT 21
#endif
#ifndef WSA_FLAG_OVERLAPPED
#define WSA_FLAG_OVERLAPPED 0x01
#endif
#endif
