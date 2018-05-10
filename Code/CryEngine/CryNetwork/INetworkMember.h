// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __INETWORKMEMBER_H__
#define __INETWORKMEMBER_H__

#pragma once

#include <CrySystem/TimeValue.h>
#include <CryNetwork/INetwork.h>

enum ENetworkMemberUpdateOrder
{
	eNMUO_Context,
	eNMUO_Nub,
	eNMUO_Utility
};

enum ENetDumpType
{
	eNDT_ObjectState,
	eNDT_HostMigrationStateCheck,
	eNDT_ClientConnectionState,
	eNDT_ServerConnectionState
};

class CNetChannel;

struct INetworkMember : public CMultiThreadRefCount
{
	const ENetworkMemberUpdateOrder UpdateOrder;
	INetworkMember(ENetworkMemberUpdateOrder updateOrder) : UpdateOrder(updateOrder) {}

	virtual void         GetMemoryStatistics(ICrySizer*) = 0;
	virtual bool         IsDead() = 0;
	virtual bool         IsSuicidal() = 0; // if we keep calling update, will this member say it's dead eventually?
	virtual void         SyncWithGame(ENetworkGameSync type) = 0;
	virtual void NetDump(ENetDumpType) = 0;
	virtual void         PerformRegularCleanup() = 0; // staggered compacting of memory where appropriate
	virtual void         GetProfilingStatistics(SNetworkProfilingStats* const pProfilingStats) {}
	virtual void         GetBandwidthStatistics(SBandwidthStats* const pStats)                 {}
	virtual CNetChannel* FindFirstClientChannel()                                              { return 0; }
	virtual CNetChannel* FindFirstRemoteChannel()                                              { return NULL; }

	virtual bool         IsEstablishingContext() const                                         { return false; }

	//#if LOCK_NETWORK_FREQUENCY
	virtual void TickChannels(CTimeValue& now, bool force) {}
	//#endif
};
typedef _smart_ptr<INetworkMember> INetworkMemberPtr;

#endif
