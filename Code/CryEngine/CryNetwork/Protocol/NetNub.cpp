// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  implements multiplexing and connecting/disconnecting
               channels
   -------------------------------------------------------------------------
   History:
   - 26/07/2004   : Created by Craig Tiller
*************************************************************************/
#include "StdAfx.h"
#include "Config.h"
#include "NetNub.h"
#include "NetChannel.h"
#include "Serialize.h"
#include "Network.h"
#include "INubMember.h"
#include <CryNetwork/INetwork.h>
#include <CryNetwork/INetworkService.h>
#include "Context/ClientContextView.h"
#include <CryEntitySystem/IEntitySystem.h>
#include "Network.h"
#include <CryMemory/STLGlobalAllocator.h>
#include <CryLobby/CommonICryMatchMaking.h>

static const float STATS_UPDATE_INTERVAL_NUB = 0.25f;
static const float KESTIMEOUT = 30.0f;
static const int VERSION_SIZE = 6;

void TraceUnrecognizedPacket(const char* inszTxt, const uint8* cBuf, size_t nCount, const TNetAddress& ip)
{
#ifdef _DEBUG
	OutputDebugString("\n");
	OutputDebugString(inszTxt);
	OutputDebugString("\n");

	static char sTemp[1024];
	::OutputDebugString("-------------------------------\n");
	cry_sprintf(sTemp, "INVALID PACKET FROM [%s]\n", RESOLVER.ToString(ip).c_str());
	::OutputDebugString(sTemp);
	for (size_t n = 0; n < nCount; n++)
	{
		cry_sprintf(sTemp, "%02X ", cBuf[n]);
		::OutputDebugString(sTemp);
		if (n && (n % 16) == 0)
			::OutputDebugString("\n");
	}
#endif
}

template<class Map>
typename Map::iterator LookupAddress(const TNetAddress& addr, Map& m, uint32 flags)
{
#if CRY_PLATFORM_WINDOWS
	if (flags & eCLF_HandleBrokenWindowsICMP)
	{
		// icmp messages returned via GetQueuedCompletionStatus have bogus ports
		if (const SIPv4Addr* pAddr = stl::get_if<SIPv4Addr>(&addr))
		{
			int count = 0;
			Map::iterator itOut = m.end();
			for (typename Map::iterator it = m.begin(); it != m.end(); ++it)
			{
				if (const SIPv4Addr* pKey = stl::get_if<SIPv4Addr>(&it->first))
				{
					if (pKey->addr == pAddr->addr)
					{
						if (pKey->port == pAddr->port)
							return it;
						itOut = it;
						count++;
					}
				}
			}
			if (count == 1)
				return itOut;
		}
	}
#endif
	return m.find(addr);
}

class CDefaultSecurity : public IGameSecurity
{
public:
	bool IsIPBanned(uint32 ip)                              { return false; }
	void OnPunkDetected(const char* ip, EPunkType punkType) {}
};

static CDefaultSecurity s_defaultSecurity;

class NatNegListener : public INatNegListener
{
public:
	enum EState
	{
		eS_None,
		eS_Idle,
		eS_Negotiating,
		eS_Finished
	};
private:
	struct NegProcess
	{
		NegProcess() :
			m_success(false),
			m_state(eS_Idle),
			m_cookie(0)
		{}
		int            m_cookie;
		EState         m_state;
		bool           m_success;
		CRYSOCKADDR_IN m_addr;
	};
	typedef std::vector<NegProcess> NegVector;
public:

	NatNegListener()
	{

	}

	void OnDetected(bool success, bool compatible)
	{
		//just do nothing
	}

	NegVector::iterator FindNeg(int cookie)
	{
		for (NegVector::iterator it = m_negotiations.begin(), end = m_negotiations.end(); it != end; ++it)
			if (it->m_cookie == cookie)
				return it;
		return m_negotiations.end();
	}

	NegVector::const_iterator FindNeg(int cookie) const
	{
		for (NegVector::const_iterator it = m_negotiations.begin(), end = m_negotiations.end(); it != end; ++it)
			if (it->m_cookie == cookie)
				return it;
		return m_negotiations.end();

	}

	void OnCompleted(int cookie, bool success, CRYSOCKADDR_IN* addr)
	{
		NegVector::iterator it = FindNeg(cookie);
		if (it != m_negotiations.end())
		{
			NegProcess& pr = *it;

			pr.m_success = success;
			if (addr)     //can be 0 if not succeeded
				pr.m_addr = *addr;
			pr.m_state = eS_Finished;
		}
	}

	void NewNegotiation(int cookie)
	{
		m_negotiations.push_back(NegProcess());
		m_negotiations.back().m_cookie = cookie;
	}

	void EndNegotiation(int cookie)
	{
		NegVector::iterator it = FindNeg(cookie);
		if (it != m_negotiations.end())
			m_negotiations.erase(it);
	}

	EState GetState(int cookie) const
	{
		NegVector::const_iterator it = FindNeg(cookie);
		if (it == m_negotiations.end())
			return eS_None;
		else
			return it->m_state;
	}
	void SetState(int cookie, EState state)
	{
		NegVector::iterator it = FindNeg(cookie);
		if (it != m_negotiations.end())
			it->m_state = state;
	}
	bool GetResult(int cookie) const
	{
		NegVector::const_iterator it = FindNeg(cookie);
		if (it == m_negotiations.end())
			return false;
		else
			return it->m_success;
	}
	void GetAddr(int cookie, CRYSOCKADDR_IN& a) const
	{
		NegVector::const_iterator it = FindNeg(cookie);
		if (it != m_negotiations.end())
			a = it->m_addr;
	}
private:
	NegVector m_negotiations;   //FIFO
};

class CDefaultNetDumpLogger : public INetDumpLogger
{
public:

	CDefaultNetDumpLogger() {}
	virtual ~CDefaultNetDumpLogger() {}
	virtual void Log(const char* pFmt, ...)
	{
		va_list args;
		va_start(args, pFmt);
		VNetLog(pFmt, args);
		va_end(args);
	}
};

NatNegListener g_NatNegListener;

CNetNub::CNetNub(const TNetAddress& addr, IGameNub* pGameNub,
                 IGameSecurity* pNetSecurity, IGameQuery* pGameQuery) :
	INetworkMember(eNMUO_Nub),
	m_pNetwork(NULL),
	m_pSecurity(pNetSecurity ? pNetSecurity : &s_defaultSecurity),
	m_pGameQuery(pGameQuery),
	m_pGameNub(pGameNub),
	m_bDead(false),
	m_addr(addr),
	m_cleanupChannel(0),
	m_serverReport(0),
	m_connectingLocks(0),
	m_keepAliveLocks(0)
{
#if TIMER_DEBUG
	char buffer[128];
	cry_sprintf(buffer, "CNetNub::CNetNub() m_statsTimer [%p]", this);
#else
	char* buffer = NULL;
#endif // TIMER_DEBUG

	m_pLobby = gEnv->pLobby;
	m_statsTimer = TIMER.ADDTIMER(g_time, TimerCallback, this, buffer);
	++g_objcnt.nub;
}

#if ENABLE_DEBUG_PACKET_DATA_SIZE
struct SMessageIDCount
{
	EMessageIDTable table;
	uint32          id;
};

int SMessageIDCountCmp(const void* p1, const void* p2)
{
	uint32 c1 = g_debugPacketDataSize.pMessageIDData[((const SMessageIDCount*)p1)->id].data[((const SMessageIDCount*)p1)->table].count;
	uint32 c2 = g_debugPacketDataSize.pMessageIDData[((const SMessageIDCount*)p2)->id].data[((const SMessageIDCount*)p1)->table].count;

	if (c1 == c2)
	{
		return 0;
	}
	else
	{
		if (c1 > c2)
		{
			return -1;
		}
		else
		{
			return 1;
		}
	}
}
#endif

