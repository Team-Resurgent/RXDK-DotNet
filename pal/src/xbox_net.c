/* Xbox network bring-up and the Mono address helpers the excluded networking-*.c files
 * would have provided. XNet is secure by default (no traffic to a PC); the devkit xnet
 * used by xemu allows XNET_STARTUP_BYPASS_SECURITY. DNS is XNetDnsLookup, not getaddrinfo. */
#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <mono/utils/networking.h>

extern volatile unsigned long KeTickCount;
extern void __attribute__((__stdcall__)) Sleep(unsigned long ms);
extern void __attribute__((__stdcall__)) OutputDebugStringA(const char *s);

static const char *
xnaddr_how (DWORD st)
{
	if (st & XNET_GET_XNADDR_DHCP) return "dhcp";
	if (st & XNET_GET_XNADDR_STATIC) return "static";
	if (st & XNET_GET_XNADDR_PPPOE) return "pppoe";
	if (st & XNET_GET_XNADDR_AUTO) return "auto";
	if (st & XNET_GET_XNADDR_ETHERNET) return "ethernet";
	if (st == XNET_GET_XNADDR_PENDING) return "pending";
	if (st == XNET_GET_XNADDR_NONE) return "none";
	return "other";
}

static int net_up;

void
mono_networking_init (void)
{
	XNetStartupParams xnsp;
	WSADATA wsa;
	if (net_up)
		return;
	memset (&xnsp, 0, sizeof (xnsp));
	xnsp.cfgSizeOfStruct = sizeof (xnsp);
	xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
	if (XNetStartup (&xnsp) != 0) {
		OutputDebugStringA ("RXDK-DotNet: net XNetStartup failed\n");
		return;
	}
	if (WSAStartup (MAKEWORD (2, 2), &wsa) != 0) {
		OutputDebugStringA ("RXDK-DotNet: net WSAStartup failed\n");
		XNetCleanup ();
		return;
	}
	net_up = 1;
}

void
mono_networking_shutdown (void)
{
	if (!net_up)
		return;
	WSACleanup ();
	XNetCleanup ();
	net_up = 0;
}

void *
mono_get_local_interfaces (int family, int *interface_count)
{
	(void)family;
	*interface_count = 0;
	return NULL;
}

int
mono_networking_get_tcp_protocol (void) { return IPPROTO_TCP; }
int
mono_networking_get_ip_protocol (void) { return IPPROTO_IP; }
int
mono_networking_get_ipv6_protocol (void) { return 41; }

gboolean
mono_networking_addr_to_str (MonoAddress *address, char *buffer, socklen_t buflen)
{
	unsigned long a;
	if (!address || address->family != AF_INET || buflen < 16)
		return FALSE;
	a = ntohl (address->addr.v4.s_addr);
	snprintf (buffer, (size_t)buflen, "%lu.%lu.%lu.%lu",
		(a >> 24) & 255, (a >> 16) & 255, (a >> 8) & 255, a & 255);
	return TRUE;
}

int
inet_pton (int af, const char *src, void *dst)
{
	unsigned long a;
	if (af != AF_INET || !src || !dst)
		return 0;
	a = inet_addr (src);
	if (a == INADDR_NONE && strcmp (src, "255.255.255.255") != 0)
		return 0;
	((struct in_addr *)dst)->s_addr = a;
	return 1;
}

int
getnameinfo (const struct sockaddr *sa, socklen_t salen,
             char *host, size_t hostlen, char *serv, size_t servlen, int flags)
{
	(void)sa; (void)salen; (void)host; (void)hostlen; (void)serv; (void)servlen; (void)flags;
	return EAI_NONAME;
}

static void
add_v4 (MonoAddressInfo *info, int flags, const char *canon, unsigned long addr_bits)
{
	MonoAddressEntry *cur = g_new0 (MonoAddressEntry, 1);
	MonoAddressEntry *tail = info->entries;
	cur->family = AF_INET;
	cur->socktype = SOCK_STREAM;
	cur->protocol = 0;
	cur->address_len = 4;
	cur->address.v4.s_addr = addr_bits;
	if ((flags & MONO_HINT_CANONICAL_NAME) && canon)
		cur->canonical_name = g_strdup (canon);
	if (!tail)
		info->entries = cur;
	else {
		while (tail->next)
			tail = tail->next;
		tail->next = cur;
	}
}

