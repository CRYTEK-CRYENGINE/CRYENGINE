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
#include <CrySystem/CryVersion.h>

static const float STATS_UPDATE_INTERVAL_NUB = 0.25f;
static const float KESTIMEOUT = 30.0f;
static const int VERSION_SIZE = 4;

namespace EncryptionVersionFlags
{
	enum EFlags : uint8
	{
		eFlags_AES_CBC = BIT(0),
		eFlags_SHA_256_HMAC = BIT(1),
		eFlags_DF_KEY_EXCHAGE = BIT(2),
		eFlags_STREAM_CIPHER = BIT(3)
	};

	constexpr static uint8 GetFlags()
	{
		return 0
#if ALLOW_ENCRYPTION
#if ENCRYPTION_RIJNDAEL || ENCRYPTION_CNG_AES || ENCRYPTION_TOMCRYPT_AES
			| eFlags_AES_CBC
#elif ENCRYPTION_STREAMCIPHER
			| eFlags_STREAM_CIPHER
#else
#error "Unknown configuration"
#endif
#endif // ALLOW_ENCRYPTION

#if ALLOW_HMAC
#if HMAC_CNG_SHA256 || HMAC_TOMCRYPT_SHA256
			| eFlags_SHA_256_HMAC
#else
#error "Unknown configuration"
#endif
#endif // ALLOW_HMAC

#if ENCRYPTION_GENERATE_KEYS
			| eFlags_DF_KEY_EXCHAGE
#endif // ENCRYPTION_GENERATE_KEYS
			;
	}
}



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
			auto itOut = m.end();
			for (auto it = m.begin(); it != m.end(); ++it)
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
#if ENABLE_GAME_QUERY
	m_pGameQuery(pGameQuery),
#endif
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
	m_statsTimer = TIMER.ADDTIMER(g_time, TimerCallback, this, buffer);
#else
	m_statsTimer = TIMER.ADDTIMER(g_time, TimerCallback, this, nullptr);
#endif // TIMER_DEBUG

	m_pLobby = gEnv->pLobby;
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
		DisconnectChannel(eDC_NubDestroyed, i->first, *i->second->GetNetChannel(), "Nub destroyed");
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

#if !defined(EXCLUDE_NORMAL_LOG)
			const INetChannel::SPerformanceMetrics* pMetrics = pNetChannel->GetPerformanceMetrics();
			NetLogAlways(" %5d :   %6d  :   %3d   :  %3d   : %s", pNetChannel->GetLocalChannelID(), pMetrics->m_bandwidthShares, pMetrics->m_packetRate, pMetrics->m_packetRateIdle, pNetChannel->GetName());
#endif
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

	const bool isLocal = stl::get_if<TLocalNetAddress>(&pc.to) != nullptr;

	// Initialize in legacy mode
	pc.tokenMode = CNetProfileTokens::EMode::Legacy;
	pc.profileTokenPair.profile = CNetCVars::Get().ProfileId;
	pc.legacyToken = CNetCVars::Get().TokenId;
	ChannelSecurity::CHMac::FillDummyToken(pc.profileTokenPair.token);

	if (!isLocal)
	{
		INetworkServicePtr pNetTokens = CNetwork::Get()->GetService("NetProfileTokens");
		if (pNetTokens && (eNSS_Ok == pNetTokens->GetState()))
		{
			CNetProfileTokens* pNetProfileTokens = static_cast<CNetProfileTokens*>(pNetTokens->GetNetProfileTokens());
			pc.tokenMode = pNetProfileTokens->GetMode();
			switch (pc.tokenMode)
			{
			case CNetProfileTokens::EMode::ProfileTokens:
				pNetProfileTokens->Client_GetToken(pc.profileTokenPair);
				break;
			case CNetProfileTokens::EMode::Legacy:
				break;
			default:
				NetWarning("Unexpected NetProfileTokens mode");
				return;
			}
		}
	}

	if (pc.hmac.Init(pc.profileTokenPair.token))
	{
		if (SendPendingConnect(pc))
			m_pendingConnections.push_back(pc);
	}
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
			DisconnectChannel(cause, *pCh, reason);
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
		pThis->m_statsTimer = TIMER.ADDTIMER(g_time + STATS_UPDATE_INTERVAL_NUB, TimerCallback, pThis, buffer);
#else
		pThis->m_statsTimer = TIMER.ADDTIMER(g_time + STATS_UPDATE_INTERVAL_NUB, TimerCallback, pThis, nullptr);
#endif // TIMER_DEBUG
	}
}