CNetNub::~CNetNub()
{
	SCOPED_GLOBAL_LOCK;
	--g_objcnt.nub;
	TIMER.CancelTimer(m_statsTimer);
	if (m_pSocketMain)
	{
		m_pSocketMain->UnregisterListener(this);
		ISocketIOManager* pSocketIOManager = &CNetwork::Get()->GetInternalSocketIOManager();
		if (m_pLobby && m_pLobby->GetLobbyServiceType() == eCLS_Online)
		{
			pSocketIOManager = &CNetwork::Get()->GetExternalSocketIOManager();
		}
		pSocketIOManager->FreeDatagramSocket(m_pSocketMain);
		m_pSocketMain = NULL;
	}

#if ENABLE_DEBUG_PACKET_DATA_SIZE
	if (g_debugPacketDataSize.totalDataSize > 0)
	{
		uint32 unaccountedData = g_debugPacketDataSize.totalDataSize -
		                         (g_debugPacketDataSize.idSize + g_debugPacketDataSize.seqBytesSize + g_debugPacketDataSize.signingKeySize + g_debugPacketDataSize.dataSize[eDPDST_Ack] +
		                          g_debugPacketDataSize.dataSize[eDPDST_MessageID] + g_debugPacketDataSize.dataSize[eDPDST_NetID] + g_debugPacketDataSize.dataSize[eDPDST_MessagesHeader] + g_debugPacketDataSize.dataSize[eDPDST_MessageBody] + g_debugPacketDataSize.dataSize[eDPDST_MessagesFooter] +
		                          g_debugPacketDataSize.dataSize[eDPDST_HadUrgentData] + g_debugPacketDataSize.dataSize[eDPDST_Padding]);

		uint32 origTotalDataSize = g_debugPacketDataSize.totalDataSize + (g_debugPacketDataSize.origMessageIDSize - g_debugPacketDataSize.dataSize[eDPDST_MessageID]) + (g_debugPacketDataSize.origNetIDSize - g_debugPacketDataSize.dataSize[eDPDST_NetID]);

		NetLog("OrigTotalData %d\nPacketID %d (%.02f%%) SeqByte %d (%.02f%%) SigningKey %d (%.02f%%) Ack %d (%.02f%%) MessageID %d (%.02f%%) NetID %d (%.02f%%) MessagesHeader %d (%.02f%%) MessageBody %d (%.02f%%) MessagesFooter %d (%.02f%%) HadUrgentData %d (%.02f%%) Padding %d (%.02f%%)",
		       origTotalDataSize,
		       g_debugPacketDataSize.idSize, (100.0f * g_debugPacketDataSize.idSize) / origTotalDataSize,
		       g_debugPacketDataSize.seqBytesSize, (100.0f * g_debugPacketDataSize.seqBytesSize) / origTotalDataSize,
		       g_debugPacketDataSize.signingKeySize, (100.0f * g_debugPacketDataSize.signingKeySize) / origTotalDataSize,
		       g_debugPacketDataSize.dataSize[eDPDST_Ack], (100.0f * g_debugPacketDataSize.dataSize[eDPDST_Ack]) / origTotalDataSize,
		       g_debugPacketDataSize.origMessageIDSize, (100.0f * g_debugPacketDataSize.origMessageIDSize) / origTotalDataSize,
		       g_debugPacketDataSize.origNetIDSize, (100.0f * g_debugPacketDataSize.origNetIDSize) / origTotalDataSize,
		       g_debugPacketDataSize.dataSize[eDPDST_MessagesHeader], (100.0f * g_debugPacketDataSize.dataSize[eDPDST_MessagesHeader]) / origTotalDataSize,
		       g_debugPacketDataSize.dataSize[eDPDST_MessageBody], (100.0f * g_debugPacketDataSize.dataSize[eDPDST_MessageBody]) / origTotalDataSize,
		       g_debugPacketDataSize.dataSize[eDPDST_MessagesFooter], (100.0f * g_debugPacketDataSize.dataSize[eDPDST_MessagesFooter]) / origTotalDataSize,
		       g_debugPacketDataSize.dataSize[eDPDST_HadUrgentData], (100.0f * g_debugPacketDataSize.dataSize[eDPDST_HadUrgentData]) / origTotalDataSize,
		       g_debugPacketDataSize.dataSize[eDPDST_Padding], (100.0f * g_debugPacketDataSize.dataSize[eDPDST_Padding]) / origTotalDataSize
		       );

		NetLog("TotalData %d Percentage of original size (%.02f%%)\nPacketID %d (%.02f%%) SeqByte %d (%.02f%%) SigningKey %d (%.02f%%) Ack %d (%.02f%%) MessageID %d (%.02f%%) NetID %d (%.02f%%) MessagesHeader %d (%.02f%%) MessageBody %d (%.02f%%) MessagesFooter %d (%.02f%%) HadUrgentData %d (%.02f%%) Padding %d (%.02f%%) unaccounted %d (%.02f%%)",
		       g_debugPacketDataSize.totalDataSize, (100.0f * g_debugPacketDataSize.totalDataSize) / origTotalDataSize,
		       g_debugPacketDataSize.idSize, (100.0f * g_debugPacketDataSize.idSize) / g_debugPacketDataSize.totalDataSize,
		       g_debugPacketDataSize.seqBytesSize, (100.0f * g_debugPacketDataSize.seqBytesSize) / g_debugPacketDataSize.totalDataSize,
		       g_debugPacketDataSize.signingKeySize, (100.0f * g_debugPacketDataSize.signingKeySize) / g_debugPacketDataSize.totalDataSize,
		       g_debugPacketDataSize.dataSize[eDPDST_Ack], (100.0f * g_debugPacketDataSize.dataSize[eDPDST_Ack]) / g_debugPacketDataSize.totalDataSize,
		       g_debugPacketDataSize.dataSize[eDPDST_MessageID], (100.0f * g_debugPacketDataSize.dataSize[eDPDST_MessageID]) / g_debugPacketDataSize.totalDataSize,
		       g_debugPacketDataSize.dataSize[eDPDST_NetID], (100.0f * g_debugPacketDataSize.dataSize[eDPDST_NetID]) / g_debugPacketDataSize.totalDataSize,
		       g_debugPacketDataSize.dataSize[eDPDST_MessagesHeader], (100.0f * g_debugPacketDataSize.dataSize[eDPDST_MessagesHeader]) / g_debugPacketDataSize.totalDataSize,
		       g_debugPacketDataSize.dataSize[eDPDST_MessageBody], (100.0f * g_debugPacketDataSize.dataSize[eDPDST_MessageBody]) / g_debugPacketDataSize.totalDataSize,
		       g_debugPacketDataSize.dataSize[eDPDST_MessagesFooter], (100.0f * g_debugPacketDataSize.dataSize[eDPDST_MessagesFooter]) / g_debugPacketDataSize.totalDataSize,
		       g_debugPacketDataSize.dataSize[eDPDST_HadUrgentData], (100.0f * g_debugPacketDataSize.dataSize[eDPDST_HadUrgentData]) / g_debugPacketDataSize.totalDataSize,
		       g_debugPacketDataSize.dataSize[eDPDST_Padding], (100.0f * g_debugPacketDataSize.dataSize[eDPDST_Padding]) / g_debugPacketDataSize.totalDataSize,
		       unaccountedData, (100.0f * unaccountedData) / g_debugPacketDataSize.totalDataSize
		       );
	}

	if (g_debugPacketDataSize.totalNumNetIDs > 0)
	{
		NetLog("Total number NetIDs sent %d Low %d (%.02f%%) Medium %d (%.02f%%) (Invalid %d (%.02f%%)) High %d (%.02f%%) Average bits per NetID %.02f",
		       g_debugPacketDataSize.totalNumNetIDs,
		       g_debugPacketDataSize.numLowBitNetIDs, (100.0f * g_debugPacketDataSize.numLowBitNetIDs) / g_debugPacketDataSize.totalNumNetIDs,
		       g_debugPacketDataSize.numMediumBitNetIDs, (100.0f * g_debugPacketDataSize.numMediumBitNetIDs) / g_debugPacketDataSize.totalNumNetIDs,
		       g_debugPacketDataSize.numInvalidNetIDs, (100.0f * g_debugPacketDataSize.numInvalidNetIDs) / g_debugPacketDataSize.totalNumNetIDs,
		       g_debugPacketDataSize.numHighBitNetIDs, (100.0f * g_debugPacketDataSize.numHighBitNetIDs) / g_debugPacketDataSize.totalNumNetIDs,
		       ((float)g_debugPacketDataSize.dataSize[eDPDST_NetID]) / g_debugPacketDataSize.totalNumNetIDs);
	}

	if ((g_debugPacketDataSize.numMessageIDs > 0) && (g_debugPacketDataSize.totalNumMessages[eMIDT_Global]))
	{
		SMessageIDCount* pMessageIDCounts = new SMessageIDCount[g_debugPacketDataSize.numMessageIDs];

		NetLog("Total number messages sent %d Average bits per MessageID %.02f", g_debugPacketDataSize.totalNumMessages[eMIDT_Global], ((float)g_debugPacketDataSize.dataSize[eDPDST_MessageID]) / g_debugPacketDataSize.totalNumMessages[eMIDT_Global]);

		for (uint32 t = 0; t < eMIDT_NumTables; t++)
		{
			if (g_debugPacketDataSize.totalNumMessages[t] > 0)
			{
				for (uint32 i = 0; i < g_debugPacketDataSize.numMessageIDs; i++)
				{
					pMessageIDCounts[i].id = i;
					pMessageIDCounts[i].table = (EMessageIDTable)t;
				}

				qsort(pMessageIDCounts, g_debugPacketDataSize.numMessageIDs, sizeof(SMessageIDCount), SMessageIDCountCmp);

				switch (t)
				{
				case eMIDT_Global:
					NetLog("Overall MessageID Data");
					break;

				case eMIDT_Normal:
					NetLog("Normal MessageID Data");
					break;

				case eMIDT_UpdateObject:
					NetLog("Update Object MessageID Data");
					break;
				}

				NetLog("Total number messages sent %d (%.02f%%)", g_debugPacketDataSize.totalNumMessages[t], (100.0f * g_debugPacketDataSize.totalNumMessages[t]) / g_debugPacketDataSize.totalNumMessages[eMIDT_Global]);

				for (uint32 i = 0; i < g_debugPacketDataSize.numMessageIDs; i++)
				{
					SDebugPacketDataSize::SMessageIDData* pID = &g_debugPacketDataSize.pMessageIDData[pMessageIDCounts[i].id];
					SDebugPacketDataSize::SMessageIDData::SMessageIDTableData* pIDTable = &pID->data[t];

					NetLog("%d Message ID %d num %d (%.02f%%) size %d (%.02f) messageID size %d (%.02f) num NetIDs %d (%.02f) netID size %d (%.02f) payload size %d (%.02f) %s",
					       i,
					       pMessageIDCounts[i].id,
					       pIDTable->count,
					       (100.0f * pIDTable->count) / g_debugPacketDataSize.totalNumMessages[t],
					       pIDTable->size, pIDTable->count ? (float)pIDTable->size / pIDTable->count : 0.0f,
					       pIDTable->messageIDSize, pIDTable->count ? (float)pIDTable->messageIDSize / pIDTable->count : 0.0f,
					       pIDTable->numNetIDs, pIDTable->count ? (float)pIDTable->numNetIDs / pIDTable->count : 0.0f,
					       pIDTable->netIDSize, pIDTable->count ? (float)pIDTable->netIDSize / pIDTable->count : 0.0f,
					       pIDTable->size - pIDTable->messageIDSize - pIDTable->netIDSize, pIDTable->count ? (float)(pIDTable->size - pIDTable->messageIDSize - pIDTable->netIDSize) / pIDTable->count : 0.0f,
					       pID->name.c_str());
				}
			}
		}

		delete[] pMessageIDCounts;
	}
#endif
}

