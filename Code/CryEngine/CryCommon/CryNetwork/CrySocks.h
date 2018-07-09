// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Cross platform socket behaviour.

//	Notes:
//		Use the wrapped calls here instead of raw socket calls wherever
//	possible as these calls abstract away the differences between the
//	platforms.
//
//		If you want to make a socket non-blocking, use MakeSocketNonBlocking()
//	as it is platform independent.
//
//		Use IsRecvPending() as a replacement for the use case of checking if
//	a single socket has pending data, rather than trying to use select()
//	directly as this is a common cause of subtle platform bugs.  If you need
//	to check multiple sockets simultaneously, then use select() as you
//  *ought* to know what you're doing!

#pragma once

#include <string.h>

#define LOCAL_FALLBACK_DOMAIN ".intern.crytek.de" // On connection error try to fallback to this domain

namespace CrySock
{
//	///////////////////////////////////////////////////////////////////////////////
//		//! All socket errors should be -ve since a 0 or +ve result indicates success.
enum eCrySockError
{

	eCSE_NO_ERROR         = 0, //!< No error reported.

	eCSE_EACCES           = -1,  //!< Tried to establish a connection to an invalid address (such as a broadcast address).
	eCSE_EADDRINUSE       = -2,  //!< Specified address already in use.
	eCSE_EADDRNOTAVAIL    = -3,  //!< Invalid address was specified.
	eCSE_EAFNOSUPPORT     = -4,  //!< Invalid socket type (or invalid protocol family).
	eCSE_EALREADY         = -5,  //!< Connection is being established (if the function is called again in non-blocking state).
	eCSE_EBADF            = -6,  //!< Invalid socket number specified.
	eCSE_ECONNABORTED     = -7,  //!< Connection was aborted.
	eCSE_ECONNREFUSED     = -8,  //!< Connection refused by destination.
	eCSE_ECONNRESET       = -9,  //!< Connection was reset (TCP only).
	eCSE_EFAULT           = -10, //!< Invalid socket number specified.
	eCSE_EHOSTDOWN        = -11, //!< Other end is down and unreachable.
	eCSE_EINPROGRESS      = -12, //!< Action is already in progress (when non-blocking).
	eCSE_EINTR            = -13, //!< A blocking socket call was cancelled.
	eCSE_EINVAL           = -14, //!< Invalid argument or function call.
	eCSE_EISCONN          = -15, //!< Specified connection is already established.
	eCSE_EMFILE           = -16, //!< No more socket descriptors available.
	eCSE_EMSGSIZE         = -17, //!< Message size is too large.
	eCSE_ENETUNREACH      = -18, //!< Destination is unreachable.
	eCSE_ENOBUFS          = -19, //!< Insufficient working memory.
	eCSE_ENOPROTOOPT      = -20, //!< Invalid combination of 'level' and 'optname'.
	eCSE_ENOTCONN         = -21, //!< Specified connection is not established.
	eCSE_ENOTINITIALISED  = -22, //!< Socket layer not initialised (e.g. need to call WSAStartup() on Windows).
	eCSE_EOPNOTSUPP       = -23, //!< Socket type cannot accept connections.
	eCSE_EPIPE            = -24, //!< The writing side of the socket has already been closed.
	eCSE_EPROTONOSUPPORT  = -25, //!< Invalid protocol family.
	eCSE_ETIMEDOUT        = -26, //!< TCP resend timeout occurred.
	eCSE_ETOOMANYREFS     = -27, //!< Too many multicast addresses specified.
	eCSE_EWOULDBLOCK      = -28, //!< Time out occurred when attempting to perform action.
	eCSE_EWOULDBLOCK_CONN = -29, //!< Only applicable to connect() - a non-blocking connection has been attempted and is in progress.
	                             //
	eCSE_MISC_ERROR       = -1000
};

}; //CrySock

#define INCLUDED_FROM_CRY_SOCKS_H

#if CRY_PLATFORM_WINAPI
	#include "CrySocks_win32.h"
#elif CRY_PLATFORM_ORBIS
	#include "CrySocks_sce.h"
#elif CRY_PLATFORM_POSIX
	#include "CrySocks_posix.h"
#endif

#undef INCLUDED_FROM_CRY_SOCKS_H

///////////////////////////////////////////////////////////////////////////////