bool CNetNub::SendPendingConnect(SPendingConnection& pc)
{
	// NOTE: a channel will be created after private key is established
	// so we might not have a valid channel here
	if (pc.pChannel && pc.pChannel->IsDead())
	{
		AddDisconnectEntry(pc.to, pc.session, pc.pChannel->GetHmac(), eDC_CantConnect, "Dead channel");
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

		const SFileVersion ver = gEnv->pSystem->GetProductVersion();
		uint32 buildVersion[VERSION_SIZE];
		for (int i = 0; i < 4; i++)
			buildVersion[i] = htonl(ver[i]);
		const uint32 protocolVersion = htonl(PROTOCOL_VERSION);
		const uint32 socketCaps = htonl(CNetwork::Get()->GetExternalSocketIOManager().caps);
		const uint8 encryptionFlags = EncryptionVersionFlags::GetFlags();

		const uint32 profile = htonl(pc.profileTokenPair.profile);
		const uint32 legacyToken = htonl(pc.legacyToken);
		const CrySessionHandle session = htonl(pc.session);

		const size_t messageSize = 1 
			+ (VERSION_SIZE + 2) * sizeof(uint32)  // versions 
			+ 1                                    // encryption flags
			+ 2 * sizeof(uint32)                   // profile/tokenId
			+ sizeof(CrySessionHandle)             // session
			;
		const size_t bufferSize = messageSize + ChannelSecurity::CHMac::HashSize;
		uint8 pBuffer[bufferSize];

		uint8* pCur = pBuffer;
		StepDataWrite(pCur, Frame_IDToHeader[eH_ConnectionSetup], false);
		StepDataWrite(pCur, buildVersion, VERSION_SIZE, false);
		StepDataWrite(pCur, protocolVersion, false);
		StepDataWrite(pCur, socketCaps, false);
		StepDataWrite(pCur, encryptionFlags, false);
		StepDataWrite(pCur, profile, false);
		StepDataWrite(pCur, legacyToken, false);
		StepDataWrite(pCur, session, false);
		
		const size_t curSize = pCur - pBuffer;
		CRY_ASSERT(curSize == messageSize);

		const bool hmacSucceeded = pc.hmac.HashAndFinish(pBuffer, curSize, pCur);
		bool bSuccess = hmacSucceeded && SendTo(pBuffer, bufferSize, pc.to);

		pc.lastSend = g_time;
		pc.connectCounter++;
		if ((pc.connectCounter % 10) == 0)
			NetWarning("Retrying connection to %s (%d)", RESOLVER.ToString(pc.to).c_str(), pc.connectCounter);

		if (!bSuccess)
		{
			AddSilentDisconnectEntry(pc.to, pc.session, eDC_CantConnect, "Failed sending connect packet");
		}

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
		return SendKeyExchange1(pc);
	}

	AddDisconnectEntry(pc.to, pc.session, pc.hmac, eDC_ProtocolError, "Key establishment handshake ordering error trying to send pending connect");

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
#if ENABLE_GAME_QUERY
	if (m_pGameQuery)
		flags |= eSF_BroadcastReceive;
#endif
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
#if ENABLE_GAME_QUERY
		case eH_PingServer:
			processed = ProcessPingQuery(from, pData, nLength);
			break;
		case eH_QueryLan:
			ProcessLanQuery(from);
			processed = true;
			break;
#endif
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
			ProcessDisconnectAck(from, pData, nLength);
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

void CNetNub::ProcessDisconnectAck(const TNetAddress& from, const uint8* pData, uint32 nLength)
{
	ASSERT_GLOBAL_LOCK;
	
	const size_t messageSize = 1;
	const size_t bufferSize = messageSize + ChannelSecurity::CHMac::HashSize;

	if (nLength != bufferSize)
	{
		return;
	}

	TDisconnectMap::iterator iter = m_disconnectMap.find(from);
	if (iter == m_disconnectMap.end())
	{
		return;
	}

	SDisconnect& dis = iter->second;
	if (!dis.hmac.HashFinishAndVerify(pData, messageSize, pData + messageSize))
	{
		return;
	}

	m_disconnectMap.erase(iter);
}

void CNetNub::ProcessDisconnect(const TNetAddress& from, const uint8* pData, uint32 nSize)
{
	const size_t minMessageSize = 1 + 1;   // 1 + cause
	const size_t minBufferSize = minMessageSize + ChannelSecurity::CHMac::HashSize;

	if (nSize < minBufferSize)
	{
		// this is definitely a corrupted packet, ignore
		return;
	}

	const size_t messageSize = nSize - ChannelSecurity::CHMac::HashSize;
	const uint8* pHmacData = pData + messageSize;

	CNetChannel* pNetChannel = GetChannelForIP(from, 0);
	TPendingConnectionSet::iterator pendingConnectionIter = m_pendingConnections.end();
	TConnectingMap::iterator connectingMapIter = m_connectingMap.end();

	// Verify
	ChannelSecurity::CHMac* pHmac = nullptr;
	{
		if (pNetChannel)
		{
			pHmac = &pNetChannel->GetHmac();
		}
		else
		{
			pendingConnectionIter = FindPendingConnection(from, false);
			if (pendingConnectionIter != m_pendingConnections.end())
			{
				pHmac = &pendingConnectionIter->hmac;
			}
			else
			{
				connectingMapIter = m_connectingMap.find(from);
				if (connectingMapIter != m_connectingMap.end())
				{
					pHmac = &connectingMapIter->second.hmac;
				}
			}
		}

		if (!pHmac) 
		{
			return; // packet from unknown source
		}

		if (!pHmac->HashFinishAndVerify(pData, messageSize, pHmacData))
		{
			return; // HMAC verification failed, ignore packet
		}
	}
	NET_ASSERT(pHmac != nullptr);

	// Parse
	uint8 cause = eDC_Unknown;
	string reason;
	std::vector<string> r;
	{
		const uint8* pCur = pData;
		++pCur;
		StepDataCopy(&cause, pCur, 1, false);
		STATIC_CHECK((eDC_Timeout == 0), eDC_Timeout_NOT_EQUAL_0);//fix compiler warning
		if (/*cause < eDC_Timeout || */ cause > eDC_Unknown)//first term not required anymore (due to previous line)
		{
			cause = eDC_ProtocolError;
			reason = "Illegal disconnect packet";
		}
		else
		{
			NET_ASSERT(pCur <= pData + messageSize);
			string backlog((const char*)pCur, (const char*)pData + messageSize);
			ParseBacklogString(backlog, reason, r);
			if (reason.empty())
				reason = "No reason";
		}	
	}

	// Act on disconnect
	if (pNetChannel && !pNetChannel->IsDead())
	{
		NetLog("Received disconnect packet from %s: %s", RESOLVER.ToString(from).c_str(), reason.c_str());
		for (size_t i = 0; i < r.size(); ++i)
			NetLog("  [remote] %s", r[i].c_str());
		DisconnectChannel((EDisconnectionCause)cause, from, *pNetChannel, ("Remote disconnected: " + reason).c_str());
	}

	// if we received a disconnect packet without a channel, we should try removing pre-channel record possibly in
	// m_pendingConnections or m_connectingMap
	if (!pNetChannel)
	{
		// active connect, need to inform GameNub
		if (pendingConnectionIter != m_pendingConnections.end())
		{
			NetWarning("Connection to %s refused", RESOLVER.ToString(from).c_str());
			NetWarning("%s", reason.c_str());
			TO_GAME(&CNetNub::GC_FailedActiveConnect, this, (EDisconnectionCause)cause, RESOLVER.ToString(from).c_str() + string(" refused connection\n") + reason, CNubKeepAliveLock(this));
			m_pendingConnections.erase(pendingConnectionIter);
		}

		if (connectingMapIter != m_connectingMap.end())
		{
			NetWarning("%s disconnected pre-maturely", RESOLVER.ToString(from).c_str());
			NetWarning("%s", reason.c_str());
			m_connectingMap.erase(connectingMapIter);
			// no need to add a disconnect entry, since it's a remote disconnect
		}
	}

	AckDisconnect(from, *pHmac);
}

void CNetNub::AckDisconnect(const TNetAddress& to, ChannelSecurity::CHMac& hmac)
{
	TAckDisconnectSet::iterator it = m_ackDisconnectSet.lower_bound(to);
	if (it == m_ackDisconnectSet.end() || !equiv(to, *it))
	{
		const size_t messageSize = 1;
		const size_t bufferSize = messageSize + ChannelSecurity::CHMac::HashSize;
		uint8 buffer[bufferSize];

		buffer[0] = Frame_IDToHeader[eH_DisconnectAck];

		if (!hmac.HashAndFinish(buffer, messageSize, buffer + messageSize))
		{
			return;
		}

		SendTo(buffer, bufferSize, to);
		m_ackDisconnectSet.insert(it, to);
	}
}

void CNetNub::GC_FailedActiveConnect(EDisconnectionCause cause, string description, CNubKeepAliveLock)
{
	// this function is executed on the game thread with the global lock held
	//SCOPED_GLOBAL_LOCK; - no need to lock

	m_pGameNub->FailedActiveConnect(cause, description.c_str());
}

void CNetNub::DisconnectChannel(EDisconnectionCause cause, CNetChannel& channel, const char* reason)
{
	const TNetAddress* pFrom = nullptr;
	for (TChannelMap::const_iterator iter = m_channels.begin(); iter != m_channels.end(); ++iter)
	{
		if (iter->second == &channel)
		{
			pFrom = &iter->first;
			break;
		}
	}
	NET_ASSERT(pFrom != nullptr);
	if (!pFrom)
	{
		NetWarning("Unable to disconnect unknown channel");
		return;
	}

	DisconnectChannel(cause, *pFrom, channel, reason);
}

void CNetNub::DisconnectChannel(EDisconnectionCause cause, const TNetAddress& from, CNetChannel& channel, const char* reason)
{
	channel.Destroy(cause, reason);

	for (TPendingConnectionSet::iterator iter = m_pendingConnections.begin(); iter != m_pendingConnections.end(); ++iter)
	{
		if (iter->pChannel == &channel)
		{
			m_pendingConnections.erase(iter);
			break;
		}
	}

	if (cause == eDC_ContextCorruption && CVARS.LogLevel > 0)
	{
		CNetwork::Get()->BroadcastNetDump(eNDT_ObjectState);
		CNetwork::Get()->AddExecuteString("es_dump_entities");
	}

	AddDisconnectEntry(from, channel.GetSession(), channel.GetHmac(), cause, reason);
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
				DisconnectChannel(cause, iter->first, *pChannel, pReason);
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

	const size_t maxMessageSize = 1
		+ 1                         // cause
		+ sizeof(dis.info)          // reason
#if defined(_DEBUG)
		+ sizeof(dis.backlog)       // backlog
#endif
		;
	const size_t maxBufferSize = maxMessageSize + ChannelSecurity::CHMac::HashSize;
	static_assert(maxBufferSize <= MAX_TRANSMISSION_PACKET_SIZE, "Buffer size is too big");
	uint8 buffer[maxBufferSize];

	uint8* pCur = buffer;
	StepDataWrite(pCur, Frame_IDToHeader[eH_Disconnect], false);
	StepDataWrite(pCur, (uint8)dis.cause, false);
	StepDataWrite(pCur, dis.info, dis.infoLength, false);
#if defined(_DEBUG)
	StepDataWrite(pCur, dis.backlog, dis.backlogLen, false);
#endif

	const size_t messageSize = pCur - buffer;
	NET_ASSERT(messageSize <= maxMessageSize);
	const size_t bufferSize = messageSize + ChannelSecurity::CHMac::HashSize;

	if (!dis.hmac.HashAndFinish(buffer, messageSize, pCur))
	{
		return;
	}

	SendTo(buffer, bufferSize, to);
	dis.lastNotify = now;
}

#if ENABLE_GAME_QUERY
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
	if (m_pGameQuery)
	{
		XmlNodeRef xml = m_pGameQuery->GetGameState();
		XmlString str = xml->getXML();
		m_pSocketMain->Send((const uint8*)str.c_str(), str.length(), from);
	}
}
#endif


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
	const size_t KS = sizeof(CExponentialKeyExchange::KEY_TYPE);
	const size_t messageSize = 1 + 3 * KS + sizeof(uint32);
	const size_t bufferSize = messageSize + ChannelSecurity::CHMac::HashSize;
	uint8 pktbuf[bufferSize];

	uint8* pCur = pktbuf;

	StepDataWrite(pCur, Frame_IDToHeader[eH_ConnectionSetup_KeyExchange_0], false);