void CNetNub::DeleteNub()
{
	SCOPED_GLOBAL_LOCK;
	for (TChannelMap::iterator i = m_channels.begin(); i != m_channels.end(); ++i)
	{
		DisconnectChannel(eDC_NubDestroyed, &i->first, NULL, "Nub destroyed");
	}

	m_bDead = true;

	if (m_pSocketMain)
	{
		m_pSocketMain->UnregisterListener(this);
	}

	TIMER.CancelTimer(m_statsTimer);
}

bool CNetNub::ConnectTo(const char* address, const char* connectionString)
{
	SCOPED_GLOBAL_LOCK;
	CNubConnectingLock conlk(this);
	NetLogAlways("connection requested to: %s", address);
	ICryLobby* pLobby = gEnv->pLobby;
	const char* nat_prefix = "<nat>";
	size_t nat_prefixLen = strlen(nat_prefix);
	const char* sessionPrefix = "<session>";
	CrySessionHandle session = CrySessionInvalidHandle;
	string addressStr = address;
	string currentStr;

	while (true)
	{
		size_t pos = addressStr.find(',');

		if (pos != string::npos)
		{
			// Found a comma
			currentStr = addressStr.substr(0, pos);
			addressStr = addressStr.substr(pos + 1);

			size_t sessionPrefixLen = strlen(sessionPrefix);
			if (strnicmp(currentStr, sessionPrefix, sessionPrefixLen) == 0)
			{
				// Found a session handle
				const string sessionString = currentStr.substr(sessionPrefixLen);
				session = strtoul(sessionString.c_str(), NULL, 16);
			}
		}
		else
		{
			size_t sessionPrefixLen = strlen(sessionPrefix);
			if (strnicmp(addressStr, sessionPrefix, sessionPrefixLen) == 0)
			{
				// Found a session handle
				session = strtoul(addressStr.substr(sessionPrefixLen).c_str(), NULL, 16);
			}
			break;
		}
	}

	ICryMatchMakingPrivate* pMMPrivate = pLobby ? pLobby->GetMatchMakingPrivate() : nullptr;
	if (session != CrySessionInvalidHandle && pMMPrivate)
	{
		TNetAddressVec tmp;
		tmp.push_back(pMMPrivate->GetHostAddressFromSessionHandle(session));
		DoConnectTo(tmp, session, connectionString, conlk);
		return true;
	}

	CNameRequestPtr pReq = RESOLVER.RequestNameLookup(addressStr);

	string sConStr = connectionString;
	GN_Lazy_ContinueConnectTo(pReq, session, sConStr, conlk);

	return true;
}

void CNetNub::GN_Lazy_ContinueConnectTo(CNameRequestPtr pReq, CrySessionHandle session, string connectionString, CNubConnectingLock conlk)
{
	SCOPED_GLOBAL_LOCK;

	if (!pReq->TimedWait(0.01f))
	{
		FROM_GAME(&CNetNub::GN_Loop_ContinueConnectTo, this, pReq, session, connectionString, conlk);
		return;
	}

	//m_connecting = false;//initial step is over

	TNetAddressVec ips;
	if (pReq->GetResult(ips) != eNRR_Succeeded)
	{
		m_pGameNub->FailedActiveConnect(eDC_ResolveFailed, "Failed to lookup address");
		return;
	}

	if (!ips.empty())
		for (uint32 i = 0; i < ips.size(); i++)
			NetLogAlways("resolved as: %s", RESOLVER.ToNumericString(ips[i]).c_str());

	//TNetAddress& ip = ips[0];
	DoConnectTo(ips, session, connectionString, conlk);
}

void CNetNub::GN_Loop_ContinueConnectTo(CNameRequestPtr pReq, CrySessionHandle session, string connectionString, CNubConnectingLock conlk)
{
	TO_GAME_LAZY(&CNetNub::GN_Lazy_ContinueConnectTo, this, pReq, session, connectionString, conlk);
}

bool CNetNub::HasPendingConnections()
{
	for (TChannelMap::iterator i = m_channels.begin(); i != m_channels.end(); ++i)
	{
		CNetChannel* pCh = i->second->GetNetChannel();

		if (pCh->GetContextViewState() <= eCVS_Begin)
		{
			return true;
		}
	}

	return false;
}

#if NEW_BANDWIDTH_MANAGEMENT
void CNetNub::LogChannelPerformanceMetrics(uint32 id) const
{
	bool foundChannel = false;

	NetLogAlways("Channel Performance Metrics");
	NetLogAlways("    ID : Bandwidth :    Packet Rate    : Name");
	NetLogAlways("       :  shares   : Desired :  Idle   :");
	for (TChannelMap::const_iterator it = m_channels.begin(); it != m_channels.end(); ++it)
	{
		CNetChannel* pNetChannel = it->second->GetNetChannel();
		if (!(pNetChannel->IsLocal()) && ((id == 0) || (id == pNetChannel->GetLocalChannelID())))
		{
			foundChannel = true;
			const INetChannel::SPerformanceMetrics* pMetrics = pNetChannel->GetPerformanceMetrics();
			NetLogAlways(" %5d :   %6d  :   %3d   :  %3d   : %s", pNetChannel->GetLocalChannelID(), pMetrics->m_bandwidthShares, pMetrics->m_packetRate, pMetrics->m_packetRateIdle, pNetChannel->GetName());
		}
	}

	if (!foundChannel)
	{
		NetLogAlways("Unable to get performance metrics for channel - not a remote channel");
	}
}

void CNetNub::SetChannelPerformanceMetrics(uint32 id, INetChannel::SPerformanceMetrics& metrics)
{
	bool foundChannel = false;

	for (TChannelMap::iterator it = m_channels.begin(); it != m_channels.end(); ++it)
	{
		CNetChannel* pNetChannel = it->second->GetNetChannel();
		if (!(pNetChannel->IsLocal()) && (id == pNetChannel->GetLocalChannelID()))
		{
			foundChannel = true;
			pNetChannel->SetPerformanceMetrics(&metrics);
			pNetChannel->SendSetRemotePerformanceMetricsWith(SSerialisePerformanceMetrics(metrics), pNetChannel);
			LogChannelPerformanceMetrics(pNetChannel->GetLocalChannelID());
			break;
		}
	}

	if (!foundChannel)
	{
		NetLogAlways("Unable to set performance metrics for channel - not a remote channel");
	}
}
#endif // NEW_BANDWIDTH_MANAGEMENT

void CNetNub::GetBandwidthStatistics(SBandwidthStats* const pStats)
{
	for (TChannelMap::iterator it = m_channels.begin(); it != m_channels.end(); ++it)
	{
		CNetChannel* pNetChannel = it->second->GetNetChannel();
		pNetChannel->GetBandwidthStatistics(pStats->m_numChannels++, pStats);
	}
}

void CNetNub::DoConnectTo(const TNetAddressVec& ips, CrySessionHandle session, string connectionString, CNubConnectingLock conlk)
{
	// cannot connect to an already connected nub
	bool isAlreadyConnected = false;
	{
		CryAutoLock<CryCriticalSection> lock(m_lockChannels);
		for (TNetAddressVec::const_iterator it = ips.begin(); it != ips.end() && !isAlreadyConnected; ++it)
		{
			if (m_channels.find(*it) != m_channels.end())
				isAlreadyConnected = true;
		}
	}
	if (isAlreadyConnected)
		return;

	SPendingConnection pc;
	pc.connectionString = connectionString;
	pc.to = ips[0]; // initially use the first entry
	pc.tos = ips;
	pc.kes = eKES_SetupInitiated;
	pc.kesStart = g_time;
	pc.conlk = conlk;
	pc.session = session;

	if (SendPendingConnect(pc))
		m_pendingConnections.push_back(pc);

	//if (pc.pChannel != NULL)
	//{
	//	if (SendPendingConnect(pc))
	//	m_pendingConnections.push_back(pc);
	//}
}

bool CNetNub::IsConnecting()
{
	return m_connectingLocks > 0;
}

void CNetNub::DisconnectPlayer(EDisconnectionCause cause, EntityId plr_id, const char* reason)
{
	for (TChannelMap::iterator i = m_channels.begin(); i != m_channels.end(); ++i)
	{
		CNetChannel* pCh = i->second->GetNetChannel();
		if (pCh->GetContextView() && pCh->GetContextView()->HasWitness(plr_id))
		{
			SCOPED_GLOBAL_LOCK;
			DisconnectChannel(cause, 0, pCh, reason);
			break;
		}
	}
}

void CNetNub::TimerCallback(NetTimerId, void* p, CTimeValue)
{
	CNetNub* pThis = static_cast<CNetNub*>(p);

	SStatistics stats;
	for (TChannelMap::iterator i = pThis->m_channels.begin(); i != pThis->m_channels.end(); ++i)
	{
		if (CNetChannel* pNetChannel = i->second->GetNetChannel())
		{
			INetChannel::SStatistics chanStats = pNetChannel->GetStatistics();
			stats.bandwidthUp += chanStats.bandwidthUp;
			stats.bandwidthDown += chanStats.bandwidthDown;
		}
	}
	pThis->m_statistics = stats;
	pThis->m_ackDisconnectSet.clear();

	if (!pThis->IsDead())
	{
#if TIMER_DEBUG
		char buffer[128];
		cry_sprintf(buffer, "CNetNub::TimerCallback() m_statsTimer [%p]", pThis);
#else
		char* buffer = NULL;
#endif // TIMER_DEBUG

		pThis->m_statsTimer = TIMER.ADDTIMER(g_time + STATS_UPDATE_INTERVAL_NUB, TimerCallback, pThis, buffer);
	}
}

