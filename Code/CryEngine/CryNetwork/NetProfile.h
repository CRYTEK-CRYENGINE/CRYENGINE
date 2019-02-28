// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NET_PROFILE_H__
#define __NET_PROFILE_H__

#include <CryNetwork/INetwork.h>

#if NET_MINI_PROFILE || NET_PROFILE_ENABLE || INTERNET_SIMULATOR

struct SSocketBandwidth
{
	#if NET_MINI_PROFILE || NET_PROFILE_ENABLE
	CCyclicStatsBuffer<uint64, 10> bandwidthUsedAmountTx;
	CCyclicStatsBuffer<uint64, 10> bandwidthUsedAmountRx;
	CCyclicStatsBuffer<uint32, 10> numPacketsSent;
	CCyclicStatsBuffer<uint32, 10> numPacketsRecv;

	CCyclicStatsBuffer<uint64, 10> lobbyBandwidthUsedAmountTx;
	CCyclicStatsBuffer<uint32, 10> numLobbyPacketsSent;

	CCyclicStatsBuffer<uint64, 10> seqBandwidthUsedAmountTx;
	CCyclicStatsBuffer<uint32, 10> numSeqPacketsSent;

	SBandwidthStats                bandwidthStats;
	SBandwidthStatsSubset          periodStats;

	float                          sizeDisplayRx;
	float                          avgValueRx;

	CTimeValue                     last;
	int                            numDisplayNetTicks;
	int                            numNetTicks;
	#endif

	#if INTERNET_SIMULATOR
	uint32 simPacketSends;
	uint32 simPacketDrops;
	uint32 simLastPacketLag;
	#endif
};

extern SSocketBandwidth g_socketBandwidth;

#endif // NET_MINI_PROFILE || NET_PROFILE_ENABLE || INTERNET_SIMULATOR

#if NET_PROFILE_ENABLE

class CNetProfileStackEntryList
{
public:
	CNetProfileStackEntryList(uint32 numItems);
	~CNetProfileStackEntryList();

	SNetProfileStackEntry* Pop();
	void                   Push(SNetProfileStackEntry*);

	uint32                 UnusedEntryCount()
	{
		uint32 nCount = 0;
		SNetProfileStackEntry* pItem = m_pFirst;
		while (pItem)
		{
			nCount++;
			pItem = pItem->m_next;
		}
		return nCount;
	}

private:

	SNetProfileStackEntry* m_pArray;
	SNetProfileStackEntry* m_pFirst;
};

extern SNetProfileStackEntry* g_netProfileCurrent;

void                         netProfileInitialise(bool isMultiplayer);
void                         netProfileShutDown();
void                         netProfileTick();
void                         netProfileAddBits(uint32 bits, bool read);
void                         netProfileStartProfile();
void                         netProfileRegisterCVars();
void                         netProfileCountReadBits(bool count);
bool                         netProfileGetChildFromCurrent(const char* name, SNetProfileStackEntry** entry, bool rmi);
void                         netProfileRegisterBeginCall(const char* name, SNetProfileStackEntry** entry, float budget, bool rmi);
void                         netProfileBeginFunction(SNetProfileStackEntry* entry, bool read);
void                         netProfileEndFunction();
bool                         netProfileIsInitialised();
SNetProfileStackEntry*       netProfileGetNullProfile();
void                         netProfileRenderStats(float x, float y);
void                         netProfileRegisterWithPerfHUD();
void                         netProfileRemoveUntouchedNodes(SNetProfileStackEntry*);
const SNetProfileStackEntry* netProfileGetProfileTreeRoot();
void                         netProfileFlattenTreeToLeafList(ProfileLeafList& list);

	#define NET_PROFILE_ADD_WRITE_BITS(bits)      if (netProfileIsInitialised()) { netProfileAddBits(bits, false); }
	#define NET_PROFILE_ADD_READ_BITS(bits)       if (netProfileIsInitialised()) { netProfileAddBits(bits, true); }
	#define NET_PROFILE_INITIALISE(isMultiplayer) netProfileInitialise(isMultiplayer)
	#define NET_PROFILE_SHUTDOWN()                netProfileShutDown()
	#define NET_PROFILE_TICK()                    netProfileTick()
	#define NET_PROFILE_REG_CVARS()               netProfileRegisterCVars()
	#define NET_PROFILE_GET_TREE_ROOT()           (netProfileIsInitialised() ? netProfileGetProfileTreeRoot() : NULL)
	#define NET_PROFILE_LEAF_LIST(list)           if (netProfileIsInitialised()) { netProfileFlattenTreeToLeafList(list); }

#else

	#define NET_PROFILE_ADD_WRITE_BITS(bits)
	#define NET_PROFILE_ADD_READ_BITS(bits)
	#define NET_PROFILE_INITIALISE(isMultiplayer)
	#define NET_PROFILE_TICK()
	#define NET_PROFILE_SHUTDOWN()
	#define NET_PROFILE_REG_CVARS()
	#define NET_PROFILE_GET_TREE_ROOT()
	#define NET_PROFILE_LEAF_LIST(list)

#endif
#endif // __NET_PROFILE_H__