int
mono_get_address_info (const char *hostname, int port, int flags, MonoAddressInfo **result)
{
	MonoAddressInfo *info;
	unsigned long numeric;
	XNDNS *dns = NULL;
	unsigned long start;
	UINT i;
	(void)port;
	*result = NULL;
	if (!hostname || !net_up)
		return 1;
	info = g_new0 (MonoAddressInfo, 1);
	numeric = inet_addr (hostname);
	if (numeric != INADDR_NONE || strcmp (hostname, "255.255.255.255") == 0) {
		add_v4 (info, flags, hostname, numeric);
		*result = info;
		return 0;
	}
	if (XNetDnsLookup (hostname, NULL, &dns) != 0 || !dns) {
		mono_free_address_info (info);
		return 1;
	}
	start = KeTickCount;
	while (dns->iStatus == WSAEINPROGRESS && (KeTickCount - start) < 3000)
		Sleep (10);
	if (dns->iStatus != 0 || dns->cina == 0) {
		XNetDnsRelease (dns);
		mono_free_address_info (info);
		return 1;
	}
	for (i = 0; i < dns->cina && i < 8; i++)
		add_v4 (info, flags, hostname, dns->aina[i].s_addr);
	XNetDnsRelease (dns);
	*result = info;
	return 0;
}

/* Async socket selector is not brought up. Close still calls this. */
void
mono_threadpool_io_remove_socket (int fd)
{
	(void)fd;
}

/* w32socket.c calls these without a prototype (Xbox winsock has socket/ioctlsocket, not the
 * WSA* forms), so the calls are cdecl. WSASocket ignores the overlapped flag. */
unsigned int
WSASocket (int af, int type, int protocol, void *protoinfo, unsigned int group, unsigned int flags)
{
	(void)protoinfo; (void)group; (void)flags;
	return (unsigned int)socket (af, type, protocol);
}

int
WSAIoctl (unsigned int s, unsigned long code, void *inbuf, unsigned long inlen,
          void *outbuf, unsigned long outlen, unsigned long *bytes, void *overlapped, void *completion)
{
	(void)inlen; (void)outbuf; (void)outlen; (void)bytes; (void)overlapped; (void)completion;
	if (code == (unsigned long)FIONBIO && inbuf)
		return ioctlsocket ((SOCKET)s, (long)FIONBIO, (u_long *)inbuf);
	WSASetLastError (WSAEOPNOTSUPP);
	return SOCKET_ERROR;
}

/* Network-order IPv4 of this title, or 0 if XNet has no address yet. */
unsigned int
rxdk_title_ipv4 (void)
{
	XNADDR xn;
	unsigned long start;
	DWORD st;
	memset (&xn, 0, sizeof (xn));
	start = KeTickCount;
	do {
		st = XNetGetTitleXnAddr (&xn);
		if (st != XNET_GET_XNADDR_PENDING && st != XNET_GET_XNADDR_NONE)
			break;
		Sleep (10);
	} while ((KeTickCount - start) < 3000);
	{
		char line[80];
		unsigned long a = ntohl (xn.ina.s_addr);
		snprintf (line, sizeof (line), "RXDK-DotNet: net %lu.%lu.%lu.%lu %s 0x%lx\n",
			(a >> 24) & 255, (a >> 16) & 255, (a >> 8) & 255, a & 255,
			xnaddr_how (st), (unsigned long)st);
		OutputDebugStringA (line);
	}
	return xn.ina.s_addr;
}

int
gethostname (char *name, size_t len)
{
	const char *host = "xbox";
	int i;
	if (!name || len <= 0)
		return -1;
	for (i = 0; host[i] && i < len - 1; i++)
		name[i] = host[i];
	name[i] = 0;
	return 0;
}