bool CNetNub::SendPendingConnect(SPendingConnection& pc)
{
	// NOTE: a channel will be created after private key is established
	// so we might not have a valid channel here
	if (pc.pChannel && pc.pChannel->IsDead())
	{
		AddDisconnectEntry(pc.to, pc.session, eDC_CantConnect, "Dead channel");
		return false;
	}
	if (pc.pChannel && pc.pChannel->IsConnectionEstablished())
		return false;
	if (pc.lastSend + 0.25f > g_time)
		return true;

	if (pc.kes == eKES_SetupInitiated)
	{
		if (g_time - pc.kesStart >= KESTIMEOUT)
		{
			NetWarning("Connection to %s timed out", RESOLVER.ToString(pc.to).c_str());
			TO_GAME(&CNetNub::GC_FailedActiveConnect, this, eDC_Timeout, string().Format("Connection attempt to %s timed out", RESOLVER.ToString(pc.to).c_str()), CNubKeepAliveLock(this));
			ICryMatchMakingPrivate* pMMPrivate = m_pLobby ? m_pLobby->GetMatchMakingPrivate() : nullptr;
			if (pMMPrivate)
			{
				CryLog("CNetNub::SendPendingConnect calling SessionDisconnectRemoteConnection");
				pMMPrivate->SessionDisconnectRemoteConnectionFromNub(pc.session, eDC_Timeout);
			}
			return false; // staying in this state too long
		}

		SFileVersion ver = gEnv->pSystem->GetProductVersion();
		uint32 v[VERSION_SIZE];
		for (int i = 0; i < 4; i++)
			v[i] = htonl(ver.v[i]);
		v[4] = htonl(PROTOCOL_VERSION);
		v[5] = htonl(CNetwork::Get()->GetExternalSocketIOManager().caps);

		uint32 profile = 0;
		uint32 tokenId = 0;

		if (ICVar* pP = gEnv->pConsole->GetCVar("cl_profile"))
		{
			pc.profile = pP->GetIVal();
			profile = htonl(pc.profile);
		}
		tokenId = htonl(CNetCVars::Get().TokenId);

		size_t cslen = pc.connectionString.length();

		// cppcheck-suppress allocaCalled
		PREFAST_SUPPRESS_WARNING(6255) uint8 * pBuffer = (uint8*) alloca(1 + (VERSION_SIZE)*sizeof(uint32) + 2 * sizeof(uint32) + sizeof(CrySessionHandle) + cslen);

		uint32 cur_size = 0;
		pBuffer[cur_size] = Frame_IDToHeader[eH_ConnectionSetup];
		cur_size++;
		memcpy(pBuffer + cur_size, v, VERSION_SIZE * sizeof(uint32));
		cur_size += VERSION_SIZE * sizeof(uint32);
		memcpy(pBuffer + cur_size, &profile, sizeof(uint32));
		cur_size += sizeof(uint32);
		memcpy(pBuffer + cur_size, &tokenId, sizeof(uint32));
		cur_size += sizeof(uint32);
		CrySessionHandle session = htonl(pc.session);
		memcpy(pBuffer + cur_size, &session, sizeof(CrySessionHandle));
		cur_size += sizeof(CrySessionHandle);
		memcpy(pBuffer + cur_size, pc.connectionString.data(), cslen);
		cur_size += cslen;

		bool bSuccess = SendTo(pBuffer, cur_size, pc.to);

		pc.lastSend = g_time;
		pc.connectCounter++;
		if ((pc.connectCounter % 10) == 0)
			NetWarning("Retrying connection to %s (%d)", RESOLVER.ToString(pc.to).c_str(), pc.connectCounter);

		if (!bSuccess)
			AddDisconnectEntry(pc.to, pc.session, eDC_CantConnect, "Failed sending connect packet");

		return bSuccess;
	}

	if (pc.kes == eKES_KeyEstablished)
	{
		// if we are in this state, a channel must have been created
		// this pending connection will be removed when data starts flowing on the channel or channel inactivity timeout

		// we do it the simple way: keep sending KeyExchange1 until we are removed by channel connection establishment
		// good thing about it is that we don't need to do the same on the passive connection side, the passive connection side
		// just need to ignore this packet when it's already in eKES_KeyEstablished state, until NetChannel is actively sending data
		pc.lastSend = g_time;
		if ((pc.connectCounter % 10) == 0)
			NetWarning("Resending KeyExchange1 to %s (%d)", RESOLVER.ToString(pc.to).c_str(), pc.connectCounter);
		pc.connectCounter++;
		return SendKeyExchange1(pc, true);
	}

	AddDisconnectEntry(pc.to, pc.session, eDC_ProtocolError, "Key establishment handshake ordering error trying to send pending connect");

	// should never come here
	NET_ASSERT(0);
	return false;
}

void CNetNub::PerformRegularCleanup()
{
	// cleanup inactive entries in connecting map
	TConnectingMap connectingMap;
	for (TConnectingMap::const_iterator iter = m_connectingMap.begin(); iter != m_connectingMap.end(); ++iter)
	{
		if (iter->second.kes == eKES_SentKeyExchange && g_time - iter->second.kesStart >= KESTIMEOUT)
			NetWarning("Removing inactive pre-mature connection from %s", RESOLVER.ToString(iter->first).c_str());
		else
			connectingMap.insert(*iter); // save entries that are still active
	}
	connectingMap.swap(m_connectingMap);

	// zeroth pass: pending connections
	TPendingConnectionSet pendingConnections = m_pendingConnections;
	m_pendingConnections.resize(0);
	for (TPendingConnectionSet::iterator iter = pendingConnections.begin(); iter != pendingConnections.end(); ++iter)
	{
		if (SendPendingConnect(*iter))
			m_pendingConnections.push_back(*iter);
	}

	// first pass: notify channels
	if (!m_channels.empty())
	{
		size_t n = m_cleanupChannel % m_channels.size();
		TChannelMap::iterator iter = m_channels.begin();
		while (n > 0)
		{
			++iter;
			n--;
		}
		if (!iter->second->IsDead())
			iter->second->PerformRegularCleanup();

		m_cleanupChannel++;
	}

	// second pass: clear out very old disconnects
	for (TDisconnectMap::iterator iter = m_disconnectMap.begin(); iter != m_disconnectMap.end(); )
	{
		TDisconnectMap::iterator next = iter;
		++next;

		if (iter->second.when + 60.0f < g_time)
			m_disconnectMap.erase(iter->first);

		iter = next;
	}
}

const INetNub::SStatistics& CNetNub::GetStatistics()
{
	//SCOPED_GLOBAL_LOCK;
	// TODO: define statistics
	// TODO: update statistics here
	return m_statistics;
}

bool CNetNub::Init(CNetwork* pNetwork)
{
	TNetAddress ipLocal;

	uint32 flags = 0;
	if (m_pGameQuery)
		flags |= eSF_BroadcastReceive;
	flags |= eSF_BigBuffer;

	ISocketIOManager* pSocketIOManager = &CNetwork::Get()->GetInternalSocketIOManager();
	ICryMatchMakingPrivate* pMMPrivate = m_pLobby ? m_pLobby->GetMatchMakingPrivate() : nullptr;
	if (pMMPrivate && pMMPrivate->GetLobbyServiceTypeForNubSession() == eCLS_Online)
	{
		flags |= eSF_Online;
		pSocketIOManager = &CNetwork::Get()->GetExternalSocketIOManager();
	}

	m_pSocketMain = pSocketIOManager->CreateDatagramSocket(m_addr, flags);
	if (!m_pSocketMain)
		return false;
	m_pSocketMain->RegisterListener(this);

	m_pNetwork = pNetwork;

	return true;
}

#if ENABLE_BUFFER_VERIFICATION
class CVerifyBuffer
{
public:
	CVerifyBuffer(const uint8* pBuffer, unsigned nLength)
	{
		m_nLength = nLength;
		m_vBuffer = new uint8[nLength];
		m_vOrigBuffer = pBuffer;
		memcpy(m_vBuffer, pBuffer, nLength);
	}
	~CVerifyBuffer()
	{
		NET_ASSERT(0 == memcmp(m_vBuffer, m_vOrigBuffer, m_nLength));
		delete[] m_vBuffer;
	}

private:
	const uint8* m_vOrigBuffer;
	uint8*       m_vBuffer;
	unsigned     m_nLength;
};
#endif

void CNetNub::SyncWithGame(ENetworkGameSync type)
{
	// first pass: garbage collection - only once per frame
	if (type == eNGS_FrameStart)
	{
		static std::vector<TNetAddress, stl::STLGlobalAllocator<TNetAddress>> svDeadChannels;
		svDeadChannels.reserve(32);
		for (TChannelMap::iterator i = m_channels.begin(); i != m_channels.end(); ++i)
		{
			if (i->second->IsDead())
				svDeadChannels.push_back(i->first);
		}
		while (!svDeadChannels.empty())
		{
			CryAutoLock<CryCriticalSection> lock(m_lockChannels);
			m_channels.find(svDeadChannels.back())->second->Die(eDC_Unknown, "Disconnected");
			m_channels.find(svDeadChannels.back())->second->RemovedFromNub();
			m_channels.erase(svDeadChannels.back());
			svDeadChannels.pop_back();
		}
	}

	// second pass: queued messages
	for (TChannelMap::iterator i = m_channels.begin(); i != m_channels.end(); ++i)
	{
		i->second->SyncWithGame(type);
	}
}