#if ENCRYPTION_GENERATE_KEYS
	if (!doNotRegenerate)
	{
		con.exchange.Generate(con.g, con.p, con.A);
	}
	StepDataWrite(pCur, con.g.key, KS, false);
	StepDataWrite(pCur, con.p.key, KS, false);
	StepDataWrite(pCur, con.A.key, KS, false);
#else
	memset(pCur, 0, 3 * KS);    // Lets not send the stack
	pCur += 3 * KS;
#endif
	uint32 socketCaps = htonl(CNetwork::Get()->GetExternalSocketIOManager().caps);
	StepDataWrite(pCur, socketCaps, false);

	const size_t curSize = pCur - pktbuf;
	CRY_ASSERT(curSize == messageSize);

	const bool hmacSucceeded = con.hmac.HashAndFinish(pktbuf, curSize, pCur);
	return hmacSucceeded && SendTo(pktbuf, bufferSize, to);
}

bool CNetNub::SendKeyExchange1(SPendingConnection& pc)
{
	static const size_t KS = sizeof(CExponentialKeyExchange::KEY_TYPE);
	const size_t messageSize = 1 
#if ENCRYPTION_GENERATE_KEYS
		+ KS
#endif
		+ pc.connectionStringBuf.size()
		;
	const size_t bufferSize = messageSize + ChannelSecurity::CHMac::HashSize;
	// cppcheck-suppress allocaCalled
	PREFAST_SUPPRESS_WARNING(6255) uint8* pktbuf = (uint8*)alloca(bufferSize);

	uint8* pCur = pktbuf;

	StepDataWrite(pCur, Frame_IDToHeader[eH_ConnectionSetup_KeyExchange_1], false);
#if ENCRYPTION_GENERATE_KEYS
	static_assert(sizeof(pc.B) == KS, "invalid size");
	StepDataWrite(pCur, pc.B.key, KS, false);
#endif

	StepDataWrite(pCur, pc.connectionStringBuf.data(), pc.connectionStringBuf.size(), false);

	const size_t curSize = pCur - pktbuf;
	CRY_ASSERT(curSize == messageSize);

	const bool hmacSucceeded = pc.hmac.HashAndFinish(pktbuf, curSize, pCur);
	return hmacSucceeded && SendTo(pktbuf, bufferSize, pc.to);
}

