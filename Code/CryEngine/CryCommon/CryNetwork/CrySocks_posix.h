// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//////////////////////////////////////////////////////////////////////////
// NOTE: INTERNAL HEADER NOT FOR PUBLIC USE
// This header should only be include by CrySocks.h
#if !defined(INCLUDED_FROM_CRY_SOCKS_H)
	#error "CRYTEK INTERNAL HEADER. Only include from CrySocks.h."
#endif
//////////////////////////////////////////////////////////////////////////

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>

#if !CRY_PLATFORM_APPLE
	#include <arpa/nameser.h>
	#include <resolv.h>
#endif

#include <CryString/StringUtils.h>

#define SD_SEND SHUT_WR
#define SD_BOTH SHUT_RDWR

typedef sockaddr    CRYSOCKADDR;
typedef sockaddr_in CRYSOCKADDR_IN;
typedef socklen_t   CRYSOCKLEN_T;
typedef int         CRYSOCKET;
typedef in_addr     CRYINADDR;
typedef in_addr_t   CRYINADDR_T;
typedef hostent     CRYHOSTENT;

// Type wrappers for sockets
typedef fd_set  CRYFD_SET;
typedef timeval CRYTIMEVAL;

#define CRY_INVALID_SOCKET CRYSOCKET(-1)
#define CRY_SOCKET_ERROR   -1