bool CNetNub::SendTo(const uint8* pData, size_t nSize, const TNetAddress& to)
{
	//if(m_disconnectMap.find(to) != m_disconnectMap.end())
	//  return false;

	ESocketError se = eSE_Ok;
	se = m_pSocketMain->Send(pData, nSize, to);

	if (se != eSE_Ok)
	{
		if (se >= eSE_Unrecoverable)
		{
			CNetChannel* pChannel = GetChannelForIP(to, 0);
			if (pChannel)
			{
				switch (se)
				{
				case eSE_UnreachableAddress:
					LobbySafeDisconnect(pChannel, eDC_ICMPError, "Host unreachable");
					break;
				case eSE_ConnectionRefused:
					LobbySafeDisconnect(pChannel, eDC_ICMPError, "Connection refused");
					break;
				case eSE_ConnectionReset:
					LobbySafeDisconnect(pChannel, eDC_ICMPError, "Connection reset by peer");
					break;
				default:
					LobbySafeDisconnect(pChannel, eDC_ICMPError, "Unrecoverable error on send");
				}
			}
			return false;
		}
		return true;
	}
	return true;
}

void CNetNub::OnPacket(const TNetAddress& from, const uint8* pData, uint32 nLength)
{
	NET_ASSERT(nLength);

	uint8 nType;

	nType = Frame_HeaderToID[pData[0]];

	bool processed = false;
	CNetChannel* pNetChannel = GetChannelForIP(from, 0);

	if (pNetChannel)
	{
		pNetChannel->ResetTimeSinceRecv();
	}

	if ((nType >= eH_TransportSeq0 && nType <= eH_TransportSeq_Last)
	    || (nType >= eH_SyncTransportSeq0 && nType <= eH_SyncTransportSeq_Last))
		processed = ProcessTransportPacketFrom(from, pData, nLength);
	else
		switch (nType)
		{
		case eH_KeepAlive:
		case eH_KeepAliveReply:
		case eH_BackOff:
			ProcessSimplePacket(from, pData, nLength);
			processed = true;
			break;
		case eH_CryLobby:
			processed = true;
			break;
		case eH_PingServer:
			processed = ProcessPingQuery(from, pData, nLength);
			break;
		case eH_QueryLan:
			ProcessLanQuery(from);
			processed = true;
			break;
#ifdef __WITH_PB__
		case eH_PunkBuster:
			ProcessPunkBuster(from, pData, nLength);
			processed = true;
			break;
#endif
		case eH_ConnectionSetup:
#if defined(PURE_CLIENT)
			if ((gEnv->bMultiplayer) && (gEnv->bServer) && (gEnv->IsClient()))
			{
				ProcessDisconnect(from, pData, 1);
			}
			else
#endif
			{
				ProcessSetup(from, pData, nLength);
			}
			processed = true;
			break;
		case eH_ConnectionSetup_KeyExchange_0:
			ProcessKeyExchange0(from, pData, nLength);
			processed = true;
			break;
		case eH_ConnectionSetup_KeyExchange_1:
#if defined(PURE_CLIENT)
			if (!(gEnv->bMultiplayer) || !(gEnv->bServer) || !(gEnv->IsClient()))
#endif
			{
				ProcessKeyExchange1(from, pData, nLength);
			}
#if defined(PURE_CLIENT)
			else
			{
				ProcessKeyExchange0(from, pData, nLength);
			}
#endif
			processed = true;
			break;
		case eH_Disconnect:
			ProcessDisconnect(from, pData, nLength);
			processed = true;
			break;
		case eH_DisconnectAck:
			ProcessDisconnectAck(from);
			processed = true;
			break;
		case eH_AlreadyConnecting:
			ProcessAlreadyConnecting(from, pData, nLength);
			processed = true;
			break;
		}

	if (!processed)
	{
		TraceUnrecognizedPacket("ProcessPacketFrom", (uint8*)pData, nLength, from);
	}
}

void CNetNub::ProcessSimplePacket(const TNetAddress& from, const uint8* pData, uint32 nLength)
{
	if (nLength != 1)
		return;

	uint8 nType = Frame_HeaderToID[pData[0]];

	TDisconnectMap::iterator iterDis = m_disconnectMap.find(from);
	if (iterDis != m_disconnectMap.end())
		SendDisconnect(from, iterDis->second);
	else if (CNetChannel* pNetChannel = GetChannelForIP(from, 0))
		pNetChannel->ProcessSimplePacket((EHeaders)nType);
}

bool CNetNub::ProcessTransportPacketFrom(const TNetAddress& from, const uint8* pData, uint32 nLength)
{
	//	NET_ASSERT(  == CTPFrameHeader );

	//	uint8 type = GetFrameType(((uint8*)MMM_PACKETDATA.PinHdl(hdl))[0]);
	//	NET_ASSERT( type == CTPFrameHeader || type == CTPSyncFrameHeader );
	uint8 nType = Frame_HeaderToID[pData[0]];

	TDisconnectMap::iterator iterDis = m_disconnectMap.find(from);
	if (iterDis != m_disconnectMap.end())
	{
		SendDisconnect(from, iterDis->second);
		return true;
	}

	CNetChannel* pNetChannel = GetChannelForIP(from, 0);

	if (pNetChannel)
	{
		pNetChannel->ProcessPacket((nType >= eH_SyncTransportSeq0 && nType <= eH_SyncTransportSeq_Last), pData, nLength);
		return true;
	}

	// legal packet, but we don't know what to do with it yet
	return true;
}

void CNetNub::ProcessDisconnectAck(const TNetAddress& from)
{
	ASSERT_GLOBAL_LOCK;
	m_disconnectMap.erase(from);
}

void CNetNub::ProcessDisconnect(const TNetAddress& from, const uint8* pData, uint32 nSize)
{
	CNetChannel* pNetChannel = GetChannelForIP(from, 0);
	uint8 cause = eDC_Unknown;
	string reason;

	if (nSize < 2)
	{
		// this is definitely a corrupted packet, disconnect the channel, if any
		cause = eDC_ProtocolError;
		reason = "Corrupted disconnect packet";
		if (pNetChannel && !pNetChannel->IsDead())
			DisconnectChannel(eDC_ProtocolError, &from, pNetChannel, reason.c_str());
	}
	else
	{
		std::vector<string> r;
		cause = pData[1];
		STATIC_CHECK((eDC_Timeout == 0), eDC_Timeout_NOT_EQUAL_0);//fix compiler warning
		if (/*cause < eDC_Timeout || */ cause > eDC_Unknown)//first term not required anymore (due to previous line)
		{
			cause = eDC_ProtocolError;
			reason = "Illegal disconnect packet";
		}
		else
		{
			string backlog((char*)pData + 2, (char*)pData + nSize);
			ParseBacklogString(backlog, reason, r);
			if (reason.empty())
				reason = "No reason";
		}
		if (pNetChannel && !pNetChannel->IsDead())
		{
			NetLog("Received disconnect packet from %s: %s", RESOLVER.ToString(from).c_str(), reason.c_str());
			for (size_t i = 0; i < r.size(); ++i)
				NetLog("  [remote] %s", r[i].c_str());
			DisconnectChannel((EDisconnectionCause)cause, &from, pNetChannel, ("Remote disconnected: " + reason).c_str());
		}
	}

	// if we received a disconnect packet without a channel, we should try removing pre-channel record possibly in
	// m_pendingConnections or m_connectingMap
	if (!pNetChannel)
	{
		// active connect, need to inform GameNub
		for (TPendingConnectionSet::iterator iter = m_pendingConnections.begin(); iter != m_pendingConnections.end(); ++iter)
		{
			if (iter->to == from)
			{
				NetWarning("Connection to %s refused", RESOLVER.ToString(from).c_str());
				NetWarning("%s", reason.c_str());
				TO_GAME(&CNetNub::GC_FailedActiveConnect, this, (EDisconnectionCause)cause, RESOLVER.ToString(from).c_str() + string(" refused connection\n") + reason, CNubKeepAliveLock(this));
				m_pendingConnections.erase(iter);
				break;
			}
		}

		TConnectingMap::iterator iter = m_connectingMap.find(from);
		if (iter != m_connectingMap.end())
		{
			NetWarning("%s disconnected pre-maturely", RESOLVER.ToString(from).c_str());
			NetWarning("%s", reason.c_str());
			m_connectingMap.erase(iter->first);
			// no need to add a disconnect entry, since it's a remote disconnect
		}
	}

	AckDisconnect(from);
}

void CNetNub::AckDisconnect(const TNetAddress& to)
{
	TAckDisconnectSet::iterator it = m_ackDisconnectSet.lower_bound(to);
	if (it == m_ackDisconnectSet.end() || !equiv(to, *it))
	{
		SendTo(&Frame_IDToHeader[eH_DisconnectAck], 1, to);
		m_ackDisconnectSet.insert(it, to);
	}
}

void CNetNub::GC_FailedActiveConnect(EDisconnectionCause cause, string description, CNubKeepAliveLock)
{
	// this function is executed on the game thread with the global lock held
	//SCOPED_GLOBAL_LOCK; - no need to lock

	m_pGameNub->FailedActiveConnect(cause, description.c_str());
}

void CNetNub::ProcessAlreadyConnecting(const TNetAddress& from, const uint8* pData, uint32 nSize)
{
	NetLog("Started connection to %s [waiting for data]", RESOLVER.ToString(from).c_str());
	for (TPendingConnectionSet::iterator iter = m_pendingConnections.begin(); iter != m_pendingConnections.end(); ++iter)
	{
		if (iter->to == from)
		{
			iter->connectCounter = -5;
			break;
		}
	}
}