void CNetNub::ProcessKeyExchange0(const TNetAddress& from, const uint8* pData, uint32 nSize)
{
	if (GetChannelForIP(from, 0) != NULL)
		return;

	TPendingConnectionSet::iterator iter = FindPendingConnection(from, true);
	if (iter == m_pendingConnections.end())
		return;

	SPendingConnection& con = *iter;
	if (con.kes != eKES_SetupInitiated)
		return;

	const size_t KS = sizeof(CExponentialKeyExchange::KEY_TYPE);
	const size_t messageSize = 1 + 3 * KS + sizeof(uint32);
	const size_t bufferSize = messageSize + ChannelSecurity::CHMac::HashSize;

	if (nSize != bufferSize)
	{
		NetWarning("processing KeyExchange0 with illegal sized packet");
		return;
	}

	if (!con.hmac.HashFinishAndVerify(pData, messageSize, pData + messageSize))
	{
		return;
	}

	const uint8* pCur = pData;
	pCur += 1;
#if ENCRYPTION_GENERATE_KEYS
	StepDataCopy(con.g.key, pCur, KS, false);
	StepDataCopy(con.p.key, pCur, KS, false);
	StepDataCopy(con.A.key, pCur, KS, false);
#else
	pCur += 3 * KS;
#endif

	uint32 socketCaps;
	StepDataCopy(&socketCaps, pCur, 1, false);
	
	CRY_ASSERT((pCur - pData) == messageSize);

	con.to = from;
	con.socketCaps = ntohl(socketCaps);

#if ENCRYPTION_GENERATE_KEYS
	if (con.tokenMode == CNetProfileTokens::EMode::Legacy)
	{
		con.exchange.Generate(con.B, con.g, con.p, con.A);

		const CExponentialKeyExchange::KEY_TYPE& cKey = con.exchange.GetKey();
		con.profileTokenPair.token.inputEncryption.secret.resize(CExponentialKeyExchange::KEY_SIZE);
		con.profileTokenPair.token.inputEncryption.secret.copy(0, cKey.key, CExponentialKeyExchange::KEY_SIZE);
		con.profileTokenPair.token.outputEncryption.secret = con.profileTokenPair.token.inputEncryption.secret;

		// Causing slightly different key, can be improved by properly re-deriving key
		con.profileTokenPair.token.outputEncryption.secret[0] += 1;
	}
	else
	{
		NetWarning("Unable to use exchanged keys: wrong token mode");
	}
#endif

	ChannelSecurity::CHMac channelHmac = con.hmac;
#if ENCRYPTION_GENERATE_KEYS
	if (con.tokenMode == CNetProfileTokens::EMode::Legacy)
	{
		// Reinitialize dummy hmac with the same secret
		// We have to leave con.hmac as dummy hmac as we may continue to call SendKeyExchange1().
		con.profileTokenPair.token.hmacSecret = con.profileTokenPair.token.inputEncryption.secret;
		channelHmac.Init(con.profileTokenPair.token);
	}
#endif

	bool securityInitRes;
	ChannelSecurity::SInitState securityInit(con.profileTokenPair.token, channelHmac, *&securityInitRes);
	if (!securityInitRes)
	{
		return;
	}

	if (!PrepareConnectionString(con, securityInit.outputCipher))
	{
		NetWarning("Failed encrypt connection string");
		return;
	}

	if (!SendKeyExchange1(con))
	{
		NetWarning("failed sending KeyExchange1");
		return;
	}

	con.kes = eKES_KeyEstablished;
	con.kesStart = g_time;
	
	CreateChannel(con.to, std::move(securityInit), "" /* NULL */, con.socketCaps, con.profileTokenPair.profile, con.session, con.conlk);   // the empty string ("") will be translated to NULL when creating GameChannel
}