namespace CrySock
{
///////////////////////////////////////////////////////////////////////////////
// Forward declarations
static eCrySockError TranslateLastSocketError();
static int           TranslateToSocketError(eCrySockError socketError);

static eCrySockError TranslateInvalidSocket(CRYSOCKET s);
static eCrySockError TranslateSocketError(int socketError);

///////////////////////////////////////////////////////////////////////////////

static int GetLastSocketError()
{
	return errno;
}

///////////////////////////////////////////////////////////////////////////////

static CRYSOCKET socket(int af, int type, int protocol)
{
	return ::socket(af, type, protocol);
}

///////////////////////////////////////////////////////////////////////////////

//! Create default INET socket.
static CRYSOCKET socketinet()
{
	return ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

///////////////////////////////////////////////////////////////////////////////

static int setsockopt(CRYSOCKET s, int level, int optname, const char* optval, CRYSOCKLEN_T optlen)
{
	return ::setsockopt(s, level, optname, optval, optlen);
}

///////////////////////////////////////////////////////////////////////////////

static int shutdown(CRYSOCKET s, int how)
{
	return ::shutdown(s, how);
}

///////////////////////////////////////////////////////////////////////////////

static int getsockname(CRYSOCKET s, CRYSOCKADDR* addr, CRYSOCKLEN_T* addrlen)
{
	return ::getsockname(s, addr, addrlen);
}

///////////////////////////////////////////////////////////////////////////////

static int listen(CRYSOCKET s, int backlog)
{
	return ::listen(s, backlog);
}

///////////////////////////////////////////////////////////////////////////////

static int recv(CRYSOCKET s, char* buf, int len, int flags)
{
	return ::recv(s, buf, len, flags);
}

///////////////////////////////////////////////////////////////////////////////

static int recvfrom(CRYSOCKET s, char* buf, int len, int flags, CRYSOCKADDR* addr, CRYSOCKLEN_T* addrLen)
{
	return ::recvfrom(s, buf, len, flags, addr, addrLen);
}

///////////////////////////////////////////////////////////////////////////////

static int sendto(CRYSOCKET s, char* buf, int len, int flags, CRYSOCKADDR* addr, CRYSOCKLEN_T addrLen)
{
	return ::sendto(s, buf, len, flags, addr, addrLen);
}

static int closesocket(CRYSOCKET s)
{
	return ::close(s);
}

static int connect(CRYSOCKET s, const CRYSOCKADDR* addr, CRYSOCKLEN_T addrlen)
{
	return ::connect(s, addr, addrlen);
}

static CRYSOCKET accept(CRYSOCKET s, CRYSOCKADDR* addr, CRYSOCKLEN_T* addrlen)
{
	return ::accept(s, addr, addrlen);
}

static int send(CRYSOCKET s, const char* buf, int len, int flags)
{
	return ::send(s, buf, len, flags | MSG_NOSIGNAL);
}

static bool MakeSocketNonBlocking(CRYSOCKET sock)
{
	int nFlags = ::fcntl(sock, F_GETFL);
	nFlags |= O_NONBLOCK;
	if (::fcntl(sock, F_SETFL, nFlags) == -1)
	{
		return false;
	}
	return true;
}

static bool MakeSocketBlocking(CRYSOCKET sock)
{
	int nFlags = ::fcntl(sock, F_GETFL);
	nFlags &= ~O_NONBLOCK;
	if (::fcntl(sock, F_SETFL, nFlags) == -1)
	{
		return false;
	}
	return true;
}

static CRYHOSTENT* gethostbyname(const char* hostname)
{
	return ::gethostbyname(hostname);
}

static int gethostname(char* name, int namelen)
{
	return ::gethostname(name, namelen);
}

static CRYINADDR_T GetAddrForHostOrIP(const char* hostnameOrIP, uint32 timeoutMilliseconds = 0)
{
	uint32 ret = INADDR_NONE;
	CRYHOSTENT* pHost = gethostbyname(hostnameOrIP);
	if (pHost)
	{
		ret = ((in_addr*)(pHost->h_addr))->s_addr;
	}
	else
	{
		ret = inet_addr(hostnameOrIP);
	}
	return ret != INADDR_NONE ? ret : 0;
}

///////////////////////////////////////////////////////////////////////////////

static int SetRecvTimeout(CRYSOCKET s, const int seconds, const int microseconds)
{
	struct timeval timeout;
	timeout.tv_sec = seconds;
	timeout.tv_usec = microseconds;
	return CrySock::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
}

///////////////////////////////////////////////////////////////////////////////

static int SetSendTimeout(CRYSOCKET s, const int seconds, const int microseconds)
{
	struct timeval timeout;
	timeout.tv_sec = seconds;
	timeout.tv_usec = microseconds;
	return CrySock::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
}

///////////////////////////////////c////////////////////////////////////////////

static int bind(CRYSOCKET s, const CRYSOCKADDR* addr, CRYSOCKLEN_T addrlen)
{
	return ::bind(s, addr, addrlen);
}

///////////////////////////////////////////////////////////////////////////////
//! \retval 1   Readable - use recv to get the data.
//! \retval 0   Timed out (exceeded passed timeout value).
//! \retval <0  An error occurred - see eCrySocketError.
static int IsRecvPending(CRYSOCKET s, CRYTIMEVAL* timeout)
{
	CRYFD_SET emptySet;
	FD_ZERO(&emptySet);

	CRYFD_SET readSet;
	FD_ZERO(&readSet);
	FD_SET(s, &readSet);

	int ret = ::select(s + 1, &readSet, &emptySet, &emptySet, timeout);

	if (ret >= 0)
	{
		ret = FD_ISSET(s, &readSet);
		if (ret != 0)
		{
			ret = 1;
		}
	}

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//! \retval 1   Connected.
//! \retval 0   Timed out (exceeded passed timeout value).
//! \retval <0  An error occurred - see eCrySocketError.
static int WaitForWritableSocket(CRYSOCKET s, CRYTIMEVAL* timeout)
{
	CRYFD_SET emptySet;
	FD_ZERO(&emptySet);

	CRYFD_SET writeSet;
	FD_ZERO(&writeSet);
	FD_SET(s, &writeSet);

	int ret = ::select(s + 1, &emptySet, &writeSet, &emptySet, timeout);

	if (ret >= 0)
	{
		ret = FD_ISSET(s, &writeSet);
		if (ret != 0)
		{
			ret = 1;
		}
	}

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//! IsRecvPending should be sufficient for most applications.
//! The correct value for the first parameter 'nfds' should be the highest
//! numbered socket (as passed to FD_SET) +1.
//! Only use this if you know what you're doing! Passing nfds as 0 on anything
//! other than WinSock is wrong!.
static int select(int nfds, CRYFD_SET* readfds, CRYFD_SET* writefds, CRYFD_SET* exceptfds, CRYTIMEVAL* timeout)
{
#if !defined(_RELEASE)
	if (nfds == 0)
	{
		CryFatalError("CrySock select detected first parameter is 0!  This *MUST* be fixed!");
	}
#endif

	return ::select(nfds, readfds, writefds, exceptfds, timeout);
}

///////////////////////////////////////////////////////////////////////////////
static uint32 DNSLookup(const char* hostname, uint32 timeoutMilliseconds = 200)
{
	CRYINADDR_T ret = CrySock::GetAddrForHostOrIP(hostname, timeoutMilliseconds);

	if (ret == 0 || ret == ~0)
	{
		char host[256];

		size_t hostlen = strlen(hostname);
		size_t domainlen = sizeof(LOCAL_FALLBACK_DOMAIN);
		if (hostlen + domainlen > sizeof(host) - 1)
		{
			CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_ERROR, "hostname too long to fallback to local domain: '%s'", hostname);
			return 0;
		}

		cry_strcpy(host, hostname);
		cry_strcat(host, LOCAL_FALLBACK_DOMAIN);

		host[hostlen + domainlen - 1] = 0;

		ret = CrySock::GetAddrForHostOrIP(host, timeoutMilliseconds);
	}

	return ret;
}

///////////////////////////////////////////////////////////////////////////////

static CRYINADDR_T inet_addr(const char* cp)
{
	return ::inet_addr(cp);
}

///////////////////////////////////////////////////////////////////////////////

static char* inet_ntoa(CRYINADDR in)
{
	return ::inet_ntoa(in);
}

///////////////////////////////////////////////////////////////////////////////

static eCrySockError TranslateInvalidSocket(CRYSOCKET s)
{
	if (s == CRY_INVALID_SOCKET)
	{
		return TranslateLastSocketError();
	}
	return eCSE_NO_ERROR;
}

///////////////////////////////////////////////////////////////////////////////

static eCrySockError TranslateSocketError(int socketError)
{
	if (socketError == CRY_SOCKET_ERROR)
	{
		return TranslateLastSocketError();
	}
	return eCSE_NO_ERROR;
}

///////////////////////////////////////////////////////////////////////////////

static eCrySockError TranslateLastSocketError()
{
#define TRANSLATE(_from, _to) case (_from): \
  socketError = (_to);  break;
	eCrySockError socketError = eCSE_NO_ERROR;
	int error = GetLastSocketError();
	switch (error)
	{
		TRANSLATE(0, eCSE_NO_ERROR);
		TRANSLATE(EACCES, eCSE_EACCES);
		TRANSLATE(EADDRINUSE, eCSE_EADDRINUSE);
		TRANSLATE(EADDRNOTAVAIL, eCSE_EADDRNOTAVAIL);
		TRANSLATE(EAFNOSUPPORT, eCSE_EAFNOSUPPORT);
		TRANSLATE(EALREADY, eCSE_EALREADY);
		TRANSLATE(EBADF, eCSE_EBADF);
		TRANSLATE(ECONNABORTED, eCSE_ECONNABORTED);
		TRANSLATE(ECONNREFUSED, eCSE_ECONNREFUSED);
		TRANSLATE(ECONNRESET, eCSE_ECONNRESET);
		TRANSLATE(EFAULT, eCSE_EFAULT);
		TRANSLATE(EHOSTDOWN, eCSE_EHOSTDOWN);
		TRANSLATE(EINPROGRESS, eCSE_EINPROGRESS);
		TRANSLATE(EINTR, eCSE_EINTR);
		TRANSLATE(EINVAL, eCSE_EINVAL);
		TRANSLATE(EISCONN, eCSE_EISCONN);
		TRANSLATE(EMFILE, eCSE_EMFILE);
		TRANSLATE(EMSGSIZE, eCSE_EMSGSIZE);
		TRANSLATE(ENETUNREACH, eCSE_ENETUNREACH);
		TRANSLATE(ENOBUFS, eCSE_ENOBUFS);
		TRANSLATE(ENOPROTOOPT, eCSE_ENOPROTOOPT);
		TRANSLATE(ENOTCONN, eCSE_ENOTCONN);
		TRANSLATE(EOPNOTSUPP, eCSE_EOPNOTSUPP);
		TRANSLATE(EPROTONOSUPPORT, eCSE_EPROTONOSUPPORT);
		TRANSLATE(ETIMEDOUT, eCSE_ETIMEDOUT);
		TRANSLATE(ETOOMANYREFS, eCSE_ETOOMANYREFS);
		TRANSLATE(EWOULDBLOCK, eCSE_EWOULDBLOCK);
	default:
		CRY_ASSERT_MESSAGE(false, string().Format("CrySock could not translate OS error code %x, treating as miscellaneous", error));
		socketError = eCSE_MISC_ERROR;
		break;
	}
#undef TRANSLATE

	return socketError;
}

///////////////////////////////////////////////////////////////////////////////

static int TranslateToSocketError(eCrySockError socketError)
{
	// reverse order of inputs (_to, _from) instead of (_from, _to) compared to TranslateLastSocketError
#define TRANSLATE(_to, _from) case (_from): \
  error = (_to); break;
	int error = 0;
	switch (socketError)
	{
		TRANSLATE(0, eCSE_NO_ERROR);
		TRANSLATE(EACCES, eCSE_EACCES);
		TRANSLATE(EADDRINUSE, eCSE_EADDRINUSE);
		TRANSLATE(EADDRNOTAVAIL, eCSE_EADDRNOTAVAIL);
		TRANSLATE(EAFNOSUPPORT, eCSE_EAFNOSUPPORT);
		TRANSLATE(EALREADY, eCSE_EALREADY);
		TRANSLATE(EBADF, eCSE_EBADF);
		TRANSLATE(ECONNABORTED, eCSE_ECONNABORTED);
		TRANSLATE(ECONNREFUSED, eCSE_ECONNREFUSED);
		TRANSLATE(ECONNRESET, eCSE_ECONNRESET);
		TRANSLATE(EFAULT, eCSE_EFAULT);
		TRANSLATE(EHOSTDOWN, eCSE_EHOSTDOWN);
		TRANSLATE(EINPROGRESS, eCSE_EINPROGRESS);
		TRANSLATE(EINTR, eCSE_EINTR);
		TRANSLATE(EINVAL, eCSE_EINVAL);
		TRANSLATE(EISCONN, eCSE_EISCONN);
		TRANSLATE(EMFILE, eCSE_EMFILE);
		TRANSLATE(EMSGSIZE, eCSE_EMSGSIZE);
		TRANSLATE(ENETUNREACH, eCSE_ENETUNREACH);
		TRANSLATE(ENOBUFS, eCSE_ENOBUFS);
		TRANSLATE(ENOPROTOOPT, eCSE_ENOPROTOOPT);
		TRANSLATE(ENOTCONN, eCSE_ENOTCONN);
		TRANSLATE(EOPNOTSUPP, eCSE_EOPNOTSUPP);
		TRANSLATE(EPROTONOSUPPORT, eCSE_EPROTONOSUPPORT);
		TRANSLATE(ETIMEDOUT, eCSE_ETIMEDOUT);
		TRANSLATE(ETOOMANYREFS, eCSE_ETOOMANYREFS);
		TRANSLATE(EWOULDBLOCK, eCSE_EWOULDBLOCK);
	default:
		CRY_ASSERT_MESSAGE(false, string().Format("CrySock could not translate eCrySockError error code %x, treating as miscellaneous", socketError));
		break;
	}
#undef TRANSLATE

	return error;
}

} // CrySock