void CNetNub::DisconnectChannel(EDisconnectionCause cause, const TNetAddress* pFrom, CNetChannel* pChannel, const char* reason)
{
	if (!pFrom)
	{
		NET_ASSERT(pChannel);
		for (TChannelMap::const_iterator iter = m_channels.begin(); iter != m_channels.end(); ++iter)
			if (iter->second == pChannel)
				pFrom = &iter->first;
	}
	if (!pChannel)
	{
		NET_ASSERT(pFrom);
		PREFAST_ASSUME(pFrom);
		pChannel = GetChannelForIP(*pFrom, 0);
	}

	if (pChannel)
	{
		pChannel->Destroy(cause, reason);

		for (TPendingConnectionSet::iterator iter = m_pendingConnections.begin(); iter != m_pendingConnections.end(); ++iter)
		{
			if (iter->pChannel == pChannel)
			{
				m_pendingConnections.erase(iter);
				break;
			}
		}
	}

	if (cause == eDC_ContextCorruption && CVARS.LogLevel > 0)
	{
		CNetwork::Get()->BroadcastNetDump(eNDT_ObjectState);
		CNetwork::Get()->AddExecuteString("es_dump_entities");
	}

	if (pFrom)
	{
		AddDisconnectEntry(*pFrom, pChannel ? pChannel->GetSession() : CrySessionInvalidHandle, cause, reason);
	}
}

void CNetNub::DisconnectChannel(EDisconnectionCause cause, CrySessionHandle session, const char* pReason)
{
	SCOPED_GLOBAL_LOCK;

	ICryMatchMakingPrivate* pMMPrivate = m_pLobby ? m_pLobby->GetMatchMakingPrivate() : nullptr;
	if (pMMPrivate)
	{
		uint32 id = pMMPrivate->GetChannelIDFromGameSessionHandle(session);

		for (TChannelMap::const_iterator iter = m_channels.begin(); iter != m_channels.end(); ++iter)
		{
			CNetChannel* pChannel = iter->second->GetNetChannel();

			if (pMMPrivate->GetChannelIDFromGameSessionHandle(pChannel->GetSession()) == id)
			{
				DisconnectChannel(cause, &iter->first, pChannel, pReason);
			}
		}
	}
}

INetChannel* CNetNub::GetChannelFromSessionHandle(CrySessionHandle session)
{
	SCOPED_GLOBAL_LOCK;

	ICryMatchMakingPrivate* pMMPrivate = m_pLobby ? m_pLobby->GetMatchMakingPrivate() : nullptr;
	if (pMMPrivate)
	{
		uint32 id = pMMPrivate->GetChannelIDFromGameSessionHandle(session);

		for (TChannelMap::const_iterator iter = m_channels.begin(); iter != m_channels.end(); ++iter)
		{
			CNetChannel* pChannel = iter->second->GetNetChannel();

			if (pMMPrivate->GetChannelIDFromGameSessionHandle(pChannel->GetSession()) == id)
			{
				return pChannel;
			}
		}
	}
	return NULL;
}

void CNetNub::NetDump(ENetDumpType type)
{
	CDefaultNetDumpLogger logger;

	NetDump(type, logger);
}

void CNetNub::NetDump(ENetDumpType type, INetDumpLogger& logger)
{
	if ((type == eNDT_ClientConnectionState) || (type == eNDT_ServerConnectionState))
	{
		CRYSOCKADDR_IN addr;
		string sockaddrStr;
		const char* pTypeName;

		if (ConvertAddr(m_addr, &addr))
		{
			sockaddrStr.Format("%s:%u", CrySock::inet_ntoa(addr.sin_addr), addr.sin_port);
		}
		else
		{
			sockaddrStr = "unknown";
		}

		if (type == eNDT_ClientConnectionState)
		{
			pTypeName = "client";
		}
		else
		{
			pTypeName = "server";
		}

		logger.Log("Local %s nub:", pTypeName);
		logger.Log("  Socket address: %s", sockaddrStr.c_str());
		logger.Log("  %u connections", m_channels.size());
	}

	for (TChannelMap::const_iterator iter = m_channels.begin(); iter != m_channels.end(); ++iter)
		iter->second->NetDump(type, logger);
}

void CNetNub::SendDisconnect(const TNetAddress& to, SDisconnect& dis)
{
	CTimeValue now = g_time;
	if (dis.lastNotify + 0.5f >= now)
		return;

#if defined(_DEBUG)
	uint8 buffer[2 + sizeof(dis.info) + sizeof(dis.backlog)]; // frame + cause + reason + backlog
#else
	uint8 buffer[2 + sizeof(dis.info)]; // frame + cause + reason
#endif
	NET_ASSERT(sizeof(buffer) <= MAX_TRANSMISSION_PACKET_SIZE);
	buffer[0] = Frame_IDToHeader[eH_Disconnect];
	buffer[1] = dis.cause;
	memcpy(buffer + 2, dis.info, dis.infoLength);
#if defined(_DEBUG)
	memcpy(buffer + 2 + dis.infoLength, dis.backlog, dis.backlogLen);
	SendTo(buffer, 2 + dis.infoLength + dis.backlogLen, to);
#else
	SendTo(buffer, 2 + dis.infoLength, to);
#endif

	dis.lastNotify = now;
}

bool CNetNub::ProcessPingQuery(const TNetAddress& from, const uint8* pData, uint32 nSize)
{
	//NET_ASSERT( GetFrameType(((uint8*)MMM_PACKETDATA.PinHdl(hdl))[0]) == QueryFrameHeader );

	if (!m_pGameQuery)
	{
		return false;
	}

	static const size_t BufferSize = 64;
	uint8 buffer[BufferSize];
	if (nSize > BufferSize)
	{
		return false;
	}
	memcpy(buffer, pData, nSize);
	buffer[1] = 'O'; // PONG
	m_pSocketMain->Send(buffer, nSize, from);
	return true;
}

void CNetNub::ProcessLanQuery(const TNetAddress& from)
{
	NET_ASSERT(m_pGameQuery);

	XmlNodeRef xml = m_pGameQuery->GetGameState();
	XmlString str = xml->getXML();
	m_pSocketMain->Send((const uint8*)str.c_str(), str.length(), from);
}

void CNetNub::LobbySafeDisconnect(CNetChannel* pChannel, EDisconnectionCause cause, const char* reason)
{
	ICryMatchMakingPrivate* pMMPrivate = m_pLobby ? m_pLobby->GetMatchMakingPrivate() : nullptr;
	if (pMMPrivate)
	{
		return;   // dont process the disconnect event, leave it for the lobby to deal with
	}
	pChannel->Disconnect(cause, reason);
}

void CNetNub::OnError(const TNetAddress& from, ESocketError err)
{
	NetWarning("%d from %s", err, RESOLVER.ToString(from).c_str());
	CNetChannel* pNetChannel = GetChannelForIP(from, eCLF_HandleBrokenWindowsICMP);
	if (pNetChannel)
	{
		if (err == eSE_FragmentationOccured)
		{
#if !NEW_BANDWIDTH_MANAGEMENT
			pNetChannel->FragmentedPacket();
#else
			NetLog("CNetNub::OnError() received eSE_FragmentationOccured from %s", RESOLVER.ToString(from).c_str());
#endif // !NEW_BANDWIDTH_MANAGEMENT
		}
		else
		{
			if (err == eSE_UnreachableAddress)
			{
				LobbySafeDisconnect(pNetChannel, eDC_ICMPError, ("Unreachable address " + RESOLVER.ToString(from)).c_str());
			}
			else
			{
				if (err >= eSE_Unrecoverable)
				{
					LobbySafeDisconnect(pNetChannel, eDC_Unknown, "Unknown unrecoverable error");
				}
			}
		}
	}

	for (TPendingConnectionSet::iterator iter = m_pendingConnections.begin(); iter != m_pendingConnections.end(); ++iter)
	{
		bool done = false;
		for (TNetAddressVec::const_iterator it = iter->tos.begin(); it != iter->tos.end(); ++it)
		{
			if (*it == from)
			{
				TO_GAME(&CNetNub::GC_FailedActiveConnect, this, eDC_CantConnect, string().Format("Connection attempt to %s failed", RESOLVER.ToString(from).c_str()), CNubKeepAliveLock(this));
				m_pendingConnections.erase(iter);
				done = true;
				break;
			}
		}
		if (done)
			break;
	}

	TConnectingMap::iterator iterCon = LookupAddress(from, m_connectingMap, eCLF_HandleBrokenWindowsICMP);
	if (iterCon != m_connectingMap.end())
		m_connectingMap.erase(iterCon->first);
}

bool CNetNub::SendKeyExchange0(const TNetAddress& to, SConnecting& con, bool doNotRegenerate)
{
	static const size_t KS = sizeof(CExponentialKeyExchange::KEY_TYPE);
	uint8 pktbuf[1 + 3 * KS + sizeof(uint32)];

	pktbuf[0] = Frame_IDToHeader[eH_ConnectionSetup_KeyExchange_0];
#if ALLOW_ENCRYPTION
	if (doNotRegenerate)
	{
	}
	else
	{
		con.exchange.Generate(con.g, con.p, con.A);
	}
	memcpy(pktbuf + 1, &con.g, KS);
	memcpy(pktbuf + 1 + KS, &con.p, KS);
	memcpy(pktbuf + 1 + 2 * KS, &con.A, KS);
#else
	memset(pktbuf + 1, 0, KS + KS + KS);    // Lets not send the stack
#endif
	uint32 socketCaps = htonl(CNetwork::Get()->GetExternalSocketIOManager().caps);
	memcpy(pktbuf + 1 + 3 * KS, &socketCaps, sizeof(uint32));
	return SendTo(pktbuf, sizeof(pktbuf), to);
}