bool CNetNub::PrepareConnectionString(SPendingConnection& con, ChannelSecurity::CCipher& outputCipher)
{
	const size_t blockSize = ChannelSecurity::CCipher::GetBlockSize();
	NET_ASSERT(IsPowerOfTwo(blockSize));
	const size_t cslen = con.connectionString.size();
	const size_t connStringMessageSize = 4 + cslen;
	size_t connectionStringBufSize = Align(connStringMessageSize, blockSize);

	con.connectionStringBuf.resize(connectionStringBufSize);
	uint8* pCur = con.connectionStringBuf.data();
	uint32 stringSize = htonl(cslen);
	StepDataWrite(pCur, stringSize, false);
	StepDataWrite(pCur, con.connectionString.data(), cslen, false);
	NET_ASSERT((pCur - con.connectionStringBuf.data()) == connStringMessageSize);
	::memset(pCur, 0, connectionStringBufSize - connStringMessageSize);

	return outputCipher.EncryptInPlace(con.connectionStringBuf.data(), connectionStringBufSize);
}

void CNetNub::ProcessKeyExchange1(const TNetAddress& from, const uint8* pData, uint32 nSize)
{
	if (GetChannelForIP(from, 0) != NULL)
		return;

	TConnectingMap::iterator iterCon = m_connectingMap.find(from);
	if (iterCon == m_connectingMap.end())
	{
		AddSilentDisconnectEntry(from, CrySessionInvalidHandle, eDC_CantConnect, "KeyExchange1 with no connection");
		return; // ignore silently
	}

	SConnecting& con = iterCon->second;

	if (con.kes != eKES_SentKeyExchange)
		return;

	static const size_t KS = sizeof(CExponentialKeyExchange::KEY_TYPE);
	const size_t minMessageSize = 1
#if ENCRYPTION_GENERATE_KEYS
		+ KS
#endif
		+ 0                           // variable size connection string
		;
	const size_t minBufferSize = minMessageSize + ChannelSecurity::CHMac::HashSize;

	if (nSize < minBufferSize)
	{
		NetWarning("processing KeyExchange1 with illegal sized packet");
		AddSilentDisconnectEntry(from, con.session, eDC_CantConnect, "processing KeyExchange1 with illegal sized packet");
		return;
	}

	const size_t messageSize = nSize - ChannelSecurity::CHMac::HashSize;

	if (!con.hmac.HashFinishAndVerify(pData, messageSize, pData + messageSize))
	{
		return;
	}

	const uint8* pCur = pData;
	pCur += 1;
#if ENCRYPTION_GENERATE_KEYS
	StepDataCopy(con.B.key, pCur, KS, false);

	con.exchange.Calculate(con.B);

	if (con.tokenMode == CNetProfileTokens::EMode::Legacy)
	{
		const CExponentialKeyExchange::KEY_TYPE& cKey = con.exchange.GetKey();
		con.profileTokenPair.token.inputEncryption.secret.resize(CExponentialKeyExchange::KEY_SIZE);
		con.profileTokenPair.token.inputEncryption.secret.copy(0, cKey.key, CExponentialKeyExchange::KEY_SIZE);
		con.profileTokenPair.token.outputEncryption.secret = con.profileTokenPair.token.inputEncryption.secret;

		// Reinitialize dummy hmac with the same secret
		con.profileTokenPair.token.hmacSecret = con.profileTokenPair.token.inputEncryption.secret;
		con.hmac.Init(con.profileTokenPair.token);

		// Causing slightly different key, can be improved by properly re-deriving key
		con.profileTokenPair.token.inputEncryption.secret[0] += 1;
	}
	else
	{
		NetWarning("Unable to use exchanged keys: wrong token mode");
	}
#endif

	bool securityInitRes;
	ChannelSecurity::SInitState securityInit(con.profileTokenPair.token, con.hmac, *&securityInitRes);
	if (!securityInitRes)
	{
		AddDisconnectEntry(from, con.session, con.hmac, eDC_CantConnect, "processing KeyExchange1, failed security init");
		return;
	}

	NET_ASSERT(minMessageSize == (pCur - pData));

	const size_t connectionStringBufSize = messageSize - minMessageSize;

	if (connectionStringBufSize < 4)
	{
		NetWarning("processing KeyExchange1 with illegal sized connection string part");
		AddDisconnectEntry(from, con.session, con.hmac, eDC_CantConnect, "processing KeyExchange1 with illegal sized packet");
		return;
	}

	const size_t blockSize = ChannelSecurity::CCipher::GetBlockSize();
	NET_ASSERT(IsPowerOfTwo(blockSize));
	if ((connectionStringBufSize) & (blockSize - 1))
	{
		NetWarning("Illegal packet block alignment [message size was %zu, from %s]", messageSize, RESOLVER.ToString(from).c_str());
		AddDisconnectEntry(from, con.session, con.hmac, eDC_CantConnect, "processing KeyExchange1 with illegal sized packet");
		return;
	}

	std::unique_ptr<uint8[]> pConnectionStringBuf(new uint8[connectionStringBufSize]);
	::memcpy(pConnectionStringBuf.get(), pCur, connectionStringBufSize);
	pCur += connectionStringBufSize;
	NET_ASSERT((pCur - pData) == messageSize);

	string connectionString;
	if (!ProcessConnectionString(pConnectionStringBuf.get(), connectionStringBufSize, securityInit.inputCipher, *&connectionString))
	{
		NetWarning("Failed to process connection string [from %s]", messageSize, RESOLVER.ToString(from).c_str());
		AddDisconnectEntry(from, con.session, con.hmac, eDC_CantConnect, "processing KeyExchange1 with illegal connection string");
		return;
	}
	
	con.kes = eKES_KeyEstablished;
	con.kesStart = g_time;

	// now that we have established the private key, let's go create the channel
	
	CreateChannel(from, std::move(securityInit), connectionString, con.socketCaps, con.profileTokenPair.profile, con.session, CNubConnectingLock());
}

