// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __INETWORKPRIVATE_H__
#define __INETWORKPRIVATE_H__

#pragma once

#include "Config.h"
#include <CryNetwork/INetwork.h>
#include "INetworkMember.h"
#include "objcnt.h"
#include "NetCVars.h"
#include "Socket/ISocketIOManager.h"
#include "Socket/NetResolver.h"

struct INetworkPrivate : public INetwork
{
public:
	virtual SObjectCounters&     GetObjectCounters() = 0;
	virtual ISocketIOManager&    GetExternalSocketIOManager() = 0;
	virtual ISocketIOManager&    GetInternalSocketIOManager() = 0;
	virtual CNetAddressResolver* GetResolver() = 0;
	virtual const CNetCVars&     GetCVars() = 0;
	virtual bool                 ConvertAddr(const TNetAddress& addrIn, CRYSOCKADDR_IN* pSockAddr) = 0;
	virtual bool                 ConvertAddr(const TNetAddress& addrIn, CRYSOCKADDR* pSockAddr, int* addrLen) = 0;
	virtual uint8                FrameIDToHeader(uint8 id) = 0;
	virtual void BroadcastNetDump(ENetDumpType) = 0;
};

struct INetNubPrivate : public INetNub
{
public:
	virtual void         DisconnectChannel(EDisconnectionCause cause, CrySessionHandle session, const char* reason) = 0;
	virtual INetChannel* GetChannelFromSessionHandle(CrySessionHandle session) = 0;
};

#endif // __INETWORKPRIVATE_H__