bool CNetNub::SendKeyExchange1(SPendingConnection& pc, bool doNotRegenerate)
{
	static const size_t KS = sizeof(CExponentialKeyExchange::KEY_TYPE);
	uint8 pktbuf[1 + KS];

	pktbuf[0] = Frame_IDToHeader[eH_ConnectionSetup_KeyExchange_1];
#if ALLOW_ENCRYPTION
	if (doNotRegenerate)
	{
	}
	else
	{
		pc.exchange.Generate(pc.B, pc.g, pc.p, pc.A);
	}
	assert(sizeof(pc.B) == KS);
	memcpy(pktbuf + 1, &pc.B, KS);
#else
	memset(pktbuf + 1, 0, KS);    // Lets not send the stack
#endif
	return SendTo(pktbuf, sizeof(pktbuf), pc.to);
}

void CNetNub::ProcessKeyExchange0(const TNetAddress& from, const uint8* pData, uint32 nSize)
{
	if (GetChannelForIP(from, 0) != NULL)
		return;

	for (TPendingConnectionSet::iterator iter = m_pendingConnections.begin(); iter != m_pendingConnections.end(); ++iter)
	{
		if (!(iter->to == from))
		{
			for (TNetAddressVec::const_iterator it = iter->tos.begin(); it != iter->tos.end(); ++it)
			{
				if (*it == from)
				{
					iter->to = from;
					goto L_continue;
				}
			}

			continue;
		}

L_continue:

		if (iter->kes != eKES_SetupInitiated)
			break;

		if (nSize != 1 + 3 * CExponentialKeyExchange::KEY_SIZE + sizeof(uint32))
		{
			NetWarning("processing KeyExchange0 with illegal sized packet");
			break;
		}

		++pData;
#if ALLOW_ENCRYPTION
		memcpy(iter->g.key, pData, CExponentialKeyExchange::KEY_SIZE);
		pData += CExponentialKeyExchange::KEY_SIZE;
		memcpy(iter->p.key, pData, CExponentialKeyExchange::KEY_SIZE);
		pData += CExponentialKeyExchange::KEY_SIZE;
		memcpy(iter->A.key, pData, CExponentialKeyExchange::KEY_SIZE);
		pData += CExponentialKeyExchange::KEY_SIZE;
#else
		pData += CExponentialKeyExchange::KEY_SIZE + CExponentialKeyExchange::KEY_SIZE + CExponentialKeyExchange::KEY_SIZE;
#endif
		memcpy(&iter->socketCaps, pData, sizeof(uint32));
		pData += sizeof(uint32);
		iter->socketCaps = ntohl(iter->socketCaps);

		if (!SendKeyExchange1(*iter))
		{
			NetWarning("failed sending KeyExchange1");
			break;
		}
#if ALLOW_ENCRYPTION
		const CExponentialKeyExchange::KEY_TYPE cKey = iter->exchange.GetKey();
#else
		CExponentialKeyExchange::KEY_TYPE cKey;   // dummy
#endif

		iter->kes = eKES_KeyEstablished;
		iter->kesStart = g_time;
		CreateChannel(iter->to, cKey, "" /* NULL */, iter->socketCaps, iter->profile, iter->session, iter->conlk);   // the empty string ("") will be translated to NULL when creating GameChannel
		break;
	}
}

void CNetNub::ProcessKeyExchange1(const TNetAddress& from, const uint8* pData, uint32 nSize)
{
	if (GetChannelForIP(from, 0) != NULL)
		return;

	TConnectingMap::iterator iterCon = m_connectingMap.find(from);
	if (iterCon == m_connectingMap.end())
	{
		AddDisconnectEntry(from, CrySessionInvalidHandle, eDC_CantConnect, "KeyExchange1 with no connection");
		return; // ignore silently
	}

	if (iterCon->second.kes != eKES_SentKeyExchange)
		return;

	if (nSize != 1 + CExponentialKeyExchange::KEY_SIZE)
	{
		NetWarning("processing KeyExchange1 with illegal sized packet");
		AddDisconnectEntry(from, iterCon->second.session, eDC_CantConnect, "processing KeyExchange1 with illegal sized packet");
		return;
	}

#if ALLOW_ENCRYPTION
	++pData;
	memcpy(iterCon->second.B.key, pData, CExponentialKeyExchange::KEY_SIZE);

	iterCon->second.exchange.Calculate(iterCon->second.B);

	const CExponentialKeyExchange::KEY_TYPE cKey = iterCon->second.exchange.GetKey();
#else
	CExponentialKeyExchange::KEY_TYPE cKey; //dummy
#endif

	iterCon->second.kes = eKES_KeyEstablished;
	iterCon->second.kesStart = g_time;

	// now that we have established the private key, let's go create the channel
	CreateChannel(from, cKey, iterCon->second.connectionString, iterCon->second.socketCaps, iterCon->second.profile, iterCon->second.session, CNubConnectingLock());
}

void CNetNub::ProcessSetup(const TNetAddress& from, const uint8* pData, uint32 nSize)
{
#if defined(PURE_CLIENT)
	if ((gEnv->bMultiplayer) && (gEnv->bServer == gEnv->IsClient()))
	{
		return ProcessDisconnect(from, pData, 1);
	}
#endif
	//	if (m_pSecurity->IsIPBanned(from.GetAsUINT()))
	//	{
	//		NetWarning("Setup discarded for %s (BANNED)", RESOLVER.ToString(from));
	//		return;
	//	}

	if (GetChannelForIP(from, 0) != NULL)
		return;

	if (nSize < 1 + VERSION_SIZE * sizeof(uint32) + 2 * sizeof(uint32) + sizeof(CrySessionHandle))
	{
		AddDisconnectEntry(from, CrySessionInvalidHandle, eDC_ProtocolError, "Too short connection packet");
		return;
	}

	SFileVersion ver = gEnv->pSystem->GetProductVersion();
	uint32 v[VERSION_SIZE];
	memcpy(v, pData + 1, VERSION_SIZE * sizeof(uint32));

	uint32 profile_data = 0;
	memcpy(&profile_data, (char*)pData + 1 + VERSION_SIZE * sizeof(uint32), sizeof(uint32));
	uint32 profile = ntohl(profile_data);

	memcpy(&profile_data, (char*)pData + 1 + VERSION_SIZE * sizeof(uint32) + sizeof(uint32), sizeof(uint32));
	uint32 tokenId = ntohl(profile_data);

	CrySessionHandle session;
	memcpy(&session, (char*)pData + 1 + VERSION_SIZE * sizeof(uint32) + 2 * sizeof(uint32), sizeof(CrySessionHandle));
	session = ntohl(session);

	for (int i = 0; i < 4; i++)
	{
		if (v[i] != ntohl(ver.v[i]))
		{
			AddDisconnectEntry(from, session, eDC_VersionMismatch, "Build version mismatch");
			m_connectingMap.erase(from);
			return;
		}
	}
	if (v[4] != ntohl(PROTOCOL_VERSION))
	{
		AddDisconnectEntry(from, session, eDC_VersionMismatch, "Protocol version mismatch");
		m_connectingMap.erase(from);
		return;
	}

	if (!stl::get_if<TLocalNetAddress>(&from))
	{
		INetworkServicePtr pNetTokens = CNetwork::Get()->GetService("NetProfileTokens");

		if (pNetTokens && (eNSS_Ok == pNetTokens->GetState()))
		{
			if (!pNetTokens->GetNetProfileTokens()->IsValid(profile, tokenId))
			{
				AddDisconnectEntry(from, session, eDC_VersionMismatch, "Client token id mismatch");
				m_connectingMap.erase(from);
				return;
			}
		}
	}
	string connectionString((char*)pData + 1 + VERSION_SIZE * sizeof(uint32) + 2 * sizeof(uint32) + sizeof(CrySessionHandle), (char*)pData + nSize);

	bool doNotRegenerate = false;
	TConnectingMap::iterator iterCon = m_connectingMap.find(from);
	if (iterCon != m_connectingMap.end())
	{
		switch (iterCon->second.kes)
		{
		case eKES_NotStarted:
		case eKES_SetupInitiated:
			AddDisconnectEntry(from, session, eDC_ProtocolError, "Key establishment handshake ordering error");
			return;
		case eKES_SentKeyExchange:
			break;
		case eKES_KeyEstablished:
			return; // ignore
		}

		// we are in eKES_SentKeyExchange state already while we get a ConnectionSetup message, probably
		// our KeyExchange0 message was lost, resend it
		doNotRegenerate = true; // do not regenerate keys
	}
	else
		iterCon = m_connectingMap.insert(std::make_pair(from, SConnecting())).first;

	SendKeyExchange0(from, iterCon->second, doNotRegenerate);

	if (gEnv->bMultiplayer && m_pLobby && m_pLobby->GetMatchMaking())
	{
		// m_disconnectMap can sometimes still hold this address if client disconnects and connect again quickly and some of the disconnect packets got lost.
		// If this happens this client will be disconnected straight away as we think it should have been disconnected.
		// m_disconnectMap isn't really needed when CryMatchMaking is in use as CryMatchMaking handles connects and disconnects
		// but we still need to keep it in case CryMatchMaking isn't in use.
		// What we can say is if CryMatchMaking knows of this address then it doesn't want to be disconnected because of m_disconnectMap so make sure it is not stored.
		m_disconnectMap.erase(from);

		CRY_ASSERT_MESSAGE(session != CrySessionInvalidHandle, "Incoming connection with no session");

		if (session == CrySessionInvalidHandle || !m_pLobby->GetMatchMakingPrivate()->IsNetAddressInLobbyConnection(from))
		{
			if (session != CrySessionInvalidHandle)
			{
				CryLogAlways("Canary : Duplicate Channel ID - Work around");
			}
			AddDisconnectEntry(from, session, eDC_ProtocolError, "Invalid session");
		}
	}

	iterCon->second.connectionString = connectionString;
	iterCon->second.kes = eKES_SentKeyExchange;
	iterCon->second.socketCaps = ntohl(v[5]);
	iterCon->second.profile = profile;
	iterCon->second.session = session;
	if (!doNotRegenerate)
		iterCon->second.kesStart = g_time;
}