bool CNetNub::ProcessConnectionString(uint8* pBuf, const size_t bufSize, ChannelSecurity::CCipher& inputCipher, string& outConnectionString)
{
	if (!inputCipher.DecryptInPlace(pBuf, bufSize))
	{
		return false;
	}

	const uint8* pCur = pBuf;

	uint32 connectionStringSize;
	StepDataCopy(&connectionStringSize, pCur, 1, false);
	connectionStringSize = ntohl(connectionStringSize);

	if (connectionStringSize > (bufSize - 4))
	{
		return false;
	}

	outConnectionString = string(reinterpret_cast<const char*>(pCur), reinterpret_cast<const char*>(pCur + connectionStringSize));
	return true;
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

	const size_t messageSize = 1
		+ (VERSION_SIZE + 2) * sizeof(uint32)  // versions
		+ 1                                    // encryption flags
		+ 2 * sizeof(uint32)                   // profile/tokenId
		+ sizeof(CrySessionHandle)             // session
		;
	const size_t packetSize = messageSize + ChannelSecurity::CHMac::HashSize;
	if (nSize != packetSize)
	{
		AddSilentDisconnectEntry(from, CrySessionInvalidHandle, eDC_ProtocolError, "Too short connection packet");
		return;
	}

	uint32 buildVersion[VERSION_SIZE];
	uint32 protocolVersion;
	uint32 socketCaps;
	uint8 encryptionFlags;
	uint32 profile;
	uint32 tokenLegacy;
	CrySessionHandle session;
	const uint8* pHmacData = pData + messageSize;

	{
		const uint8* pCurData = pData;
		pCurData += 1;
		StepDataCopy(buildVersion, pCurData, VERSION_SIZE, false);
		StepDataCopy(&protocolVersion, pCurData, 1, false);
		StepDataCopy(&socketCaps, pCurData, 1, false);
		StepDataCopy(&encryptionFlags, pCurData, 1, false);
		StepDataCopy(&profile, pCurData, 1, false);
		StepDataCopy(&tokenLegacy, pCurData, 1, false);
		StepDataCopy(&session, pCurData, 1, false);
		CRY_ASSERT(messageSize == (pCurData - pData));
	}

	for (uint32& v : buildVersion) { v = ntohl(v); }
	protocolVersion = ntohl(protocolVersion);
	socketCaps = ntohl(socketCaps);
	profile = ntohl(profile);
	tokenLegacy = ntohl(tokenLegacy);
	session = ntohl(session);

	const SFileVersion ver = gEnv->pSystem->GetProductVersion();
	
	for (int i = 0; i < 4; i++)
	{
		if (buildVersion[i] != ver[i])
		{
			AddSilentDisconnectEntry(from, session, eDC_VersionMismatch, "Build version mismatch");
			m_connectingMap.erase(from);
			return;
		}
	}
	if (protocolVersion != PROTOCOL_VERSION)
	{
		AddSilentDisconnectEntry(from, session, eDC_VersionMismatch, "Protocol version mismatch");
		m_connectingMap.erase(from);
		return;
	}
	if (encryptionFlags != EncryptionVersionFlags::GetFlags())
	{
		AddSilentDisconnectEntry(from, session, eDC_VersionMismatch, "Encryption version mismatch");
		m_connectingMap.erase(from);
		return;
	}

	const bool isLocal = stl::get_if<TLocalNetAddress>(&from) != nullptr;

	CNetProfileTokens::EMode tokenMode = CNetProfileTokens::EMode::Legacy;
	CNetProfileTokens::SProfileTokenPair profileToken;
	ChannelSecurity::CHMac::FillDummyToken(profileToken.token);
	if (!isLocal)
	{
		INetworkServicePtr pNetTokens = CNetwork::Get()->GetService("NetProfileTokens");
		if (pNetTokens && (eNSS_Ok == pNetTokens->GetState()))
		{
			CNetProfileTokens* pNetProfileTokens = static_cast<CNetProfileTokens*>(pNetTokens->GetNetProfileTokens());
			tokenMode = pNetProfileTokens->GetMode();
			switch (tokenMode)
			{
			case CNetProfileTokens::EMode::Legacy:
				if (!pNetProfileTokens->IsValid(profile, tokenLegacy))
				{
					AddSilentDisconnectEntry(from, session, eDC_AuthenticationFailed, "Client token id mismatch");
					m_connectingMap.erase(from);
					return;
				}
				break;

			case CNetProfileTokens::EMode::ProfileTokens:
				if (!pNetProfileTokens->Server_FindToken(profile, *&profileToken))
				{
					AddSilentDisconnectEntry(from, session, eDC_AuthenticationFailed, "Client token id mismatch");
					m_connectingMap.erase(from);
					return;
				}
				break;
			}
		}
	}

	ChannelSecurity::CHMac hmac;
	if (!hmac.Init(profileToken.token))
	{
		return;
	}
	if (!hmac.HashFinishAndVerify(pData, messageSize, pHmacData))
	{
		return;
	}

	bool doNotRegenerate = false;
	TConnectingMap::iterator iterCon = m_connectingMap.find(from);
	if (iterCon != m_connectingMap.end())
	{
		switch (iterCon->second.kes)
		{
		case eKES_NotStarted:
		case eKES_SetupInitiated:
			AddDisconnectEntry(from, session, iterCon->second.hmac, eDC_ProtocolError, "Key establishment handshake ordering error");
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


	iterCon->second.hmac = std::move(hmac);

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
			AddDisconnectEntry(from, session, iterCon->second.hmac, eDC_ProtocolError, "Invalid session");
		}
	}

	iterCon->second.kes = eKES_SentKeyExchange;
	iterCon->second.socketCaps = socketCaps;
	iterCon->second.session = session;
	iterCon->second.tokenMode = tokenMode;
	iterCon->second.profileTokenPair = profileToken;
	
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

void CNetNub::CreateChannel(const TNetAddress& ip, ChannelSecurity::SInitState&& securityInit, const string& connectionString, uint32 remoteSocketCaps, uint32 proifle, CrySessionHandle session, CNubConnectingLock conlk)
{
	// this function is executed on the network thread
	CNetChannel* pNetChannel = new CNetChannel(ip, std::move(securityInit), remoteSocketCaps, session);
	pNetChannel->SetProfileId(proifle);

	TO_GAME(&CNetNub::GC_CreateChannel, this, pNetChannel, connectionString, conlk);
}

void CNetNub::GC_CreateChannel(CNetChannel* pNetChannel, string connectionString, CNubConnectingLock conlk)
{
	// this function is executed on the game thread with the global lock held
	// see CSystem::Update -> CNetwork::SyncWithGame
	//SCOPED_GLOBAL_LOCK; - no need to lock
	ASSERT_GLOBAL_LOCK;

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
			AddDisconnectEntry(pNetChannel->GetIP(), pNetChannel->GetSession(), pNetChannel->GetHmac(), eDC_CantConnect, "Pending connection not found");
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

			AddDisconnectEntry(pNetChannel->GetIP(), pNetChannel->GetSession(), pNetChannel->GetHmac(), eDC_CantConnect, "Address not found in MatchMaking");
			delete pNetChannel;

			return;
		}
	}

	SCreateChannelResult res = m_pGameNub->CreateChannel(pNetChannel, connectionString.empty() ? NULL : connectionString.c_str());
	if (!res.pChannel)
	{
		if (fromRequest)
			m_connectingMap.erase(iterCon->first);
		AddDisconnectEntry(pNetChannel->GetIP(), pNetChannel->GetSession(), pNetChannel->GetHmac(), res.cause, res.errorMsg);
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

void CNetNub::AddDisconnectEntry(const TNetAddress& ip, CrySessionHandle session, const ChannelSecurity::CHMac& hmac, EDisconnectionCause cause, const char* reason)
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

		dc.hmac = hmac;

		TDisconnectMap::iterator iterDis = m_disconnectMap.insert(std::make_pair(ip, dc)).first;

		if (cause != eDC_ICMPError) // If we are here because of eDC_ICMPError from a send then the send here will bring us here again and cause a stack overflow.
		{
			SendDisconnect(ip, iterDis->second);
		}
	}
}

