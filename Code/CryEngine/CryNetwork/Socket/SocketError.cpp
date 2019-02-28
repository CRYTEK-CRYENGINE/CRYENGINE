// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SocketError.h"
#include "Network.h"

ESocketError OSErrorToSocketError(uint32 err)
{
	ESocketError sockErr = eSE_MiscNonFatalError;
	switch (err)
	{
#define DECLERR(win, sock) case win: \
  sockErr = sock; break
#if CRY_PLATFORM_WINDOWS
		DECLERR(ERROR_OPERATION_ABORTED, eSE_Cancelled);
		DECLERR(ERROR_HOST_UNREACHABLE, eSE_UnreachableAddress);
		DECLERR(ERROR_PROTOCOL_UNREACHABLE, eSE_UnreachableAddress);
		DECLERR(ERROR_PORT_UNREACHABLE, eSE_UnreachableAddress);
		DECLERR(ERROR_CONNECTION_ABORTED, eSE_ConnectionReset);
		DECLERR(ERROR_INVALID_USER_BUFFER, eSE_FragmentationOccured);
		DECLERR(ERROR_CONNECTION_REFUSED, eSE_ConnectionReset);
		DECLERR(ERROR_NETNAME_DELETED, eSE_SocketClosed);
#endif
		DECLERR(WSAEMSGSIZE, eSE_FragmentationOccured);
		DECLERR(WSAECONNRESET, eSE_UnreachableAddress);
		DECLERR(WSAEHOSTUNREACH, eSE_UnreachableAddress);
		DECLERR(WSAECONNABORTED, eSE_ConnectionReset);
		DECLERR(WSAEALREADY, eSE_ConnectInProgress);
		DECLERR(WSAEISCONN, eSE_AlreadyConnected);
	default:
		NetWarning("socket error: %.8x %s", err, CNetwork::Get()->EnumerateError(MAKE_NRESULT(NET_FAIL, NET_FACILITY_SOCKET, err)));
		break;
#undef DECLERR
	}
	return sockErr;
}