CNetChannel* CNetNub::GetChannelForIP(const TNetAddress& ip, uint32 flags)
{
	ASSERT_GLOBAL_LOCK;
	TChannelMap::iterator i = LookupAddress(ip, m_channels, flags);
	return i == m_channels.end() ? NULL : i->second->GetNetChannel();
}

TNetAddress CNetNub::GetIPForChannel(const INetChannel* ch)
{
	for (TChannelMap::iterator i = m_channels.begin(), e = m_channels.end(); i != e; ++i)
	{
		if (i->second->GetNetChannel() == ch)
			return i->first;
	}
	return TNetAddress();
}

void CNetNub::GetMemoryStatistics(ICrySizer* pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "CNetNub");

	if (!pSizer->Add(*this))
		return;
	pSizer->AddContainer(m_channels);
	for (TChannelMap::iterator i = m_channels.begin(); i != m_channels.end(); ++i)
	{
		if (CNetChannel* pNetChannel = i->second->GetNetChannel())
			pNetChannel->GetMemoryStatistics(pSizer, true);
	}
	m_pSocketMain->GetMemoryStatistics(pSizer);
}

void CNetNub::CreateChannel(const TNetAddress& ip, const CExponentialKeyExchange::KEY_TYPE& key, const string& connectionString, uint32 remoteSocketCaps, uint32 proifle, CrySessionHandle session, CNubConnectingLock conlk)
{
	// this function is executed on the network thread

	CNetChannel* pNetChannel = new CNetChannel(ip, key, remoteSocketCaps, session);    // I don't want to pass the 32 bytes key by value between threads
	pNetChannel->SetProfileId(proifle);

	TO_GAME(&CNetNub::GC_CreateChannel, this, pNetChannel, connectionString, conlk);
}

void CNetNub::GC_CreateChannel(CNetChannel* pNetChannel, string connectionString, CNubConnectingLock conlk)
{
	// this function is executed on the game thread with the global lock held
	// see CSystem::Update -> CNetwork::SyncWithGame
	//SCOPED_GLOBAL_LOCK; - no need to lock

	TConnectingMap::iterator iterCon = m_connectingMap.find(pNetChannel->GetIP());
	bool fromRequest = iterCon != m_connectingMap.end();

	if (!fromRequest)
	{
		bool pending = false;
		for (TPendingConnectionSet::iterator iter = m_pendingConnections.begin(); iter != m_pendingConnections.end(); ++iter)
		{
			if (iter->to == pNetChannel->GetIP())
			{
				iter->pChannel = pNetChannel;
				pending = true;
				break;
			}
		}

		if (!pending)
		{
			// the IP associated with the NetChannel is not tracked (we didn't find it in either list), probably it gets removed pre-maturely
			AddDisconnectEntry(pNetChannel->GetIP(), pNetChannel->GetSession(), eDC_CantConnect, "Pending connection not found");
			delete pNetChannel;
			return;
		}
	}

	if (gEnv->bMultiplayer && m_pLobby && m_pLobby->GetMatchMaking())
	{
		if (!m_pLobby->GetMatchMakingPrivate()->IsNetAddressInLobbyConnection(pNetChannel->GetIP()))
		{
			CryLogAlways("Canary : Duplicate Channel ID - Work around");

			if (fromRequest)
			{
				m_connectingMap.erase(iterCon->first);
			}

			AddDisconnectEntry(pNetChannel->GetIP(), pNetChannel->GetSession(), eDC_CantConnect, "Address not found in MatchMaking");
			delete pNetChannel;

			return;
		}
	}

	SCreateChannelResult res = m_pGameNub->CreateChannel(pNetChannel, connectionString.empty() ? NULL : connectionString.c_str());
	if (!res.pChannel)
	{
		if (fromRequest)
			m_connectingMap.erase(iterCon->first);
		AddDisconnectEntry(pNetChannel->GetIP(), pNetChannel->GetSession(), res.cause, res.errorMsg);
		delete pNetChannel;
		return;
	}

	pNetChannel->Init(this, res.pChannel);

	ASSERT_GLOBAL_LOCK;
	m_channels.insert(std::make_pair(pNetChannel->GetIP(), pNetChannel));
	if (fromRequest)
		m_connectingMap.erase(iterCon->first);
}

#if NEW_BANDWIDTH_MANAGEMENT
uint32 CNetNub::GetTotalBandwidthShares() const
{
	uint32 totalShares = 0;

	TChannelMap::const_iterator end = m_channels.end();
	for (TChannelMap::const_iterator iter = m_channels.begin(); iter != end; ++iter)
	{
		CNetChannel* pNetChannel = iter->second->GetNetChannel();
		if (!pNetChannel->IsLocal())
		{
			totalShares += pNetChannel->GetPerformanceMetrics()->m_bandwidthShares;
		}
	}

	if (totalShares == 0)
	{
		// Until there are more than one non-local channels to share bandwidth between, just use all of it
		totalShares = 1;
	}

	return totalShares;
}
#endif // NEW_BANDWIDTH_MANAGEMENT

void CNetNub::SendConnecting(const TNetAddress& to, SConnecting& con)
{
	CTimeValue now = g_time;
	if (con.lastNotify + 0.5f >= now)
		return;

	SendTo(Frame_IDToHeader + eH_AlreadyConnecting, 1, to);

	con.lastNotify = now;
}

void CNetNub::AddDisconnectEntry(const TNetAddress& ip, CrySessionHandle session, EDisconnectionCause cause, const char* reason)
{
	// If we are using CCryMatchMaking then it is responsible for the connections so don't send nub disconnects just inform matchmaking.
	if (m_pLobby && m_pLobby->GetMatchMaking())
	{
		CryLog("CNetNub::AddDisconnectEntry calling SessionDisconnectRemoteConnection reason %s", reason ? reason : "no reason");
		m_pLobby->GetMatchMakingPrivate()->SessionDisconnectRemoteConnectionFromNub(session, cause);
	}
	else
	{
		SDisconnect dc;
		dc.when = g_time;
		dc.lastNotify = 0.0f;
		dc.infoLength = strlen(reason);
		dc.cause = cause;
		if (dc.infoLength > sizeof(dc.info))
			dc.infoLength = sizeof(dc.info);
		memcpy(dc.info, reason, dc.infoLength);
#if !defined(EXCLUDE_NORMAL_LOG)
		string backlog = GetBacklogAsString();
		dc.backlogLen = backlog.size();
		if (dc.backlogLen > sizeof(dc.backlog))
			dc.backlogLen = sizeof(dc.backlog);
		memcpy(dc.backlog, backlog.data(), dc.backlogLen);
#else
		dc.backlogLen = 0;
		dc.backlog[0] = 0;
#endif // !defined(EXCLUDE_NORMAL_LOG)

		TDisconnectMap::iterator iterDis = m_disconnectMap.insert(std::make_pair(ip, dc)).first;

		if (cause != eDC_ICMPError) // If we are here because of eDC_ICMPError from a send then the send here will bring us here again and cause a stack overflow.
		{
			SendDisconnect(ip, iterDis->second);
		}
	}
}

bool CNetNub::IsSuicidal()
{
	if (IsDead())
		return true;
	if (m_keepAliveLocks)
		return true;
	if (m_bDead)
	{
		bool allChannelsSuicidal = true;
		for (TChannelMap::iterator iter = m_channels.begin(); iter != m_channels.end(); ++iter)
		{
			allChannelsSuicidal &= iter->second->IsSuicidal();
		}
		if (allChannelsSuicidal)
			return true;
	}
	return false;
}

CNetChannel* CNetNub::FindFirstClientChannel()
{
	for (TChannelMap::iterator iter = m_channels.begin(); iter != m_channels.end(); ++iter)
	{
		if (CNetChannel* pChan = iter->second->GetNetChannel())
			if (!pChan->IsServer())
				return pChan;
	}
	return 0;
}

CNetChannel* CNetNub::FindFirstRemoteChannel()
{
	for (TChannelMap::iterator iter = m_channels.begin(); iter != m_channels.end(); ++iter)
	{
		if (CNetChannel* pChan = iter->second->GetNetChannel())
		{
			if (!pChan->IsLocal())
			{
				return pChan;
			}
		}
	}

	return NULL;
}

//#if LOCK_NETWORK_FREQUENCY
void CNetNub::TickChannels(CTimeValue& now, bool force)
{
	TChannelMap::iterator end = m_channels.end();
	for (TChannelMap::iterator iter = m_channels.begin(); iter != end; ++iter)
	{
		if (CNetChannel* pChan = iter->second->GetNetChannel())
		{
#if USE_CHANNEL_TIMERS
			pChan->CallUpdate(now);
#else
			pChan->CallUpdateIfNecessary(now, force);
#endif // USE_CHANNEL_TIMERS
		}
	}
}
//#endif

bool CNetNub::IsEstablishingContext() const
{
	TChannelMap::const_iterator end = m_channels.end();
	for (TChannelMap::const_iterator iter = m_channels.begin(); iter != end; ++iter)
	{
		if (CNetChannel* pChan = iter->second->GetNetChannel())
		{
			if (!pChan->IsDead() && pChan->GetContextViewState() < eCVS_InGame)
			{
				return true;
			}
		}
	}
	return false;
}