void CNetNub::AddSilentDisconnectEntry(const TNetAddress& ip, CrySessionHandle session, EDisconnectionCause cause, const char* szReason)
{
	// Notify legacy lobby to keep functionality
	if (m_pLobby && m_pLobby->GetMatchMaking())
	{
		CryLog("CNetNub::AddSilentDisconnectEntry calling SessionDisconnectRemoteConnection reason %s", szReason ? szReason : "no reason");
		m_pLobby->GetMatchMakingPrivate()->SessionDisconnectRemoteConnectionFromNub(session, cause);
	}
	else
	{
		NetLog("Silent disconnect, cause %u reason: %s", cause, szReason);
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


CNetNub::TPendingConnectionSet::iterator CNetNub::FindPendingConnection(const TNetAddress& queryAddr, const bool checkAllTos)
{
	TPendingConnectionSet::iterator iter = std::find_if(m_pendingConnections.begin(), m_pendingConnections.end(),
		[&](const SPendingConnection& pc)
		{
			if (pc.to == queryAddr)
			{
				return true;
			}

			if (checkAllTos)
			{
				TNetAddressVec::const_iterator it = std::find(pc.tos.begin(), pc.tos.end(), queryAddr);
				if (it != pc.tos.end())
				{
					return true;
				}
			}
			return false;
		});
	return iter;
}