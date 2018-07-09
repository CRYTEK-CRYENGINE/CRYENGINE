// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  implements communications between two machines, with
               context setup
   -------------------------------------------------------------------------
   History:
   - 26/07/2004   : Created by Craig Tiller
*************************************************************************/
#include "StdAfx.h"
#include "NetChannel.h"
#include <CrySystem/ITimer.h>
#include "Context/ClientContextView.h"
#include "Context/ServerContextView.h"
#include <CryString/StringUtils.h>
#include "FixedSizeArena.h"
#include "Context/SyncedFileSet.h"

// must be greater than zero; scott chose this one (it's his birthday)
TNetChannelID CNetChannel::m_nextLocalChannelID = 989;

int g_nChannels = 0;

static const float STATS_UPDATE_INTERVAL = 0.1f;
static const CTimeValue KEEPALIVE_REPLY_TIMEOUT = 0.7f;

CNetChannel::CNetChannel(const TNetAddress& ipRemote, const CExponentialKeyExchange::KEY_TYPE& key, uint32 remoteSocketCaps, CrySessionHandle session) :
	m_privateKey(key),
	m_ip(ipRemote),
	m_nKeepAliveTimer(0.0f),
	m_nKeepAliveTimer2(0.0f),
	m_nKeepAlivePingTimer(0.0f),
	m_nKeepAliveReplyTimer(0.0f),
	m_lastVoiceTransmission(0.0f),
	m_lastPingSent(0.0f),
	m_remoteTime(0.0f),
	m_timeSinceRecv(0.0f),
#if USE_CHANNEL_TIMERS
	m_timer(0),
#endif // USE_CHANNEL_TIMERS
	m_statsTimer(0),
	m_pMMM(new CMementoMemoryManager(string().Format("NetChannel[%s]", ((CNetwork*)gEnv->pNetwork)->GetResolver()->ToNumericString(ipRemote).c_str()))),
	m_pGameChannel(NULL),
	m_pContextView(NULL),
	m_pNub(NULL),
#if ENABLE_DEBUG_KIT
	m_pDebugHistory(0),
	m_pElapsedRemote(0),
	m_pSinceSent(0),
	m_pPing(0),
#endif
	m_ctpEndpoint(m_pMMM),
	m_localChannelID(m_nextLocalChannelID++),
	m_remoteChannelID(0),
	m_session(session),
	m_nStrongCaptureRMIs(0),
	m_remoteSocketCaps(remoteSocketCaps),
	m_profileId(0),
	m_pendingStuffBeforeDead(0),
	m_fastLookupId(-1),
	m_channelMask(ChannelMaskType(1)),    // By default a channel only sets bit 0, this frees the other bits for use with masking
	m_bDead(false),
	m_pingLock(false),
	m_isSufferingHighLatency(false),
	m_preordered(false),
	m_bConnectionEstablished(false),
	m_bForcePacketSend(true),
	m_sentDisconnect(false),
	m_gotFakePacket(false),
	m_isLocal((stl::get_if<TLocalNetAddress>(&m_ip)) ? true : false),
	m_gameHasRequestedUpdate(false),
	m_bIsMigratingChannel(false)
#if ENABLE_URGENT_RMIS
	, m_writeUrgentMessages(false)
	, m_haveUrgentMessages(false)
#endif // ENABLE_URGENT_RMIS
{
	SCOPED_GLOBAL_LOCK;
	++g_nChannels;
	++g_objcnt.channel;
}

CNetChannel::~CNetChannel()
{
	SCOPED_GLOBAL_LOCK;
	MMM_REGION(m_pMMM);

	--g_nChannels;
	--g_objcnt.channel;

	if (m_pContextView)
		m_pContextView->OnChannelDestroyed();

	m_pContextView = 0;

	while (!m_queuedForSyncPackets.empty())
	{
		MMM().FreeHdl(m_queuedForSyncPackets.front());
		m_queuedForSyncPackets.pop();
	}

#if USE_CHANNEL_TIMERS
	TIMER.CancelTimer(m_timer);
#endif // USE_CHANNEL_TIMERS
	TIMER.CancelTimer(m_statsTimer);

#if ENABLE_DEBUG_KIT
	if (m_pDebugHistory)
	{
		ASSERT_PRIMARY_THREAD;
		m_pDebugHistory->Release();
	}
#endif

	m_ctpEndpoint.ChangeSubscription(this, 0);

	if (m_fastLookupId >= 0)
		CNetwork::Get()->UnregisterFastLookup(m_fastLookupId);
}

void CNetChannel::SetClient(INetContext* pNetContext)
{
	SCOPED_GLOBAL_LOCK;
	CRY_ASSERT_MESSAGE(!m_pContextView, "Error: setting the channel type twice.");
	CRY_ASSERT_MESSAGE(pNetContext, "Must set a context to make a client.");
	m_pContextView = new CClientContextView(this, (CNetContext*)pNetContext);
}

void CNetChannel::SetServer(INetContext* pNetContext)
{
	SCOPED_GLOBAL_LOCK;
	CRY_ASSERT_MESSAGE(!m_pContextView, "Error: setting the channel type twice.");
	CRY_ASSERT_MESSAGE(pNetContext, "Must set a context to make a server.");
	m_pContextView = new CServerContextView(this, (CNetContext*)pNetContext);
	m_fastLookupId = CNetwork::Get()->RegisterForFastLookup(this);
}

void CNetChannel::PerformRegularCleanup()
{
	m_ctpEndpoint.PerformRegularCleanup();
	if (m_pContextView)
		m_pContextView->PerformRegularCleanup();
}

string CNetChannel::GetRemoteAddressString() const
{
	return RESOLVER.ToString(m_ip);
}

bool CNetChannel::GetRemoteNetAddress(uint32& uip, uint16& port, bool firstLocal)
{
	TNetAddress ip = m_ip;
	if (IsLocal())
	{
		TNetAddressVec addrv;
		GetLocalIPs(addrv);
		for (uint32 i = 0; i < addrv.size(); i++)
			if (stl::get_if<SIPv4Addr>(&addrv[i]))
			{
				ip = addrv[i];
				if (firstLocal)
					break;
			}
	}

	CRYSOCKADDR_IN addr;
	if (const SIPv4Addr* adr = stl::get_if<SIPv4Addr>(&ip))
	{
		ConvertAddr(TNetAddress(*adr), &addr);
		uip = S_ADDR_IP4(addr);
		port = addr.sin_port;
		return true;
	}
	else
		return false;
}

void CNetChannel::SetPerformanceMetrics(SPerformanceMetrics* pMetrics)
{
	SCOPED_GLOBAL_LOCK;
	m_ctpEndpoint.SetPerformanceMetrics(pMetrics);
}

#if NEW_BANDWIDTH_MANAGEMENT
const INetChannel::SPerformanceMetrics* CNetChannel::GetPerformanceMetrics() const
{
	// Only called on network thread so don't need the lock
	return m_ctpEndpoint.GetPerformanceMetrics();
}
#endif // NEW_BANDWIDTH_MANAGEMENT

void CNetChannel::GetBandwidthStatistics(uint32 channelIndex, SBandwidthStats* const pStats)
{
	SNetChannelStats& stats = pStats->m_channel[channelIndex];
	stats.m_inUse = true;

	const char* nickName = GetNickname();
	if (nickName == NULL)
	{
		nickName = "<no name>";
	}

	char type = 'U';
	if (m_pContextView)
	{
		if (m_pContextView->IsClient())
		{
			type = 'C';
		}
		if (m_pContextView->IsServer())
		{
			type = 'S';
		}
	}

	CryFixedStringT<STATS_MAX_NAME_SIZE> name;
	name.Format("%s [%c]", nickName, type);
	cry_strcpy(stats.m_name, name.c_str());

	m_ctpEndpoint.GetBandwidthStatistics(channelIndex, pStats);
}

const char* CNetChannel::GetName()
{
	TAddressString temp = RESOLVER.ToString(m_ip);

	static char buffer[256];
	if (temp.length() > 255)
		temp = temp.substr(0, 255);

	memcpy(buffer, temp.c_str(), temp.length() + 1);

	return buffer;
}

const char* CNetChannel::GetNickname()
{
	return m_nickname.empty() ? NULL : m_nickname.c_str();
}

EContextViewState CNetChannel::GetContextViewState() const
{
	if (m_pContextView)
	{
		return m_pContextView->GetLocalState();
	}

	return eCVS_Initial;
}

void CNetChannel::Disconnect(EDisconnectionCause cause, const char* fmt, ...)
{
	SCOPED_GLOBAL_LOCK;

	char temp[256];
	va_list args;
	va_start(args, fmt);
	cry_vsprintf(temp, fmt, args);
	va_end(args);

#if ENABLE_CORRUPT_PACKET_DUMP
	if (gEnv->bMultiplayer)
	{
		// only want packet dumps in multiplayer
		m_ctpEndpoint.DoPacketDump();
	}
#endif

	if (m_pNub)
		m_pNub->DisconnectChannel(cause, NULL, this, temp);
}

void CNetChannel::SendMsg(INetMessage* pMsg)
{
	if (pMsg->CheckParallelFlag(eMPF_NoSendDelay))
	{
		SCOPED_GLOBAL_LOCK;
		NC_SendMsg(pMsg);
	}
	else
	{
		FROM_GAME(&CNetChannel::NC_SendMsg, this, pMsg);
	}
}

void CNetChannel::NC_SendMsg(INetMessage* pMsg)
{
	NetAddSendable(pMsg, 0, NULL, NULL);
}

bool CNetChannel::AddSendable(INetSendablePtr pSendable, int numAfterHandle, const SSendableHandle* afterHandle, SSendableHandle* handle)
{
	SCOPED_GLOBAL_LOCK;
	return NetAddSendable(pSendable, numAfterHandle, afterHandle, handle);
}

bool CNetChannel::SubstituteSendable(INetSendablePtr pSendable, int numAfterHandle, const SSendableHandle* afterHandle, SSendableHandle* handle)
{
	SCOPED_GLOBAL_LOCK;
	return NetSubstituteSendable(pSendable, numAfterHandle, afterHandle, handle);
}

bool CNetChannel::RemoveSendable(SSendableHandle handle)
{
	SCOPED_GLOBAL_LOCK;
	return NetRemoveSendable(handle);
}

void CNetChannel::DispatchRMI(IRMIMessageBodyPtr pBody)
{
	NET_PROFILE_SCOPE_RMI(pBody->pMessageDef ? pBody->pMessageDef->description : "RMI:Unknown", false);

#if NET_PROFILE_ENABLE
	#if ENABLE_SERIALIZATION_LOGGING
	CNetSizingSerializeImpl out(this);
	if (!IsLocal())
	{
		NetLog("[DispatchRMI] [%s] BEGIN [%s]", GetName(), pBody->pMessageDef ? pBody->pMessageDef->description : "RMI:Unknown");
	}
	#else
	CNetSizingSerializeImpl out;
	#endif // ENABLE_SERIALIZATION_LOGGING
	CSimpleSerialize<CNetSizingSerializeImpl> outSimple(out);
	pBody->SerializeWith(TSerialize(&outSimple));
	#if ENABLE_SERIALIZATION_LOGGING
	if (!IsLocal())
	{
		NetLog("[DispatchRMI] [%s] END   [%s] bit count %d", GetName(), pBody->pMessageDef ? pBody->pMessageDef->description : "RMI:Unknown", out.GetBitSize());
	}
	#endif // ENABLE_SERIALIZATION_LOGGING
	NET_PROFILE_ADD_WRITE_BITS(out.GetBitSize());
#endif

#if ENABLE_RMI_BENCHMARK

	const SRMIBenchmarkParams* pBenchmarkParams = pBody->GetRMIBenchmarkParams();

	if (pBenchmarkParams)
	{
		LogRMIBenchmark(eRMIBA_Queue, *pBenchmarkParams, NULL, NULL);
	}

#endif

	if (m_nStrongCaptureRMIs)
	{
		ASSERT_GLOBAL_LOCK;
		NC_DispatchRMI(pBody);
	}
	else if (pBody->pMessageDef && pBody->pMessageDef->CheckParallelFlag(eMPF_NoSendDelay))
	{
		SCOPED_GLOBAL_LOCK;
		if (!DoDispatchRMI(pBody))
		{
			FROM_GAME(&CNetChannel::NC_DispatchRMI, this, pBody);
		}
		else
			ForcePacketSend();
	}
	else
	{
		FROM_GAME(&CNetChannel::NC_DispatchRMI, this, pBody);
	}
}

void CNetChannel::NC_DispatchRMI(IRMIMessageBodyPtr pBody)
{
	if (!DoDispatchRMI(pBody) && CVARS.LogLevel > 1)
		NetWarning("RMI message discarded (%s)", pBody->pMessageDef ? pBody->pMessageDef->description : "<<unknown>>");
}

bool CNetChannel::DoDispatchRMI(IRMIMessageBodyPtr pBody)
{
	NET_ASSERT(pBody != NULL);

	if (!m_pContextView)
	{
#if ENABLE_SERIALIZATION_LOGGING
		if (!IsLocal())
		{
			NetLog("[DispatchRMI] [%s] RMI [%s] with no context; ignored", GetName(), pBody->pMessageDef ? pBody->pMessageDef->description : "RMI:Unknown");
		}
#endif // ENABLE_SERIALIZATION_LOGGING
		NetWarning("RMI message with no context; ignored");
		return false;
	}

#if ENABLE_URGENT_RMIS
	if (pBody->attachment == eRAT_Urgent)
	{
		// This RMI has been flagged as urgent
		m_haveUrgentMessages = true;
	}
#endif // ENABLE_URGENT_RMIS

	bool dispatched = m_pContextView->ScheduleAttachment(true, pBody);
#if ENABLE_SERIALIZATION_LOGGING
	if ((!dispatched) && (!IsLocal()))
	{
		NetLog("[DispatchRMI] [%s] RMI [%s] was not scheduled", GetName(), pBody->pMessageDef ? pBody->pMessageDef->description : "RMI:Unknown");
	}
#endif // ENABLE_SERIALIZATION_LOGGING
	return dispatched;
}

void CNetChannel::DeclareWitness(EntityId id)
{
	FROM_GAME(&CNetChannel::NC_DeclareWitness, this, id);
}

void CNetChannel::NC_DeclareWitness(EntityId id)
{
	if (!m_pContextView)
	{
		NetWarning("DeclareWitness %.8x without a context-view... ignored (hint: SetServer or SetClient", id);
		return;
	}

	m_pContextView->DeclareWitness(id);
}

const INetChannel::SStatistics& CNetChannel::GetStatistics()
{
	SCOPED_GLOBAL_LOCK;
	return m_statistics;
}

void CNetChannel::SetPassword(const char* password)
{
	SCOPED_GLOBAL_LOCK;
	if (m_pContextView)
		m_pContextView->SetPassword(password);
}

#if !NEW_BANDWIDTH_MANAGEMENT
void CNetChannel::FragmentedPacket()
{
	CTimeValue now = g_time;
	return m_ctpEndpoint.FragmentedPacket(now);
}
#endif // !NEW_BANDWIDTH_MANAGEMENT

CTimeValue CNetChannel::GetRemoteTime() const
{
	return m_remoteTime;
}

float CNetChannel::GetPing(bool smoothed) const
{
	return m_ctpEndpoint.GetPing(smoothed);
}

bool CNetChannel::IsSufferingHighLatency(CTimeValue nTime) const
{
	return m_isSufferingHighLatency;
}

CTimeValue CNetChannel::GetTimeSinceRecv() const
{
	return m_timeSinceRecv;
}

void CNetChannel::DefineProtocol(IProtocolBuilder* pBuilder)
{
	// we send and receive the same protocol here!
	pBuilder->AddMessageSink(this, GetProtocolDef(), GetProtocolDef());

	if (m_pGameChannel)
		m_pGameChannel->DefineProtocol(pBuilder);
	if (m_pContextView)
		m_pContextView->DefineProtocol(pBuilder);
}

static CFixedSizeArena<sizeof(void*)*8 + sizeof(CTimeValue)*2, 64> m_pingpongarena;

class CNetChannel::CPingMsg : public INetSendable
{
public:
	CPingMsg() : INetSendable(eMPF_DontAwake, eNRT_UnreliableUnordered)
	{
		m_pChannel = 0;
		SetGroup('ping');
	}

	void Reset(CNetChannel* pChannel)
	{
		m_pChannel = pChannel;
	}

	virtual size_t      GetSize() { return sizeof(*this); }
#if ENABLE_PACKET_PREDICTION
	virtual SMessageTag GetMessageTag(INetSender* pSender, IMessageMapper* mapper)
	{
		SMessageTag mTag;
		mTag.messageId = mapper->GetMsgId(CNetChannel::Ping);
		mTag.varying1 = 0;
		mTag.varying2 = 0;
		return mTag;
	}
#endif

	virtual EMessageSendResult Send(INetSender* pSender)
	{
		CTimeValue currentTime = gEnv->pTimer->GetAsyncTime();
		m_pChannel->m_pingLock = false;
		m_pChannel->m_lastPingSent = currentTime;
		m_pChannel->m_pings.push(currentTime);
		pSender->BeginMessage(CNetChannel::Ping);
		pSender->ser.Value("when", currentTime, 'ping');
		return eMSR_SentOk;
	}
	virtual void        UpdateState(uint32 nFromSeq, ENetSendableStateUpdate update) {}
	virtual const char* GetDescription()                                             { return "Ping"; }
	virtual void        GetPositionInfo(SMessagePositionInfo& pos)                   {}

private:
	CNetChannel* m_pChannel;

	virtual void DeleteThis()
	{
		m_pingpongarena.Dispose(this);
	}
};

class CNetChannel::CPongMsg : public INetSendable
{
public:
	CPongMsg() : INetSendable(0, eNRT_UnreliableUnordered), m_pChannel(nullptr)
	{
		SetGroup('pong');
	}

	void Reset(CNetChannel* pChannel, CTimeValue when, CTimeValue recvd)
	{
		m_pChannel = pChannel;
		m_when = when;
		m_recvd = recvd;
	}

	virtual size_t GetSize() { return sizeof(*this); }

#if ENABLE_PACKET_PREDICTION
	virtual SMessageTag GetMessageTag(INetSender* pSender, IMessageMapper* mapper)
	{
		SMessageTag mTag;
		mTag.messageId = mapper->GetMsgId(CNetChannel::Pong);
		mTag.varying1 = 0;
		mTag.varying2 = 0;
		return mTag;
	}
#endif

	virtual EMessageSendResult Send(INetSender* pSender)
	{
		CTimeValue currentTime = gEnv->pTimer->GetAsyncTime();
		pSender->BeginMessage(CNetChannel::Pong);
		pSender->ser.Value("when", m_when, 'pong');
		CTimeValue elapsed = currentTime - m_recvd;
		pSender->ser.Value("elapsed", elapsed, 'pelp');
		CTimeValue gamenow = CNetwork::Get()->GetGameTime();
		pSender->ser.Value("remote", gamenow, 'trem');
		return eMSR_SentOk;
	}
	virtual void        UpdateState(uint32 nFromSeq, ENetSendableStateUpdate update) {}
	virtual const char* GetDescription()                                             { return "Pong"; }
	virtual void        GetPositionInfo(SMessagePositionInfo& pos)                   {}

private:
	CNetChannel* m_pChannel;
	CTimeValue   m_when;
	CTimeValue   m_recvd;

	virtual void DeleteThis()
	{
		m_pingpongarena.Dispose(this);
	}
};

void CNetChannel::TimerCallback(NetTimerId id, void* pUser, CTimeValue time)
{
	CNetChannel* pThis = static_cast<CNetChannel*>(pUser);

#if USE_CHANNEL_TIMERS
	if (pThis->m_timer == id)
	{
	#define MAX_TIME_SINCE_UPDATE (0.2f)
		CTimeValue crv = (time - pThis->m_lastUpdateTime);
		if (pThis->HasGameRequestedUpdate() || pThis->IsLocal() || (crv.GetSeconds() > MAX_TIME_SINCE_UPDATE))
		{
			pThis->CallUpdate(g_time);
		}

		pThis->m_timer = 0; // expired this one...
		//#if CHANNEL_TIED_TO_NETWORK_TICK
		//if (gEnv->bMultiplayer == false)
		//#endif
		{
			pThis->UpdateTimer(time, true);
		}
	}
	else
#endif // USE_CHANNEL_TIMERS
	if (pThis->m_statsTimer == id)
	{
		pThis->UpdateStats(g_time);
		if (!pThis->IsDead())
		{
#if TIMER_DEBUG
			char buffer[128];
			cry_sprintf(buffer, "CNetChannel::TimerCallback() m_statsTimer [%s][%p]", pThis->GetNickname(), pThis);
#else
			char* buffer = NULL;
#endif // TIMER_DEBUG

			pThis->m_statsTimer = TIMER.ADDTIMER(g_time + STATS_UPDATE_INTERVAL, TimerCallback, pThis, buffer);
		}
	}
}

void CNetChannel::CallUpdate(CTimeValue time)
{
	// opportunistically try and send a ping (piggybacking on whatever update this is)
	// IsTimeReady() doesn't become true until 3 ping pongs have happened and after this pings are less important
	// So send pings more often until IsTimeReady() becomes true
#define TIME_NOT_READY_PING_TIMER 0.1f
	bool timeReady = IsTimeReady();
	float timeSinceLastPing = (g_time - m_lastPingSent).GetSeconds();

	if (!m_pingLock &&
	    ((timeReady && (timeSinceLastPing > CNetCVars::Get().PingTime)) ||
	     (!timeReady && (timeSinceLastPing > TIME_NOT_READY_PING_TIMER))))
	{
		if (CPingMsg* pMsg = m_pingpongarena.Construct<CPingMsg>())
		{
			pMsg->Reset(this);
			NetAddSendable(pMsg, 0, 0, 0);
			m_pingLock = true;
		}
	}

	if (!IsLocal())
	{
		m_timeSinceRecv += time - m_lastUpdateTime;
	}

	m_lastUpdateTime = time;
	Update(false, time);
}

#if !USE_CHANNEL_TIMERS
void CNetChannel::CallUpdateIfNecessary(CTimeValue time, bool force)
{
	bool needToUpdate = force | IsLocal();

	CTimeValue frameTime(g_time - m_lastSyncTime);
	// Subtract game frame time
	//NetLog("[CH ACC]: subtracting %0.3fs from accumulator (%0.3fs->%0.3fs) for channel %s", frameTime.GetSeconds(), m_channelUpdateAccumulator.GetSeconds(), (m_channelUpdateAccumulator-frameTime).GetSeconds(), GetName());
	m_channelUpdateAccumulator -= frameTime;

	if (!IsLocal())
	{
		m_lastSyncTime = g_time;
		needToUpdate |= (m_channelUpdateAccumulator <= 0.0f);
	}

	if (needToUpdate)
	{
		// Accumulate the desired send interval
		CTimeValue interval = (1.0f / static_cast<float>(m_ctpEndpoint.GetPerformanceMetrics()->m_packetRate));
		m_channelUpdateAccumulator += interval;
		//NetLog("[CH ACC]: adding %0.3fs to accumulator (%0.3fs) for channel %s", interval.GetSeconds(), m_channelUpdateAccumulator.GetSeconds(), GetName());
		if (m_channelUpdateAccumulator.GetSeconds() < 0.0f)
		{
			// Prevent accumulator triggering multiple times (e.g. when accumulating large negative values during loading)
			m_channelUpdateAccumulator.SetValue(0);
		}

		CallUpdate(time);
	}
	#if ENABLE_URGENT_RMIS
	else if (m_haveUrgentMessages)
	{
		// If we *don't* already need to update, then this is just an urgent message flush
		// and we don't need to add time to the channel update accumulator.
		m_writeUrgentMessages = true;
		CallUpdate(time);
		m_writeUrgentMessages = false;
	}
	#endif // ENABLE_URGENT_RMIS

	m_haveUrgentMessages = false;
}
#endif // !USE_CHANNEL_TIMERS

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CNetChannel, Ping, eNRT_UnreliableUnordered, 0)
{
	CTimeValue when;
	ser.Value("when", when, 'ping');
	if (CPongMsg* pMsg = m_pingpongarena.Construct<CPongMsg>())
	{
		CTimeValue currentTime = gEnv->pTimer->GetAsyncTime();
		pMsg->Reset(this, when, currentTime);
		NetSubstituteSendable(pMsg, 0, 0, &m_pongHdl);
	}
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CNetChannel, Pong, eNRT_UnreliableUnordered, 0)
{
	CTimeValue when, elapsed, remote;
	CTimeValue currentTime = gEnv->pTimer->GetAsyncTime();
	ser.Value("when", when, 'pong');
	ser.Value("elapsed", elapsed, 'pelp');
	ser.Value("remote", remote, 'trem');
	bool found = false;
	while (!found && !m_pings.empty())
	{
		float millis = (m_pings.top() - when).GetMilliSeconds();
		if (millis > 1.0f)
			break;
		else if (millis > -1.0f)
		{
			when = m_pings.top();
			found = true;
		}
		m_pings.pop();
	}
	if (found)
	{
		CTimeValue ping = currentTime - when - elapsed;
		if (ping > 0.0f)
			m_ctpEndpoint.AddPingSample(currentTime, ping, remote);
	}
	return true;
}

CTimeValue CNetChannel::GetInactivityTimeout(bool backingOff)
{
	CTimeValue inactivityTimeout = 30.0f;
	if (gEnv->pSystem->IsDevMode())
	{
		if (CNetCVars::Get().InactivityTimeoutDevmode > 0.001f)
			inactivityTimeout = CNetCVars::Get().InactivityTimeoutDevmode;
	}
	else if (CNetCVars::Get().InactivityTimeout > 0.001f)
		inactivityTimeout = CNetCVars::Get().InactivityTimeout;
	inactivityTimeout += 30.0f * !SupportsBackoff();
	inactivityTimeout += 60.0f * CNetwork::Get()->GetExternalSocketIOManager().HasPendingData();
	inactivityTimeout += 60.0f * (int)backingOff;
	return inactivityTimeout;
}

void CNetChannel::Update(bool finalUpdate, CTimeValue nTime)
{
	MMM_REGION(m_pMMM);

	if (gEnv->pLobby == NULL)
	{
		// If CryLobby is available, it will deal with timeouts and disconnects
		CTimeValue backoffTime;
		bool backingOff = m_ctpEndpoint.GetBackoffTime(backoffTime, true);
		if (CNetCVars::Get().BackoffTimeout && backingOff && backoffTime.GetSeconds() > CNetCVars::Get().BackoffTimeout)
		{
			float idleTime = backoffTime.GetSeconds();
			char buf[256];
			cry_sprintf(buf, "Zombie kicked; remote machine was backing off for %.1f seconds", idleTime);
			Disconnect(eDC_Timeout, buf);
		}

		if (m_nKeepAliveTimer != CTimeValue(0.0f) && !IsLocal())
		{
			CTimeValue idleTime = nTime - m_nKeepAliveTimer;
			if ((idleTime > GetInactivityTimeout(backingOff)))
			{
				char buf[256];
				cry_sprintf(buf, "Timeout occurred; no packet for %.1f seconds", idleTime.GetSeconds());
				Disconnect(eDC_Timeout, buf);
			}
		}
	}

	if (m_nKeepAliveTimer != CTimeValue(0.0f) && nTime > m_nKeepAlivePingTimer + CNetCVars::Get().KeepAliveTime && !IsLocal())
	{
		SendSimplePacket(eH_KeepAlive);
		m_nKeepAlivePingTimer = nTime;
	}

	if (!IsDead())
	{
		bool bAllowUserMessages = true;

		if (m_pContextView)
			bAllowUserMessages &= m_pContextView->IsInState(eCVS_InGame);
		bAllowUserMessages &= !finalUpdate;

		m_ctpEndpoint.Update(nTime, m_bDead, bAllowUserMessages, m_bForcePacketSend, finalUpdate);
		m_bForcePacketSend = false;
	}

	m_isSufferingHighLatency = m_ctpEndpoint.IsSufferingHighLatency(nTime);
	if (IsTimeReady())
	{
		m_remoteTime = m_ctpEndpoint.GetRemoteTime();
	}
}

#if USE_CHANNEL_TIMERS
void CNetChannel::UpdateTimer(CTimeValue time, bool forceUpdate)
{
	if (!IsDead())
	{
		if (forceUpdate)
		{
	#if TIMER_DEBUG
			char buffer[128];
			cry_sprintf(buffer, "CNetChannel::UpdateTimer() m_timer [%s][%p]", GetNickname(), this);
	#else
			char* buffer = NULL;
	#endif // TIMER_DEBUG
			TIMER.CancelTimer(m_timer);
			CTimeValue localSleepTime(g_time);
			CTimeValue latency(time - g_time);
	#if USE_ACCURATE_NET_TIMERS
		#if LOCK_NETWORK_FREQUENCY == 0
			const float LOCAL_MIN_SLEEP_TIME = CNetCVars::Get().channelLocalSleepTime;
			localSleepTime += LOCAL_MIN_SLEEP_TIME;
		#endif // #if LOCK_NETWORK_FREQUENCY == 0
	#endif   // USE_ACCURATE_NET_TIMERS
			CTimeValue when = IsLocal() ? localSleepTime : m_ctpEndpoint.GetNextUpdateTime(time);
			when += latency;
			if (when < g_time)
			{
				when = g_time;
			}
			m_timer = TIMER.ADDTIMER(when, TimerCallback, this, buffer);
		}
	}
	else
	{
		TIMER.CancelTimer(m_timer);
	}
}
#endif // USE_CHANNEL_TIMERS

void CNetChannel::Init(CNetNub* pNub, IGameChannel* pGameChannel)
{
	NET_ASSERT(!m_pGameChannel);
	m_pNub = pNub;
	m_pGameChannel = pGameChannel;

	if (m_pContextView)
		m_pContextView->CompleteInitialization();

	m_ctpEndpoint.Init(this);
	m_ctpEndpoint.MarkNotUserSink(this);
	m_ctpEndpoint.MarkNotUserSink(m_pContextView);

#if USE_CHANNEL_TIMERS
	//#if CHANNEL_TIED_TO_NETWORK_TICK
	//if (gEnv->bMultiplayer == false)
	//#endif
	{
		UpdateTimer(g_time, true);
	}
#endif // USE_CHANNEL_TIMERS

#if TIMER_DEBUG
	char buffer[128];
	cry_sprintf(buffer, "CNetChannel::Init() m_statsTimer [%s][%p]", GetNickname(), this);
#else
	char* buffer = NULL;
#endif // TIMER_DEBUG

	m_statsTimer = TIMER.ADDTIMER(g_time, TimerCallback, this, buffer);
	CNetChannel::SendSetRemoteChannelIDWith(SSetRemoteChannelID(m_localChannelID), this);

	if (!IsLocal())
	{
		if (SupportsBackoff())
			m_pNub->RegisterBackoffAddress(m_ip);
		else
			NetComment("Backoff system not supported to %s; timeouts are extended", RESOLVER.ToString(m_ip).c_str());
	}
}

void CNetChannel::RemovedFromNub()
{
	ASSERT_GLOBAL_LOCK;
	if (!IsLocal() && SupportsBackoff())
		m_pNub->UnregisterBackoffAddress(m_ip);
	m_pNub = 0;
}

void CNetChannel::SyncWithGame(ENetworkGameSync type)
{
#if USE_CHANNEL_TIMERS
	#if ENABLE_URGENT_RMIS
	if ((type == eNGS_FrameEnd) && (m_haveUrgentMessages))
	{
		m_writeUrgentMessages = true;
		Update(false, g_time);
		m_writeUrgentMessages = false;
		m_haveUrgentMessages = false;
	}
	#endif // ENABLE_URGENT_RMIS
#endif   // USE_CHANNEL_TIMERS

#if ENABLE_DEFERRED_RMI_QUEUE
	if (type == eNGS_FrameEnd)
	{
		m_pContextView->ProcessDeferredRMIList();
	}
#endif // ENABLE_DEFERRED_RMI_QUEUE

	if (type != eNGS_FrameStart)
		return;

	MMM_REGION(m_pMMM);

	while (!m_queuedForSyncPackets.empty())
	{
		TMemHdl hdl = m_queuedForSyncPackets.front();

		DoProcessPacket(hdl, true);
		m_queuedForSyncPackets.pop();
	}

	static ICVar* pSchedDebug = CNetCVars::Get().pSchedulerDebug;
	if (pSchedDebug)
	{
		const char* v = pSchedDebug->GetString();
		if (v[0] == '0' && v[1] == '\0')
			;
		else if (CryStringUtils::stristr(GetName(), v))
			m_ctpEndpoint.SchedulerDebugDraw();
	}

	if (CNetCVars::Get().ChannelStats)
	{
		m_ctpEndpoint.ChannelStatsDraw();
	}

	//if (IsTimeReady())
	//	NetQuickLog("%s: %f", GetName(), GetRemoteTime().GetSeconds());
}

void CNetChannel::UpdateStats(CTimeValue time)
{
	m_statistics.bandwidthDown = m_ctpEndpoint.GetBandwidth(CPacketRateCalculator::eIO_Incoming);
	m_statistics.bandwidthUp = m_ctpEndpoint.GetBandwidth(CPacketRateCalculator::eIO_Outgoing);
}

#if LOAD_NETWORK_CODE
class CNetChannel::CLoadMsg : public CSimpleNetMessage<CNetChannel::SLoadStructure>
{
public:
	CLoadMsg(CNetChannel* pChannel) : CSimpleNetMessage<CNetChannel::SLoadStructure>(SLoadStructure(), CNetChannel::LoadNetworkMessage)
	{
		m_pChannel = pChannel;
	}

	void SetHandle(SSendableHandle handle)
	{
		m_id = handle;
	}

	EMessageSendResult Send(INetSender* pSender)
	{
		m_k++;
		return CSimpleNetMessage<CNetChannel::SLoadStructure>::Send(pSender);
	}

	void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate state)
	{
		if (state != eNSSU_Requeue)
		{
			m_pChannel->m_loadHandles.erase(m_id);
			if (state != eNSSU_Rejected)
				m_pChannel->AddLoadMsg();
		}
	}

private:
	_smart_ptr<CNetChannel> m_pChannel;
	SSendableHandle         m_id;
	int                     m_k;
};

void CNetChannel::AddLoadMsg()
{
	_smart_ptr<CLoadMsg> pMsg = new CLoadMsg(this);
	std::vector<SSendableHandle> deps;
	for (std::set<SSendableHandle>::iterator it = m_loadHandles.begin(); it != m_loadHandles.end(); ++it)
	{
		if (cry_random(0, 63) == 0)
			deps.push_back(*it);
	}
	//	if (deps.empty() && !m_loadHandles.empty())
	//		deps.push_back(*m_loadHandles.begin());
	SSendableHandle hdl;
	NetAddSendable(&*pMsg, deps.size(), deps.empty() ? 0 : &deps[0], &hdl);
	pMsg->SetHandle(hdl);
	m_loadHandles.insert(hdl);
}
#endif

void CNetChannel::ProcessPacket(bool forceWaitForSync, const uint8* pData, uint32 nLength)
{
	MMM_REGION(m_pMMM);

	if (m_bDead)
		return;

#if LOAD_NETWORK_CODE
	if (!m_bConnectionEstablished)
	{
		for (int i = 0; i < LOAD_NETWORK_COUNT; i++)
		{
			AddLoadMsg();
		}
	}
#endif

	if (!m_bConnectionEstablished)
		m_ctpEndpoint.ChangeSubscription(this, eCEE_BecomeAlerted | eCEE_BackoffTooLong);
	m_bConnectionEstablished = true;

#if FORCE_DECODING_TO_GAME_THREAD
	forceWaitForSync = true;
#endif

	TMemHdl hdl = MMM().AllocHdl(nLength);
	memcpy(MMM().PinHdl(hdl), pData, nLength);

	if (!forceWaitForSync && m_queuedForSyncPackets.empty())
		DoProcessPacket(hdl, false);
	else
		m_queuedForSyncPackets.push(hdl);
}

void CNetChannel::GotPacket(EGotPacketType type)
{
	ASSERT_GLOBAL_LOCK;
	switch (type)
	{
	case eGPT_Fake:
		if (m_gotFakePacket)
		{
		case eGPT_Normal:
			m_nKeepAliveTimer2 = g_time;
		case eGPT_KeepAlive:
			m_nKeepAlivePingTimer = m_nKeepAliveTimer = g_time;
			m_gotFakePacket = (type == eGPT_Fake);
		}
		break;
	}
}

void CNetChannel::DoProcessPacket(TMemHdl hdl, bool inSync)
{
	MMM_REGION(m_pMMM);

	if (m_bDead)
	{
		MMM().FreeHdl(hdl);
		return;
	}

	GotPacket(eGPT_Normal);
	CAutoFreeHandle freeHdl(hdl);
	m_ctpEndpoint.ProcessPacket(g_time, freeHdl, true, inSync);

#if USE_CHANNEL_TIMERS
	//#if CHANNEL_TIED_TO_NETWORK_TICK
	//if (gEnv->bMultiplayer == false)
	//#endif
	{
		UpdateTimer(g_time, false);
	}
#endif // USE_CHANNEL_TIMERS
}

void CNetChannel::PunkDetected(EPunkType punkType)
{
	if (!m_pNub)
		return;
	NET_ASSERT(m_pNub->GetSecurity());
	m_pNub->GetSecurity()->OnPunkDetected(RESOLVER.ToString(m_ip), punkType);
}

NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CNetChannel, SetRemoteChannelID, eNRT_ReliableUnordered, true)
{
	m_remoteChannelID = param.m_id;
	return m_remoteChannelID > 0;
}

#if NEW_BANDWIDTH_MANAGEMENT
NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CNetChannel, SetRemotePerformanceMetrics, eNRT_ReliableUnordered, true)
{
	INetChannel::SPerformanceMetrics metrics;
	metrics.m_bandwidthShares = param.m_bandwidthShares;
	metrics.m_packetRate = param.m_packetRate;
	metrics.m_packetRateIdle = param.m_packetRateIdle;

	SetPerformanceMetrics(&metrics);
	return true;
}
#endif

#if LOAD_NETWORK_CODE
NET_IMPLEMENT_SIMPLE_IMMEDIATE_MESSAGE(CNetChannel, LoadNetworkMessage, eNRT_ReliableUnordered, false)
{
	return true;
}
#endif

void CNetChannel::GC_OnDisconnect(EDisconnectionCause cause, string msg, CDontDieLock lock)
{
	if (m_pGameChannel)
	{
		m_pGameChannel->OnDisconnect(cause, msg.c_str());
	}
}

void CNetChannel::GC_DeleteGameChannel(CDontDieLock lock)
{
	if (m_pGameChannel)
		m_pGameChannel->Release();
	m_pGameChannel = 0;
}

void CNetChannel::Die(EDisconnectionCause cause, string msg)
{
	if (!m_bDead)
	{
		if (m_pNub)
		{
			m_pNub->DisconnectChannel(cause, NULL, this, msg.c_str());
		}
		else
		{
			Destroy(cause, msg);
		}
	}
}

void CNetChannel::Destroy(EDisconnectionCause cause, string msg)
{
	if (!m_bDead)
	{
		MMM_REGION(m_pMMM);

		m_bDead = true;
		DisconnectGame(cause, msg);

		if (m_pContextView)
		{
			m_pContextView->Die();
		}

		TO_GAME(&CNetChannel::GC_DeleteGameChannel, this, CDontDieLock(this));

		m_ctpEndpoint.EmptyMessages();
		m_bConnectionEstablished = false;

#if USE_CHANNEL_TIMERS
		TIMER.CancelTimer(m_timer);
		m_timer = 0;
#endif // USE_CHANNEL_TIMERS
		TIMER.CancelTimer(m_statsTimer);
		m_statsTimer = 0;
	}
}

void CNetChannel::GetMemoryStatistics(ICrySizer* pSizer, bool countingThis)
{
	SIZER_COMPONENT_NAME(pSizer, "CNetChannel");
	MMM_REGION(m_pMMM);

	if (countingThis)
		if (!pSizer->Add(*this))
			return;
	for (size_t i = 0; i < m_queuedForSyncPackets.size(); ++i)
	{
		TMemHdl hdl = m_queuedForSyncPackets.front();
		MMM().AddHdlToSizer(hdl, pSizer);
		m_queuedForSyncPackets.pop();
		m_queuedForSyncPackets.push(hdl);
	}
	m_ctpEndpoint.GetMemoryStatistics(pSizer);
	m_pContextView->GetMemoryStatistics(pSizer);
}

bool CNetChannel::IsLocal() const
{
	if (m_isLocal)
		return true;
	return m_pContextView ? m_pContextView->IsLocal() : false;
}

bool CNetChannel::IsFakeChannel() const
{
	return IsLocal() && gEnv->IsDedicated();
}

bool CNetChannel::IsConnectionEstablished() const
{
	SCOPED_GLOBAL_LOCK;
	return m_bConnectionEstablished;
}

bool CNetChannel::IsConnected() const
{
	return !m_bDead && !m_ctpEndpoint.InEmptyMode();
}

void CNetChannel::ForcePacketSend()
{
	SCOPED_GLOBAL_LOCK;
	m_bForcePacketSend = true;
}

CNetChannel* CNetChannel::GetNetChannel()
{
	return this;
}

bool CNetChannel::IsDead()
{
	if (!(m_pendingStuffBeforeDead))
	{
		return m_bDead;
	}
	else
	{
		return false;
	}
}

bool CNetChannel::IsSuicidal()
{
	return m_bDead;
}

#if ENABLE_DEBUG_KIT
TNetChannelID CNetChannel::GetLoggingChannelID()
{
	TNetChannelID out = 0;
	if (m_pContextView)
		out = m_pContextView->GetLoggingChannelID(m_localChannelID, m_remoteChannelID);
	return out;
}
#endif

void CNetChannel::DisconnectGame(EDisconnectionCause cause, string msg)
{
	if (m_sentDisconnect)
		return;

	NetLog("Disconnect %s; profid=%d; cause=%d; msg='%s'", GetRemoteAddressString().c_str(), GetProfileId(), cause, msg.c_str());

	TO_GAME(&CNetChannel::GC_OnDisconnect, this, cause, msg, CDontDieLock(this));
	m_sentDisconnect = true;
}

INetSendablePtr CNetChannel::NetFindSendable(SSendableHandle handle)
{
	return m_ctpEndpoint.FindSendable(handle);
}

#if FULL_ON_SCHEDULING
bool CNetChannel::GetWitnessPosition(Vec3& pos)
{
	if (m_pContextView)
		return m_pContextView->GetWitnessPosition(pos);
	else
		return false;
}

bool CNetChannel::GetWitnessDirection(Vec3& pos)
{
	if (m_pContextView)
		return m_pContextView->GetWitnessDirection(pos);
	else
		return false;
}

bool CNetChannel::GetWitnessFov(float& fov)
{
	if (m_pContextView)
		return m_pContextView->GetWitnessFov(fov);
	else
		return false;
}
#endif

EMessageSendResult CNetChannel::WriteHeader(INetSender* pSender)
{
	if (m_pContextView)
		return m_pContextView->WriteHeader(pSender);
	else
		return eMSR_SentOk;
}

EMessageSendResult CNetChannel::WriteFooter(INetSender* pSender)
{
	if (m_pContextView)
		return m_pContextView->WriteFooter(pSender);
	else
		return eMSR_SentOk;
}

bool CNetChannel::IsServer()
{
	if (!m_pContextView)
		return false;
	else
		return m_pContextView->IsServer();
}

EChannelConnectionState CNetChannel::GetChannelConnectionState() const
{
	if (m_bDead || m_sentDisconnect)
		return eCCS_Disconnecting;
	if (!m_bConnectionEstablished)
		return eCCS_StartingConnection;
	if (m_pContextView && m_pContextView->IsPastOrInState(eCVS_InGame))
		return eCCS_InGame;
	if (m_pContextView)
		return eCCS_InContextInitiation;
	else
		return eCCS_InGame;
}

int CNetChannel::GetContextViewStateDebugCode() const
{
	if (m_pContextView)
		return m_pContextView->GetStateDebugCode();
	else
		return 0;
}

#if ENABLE_DEBUG_KIT
	#include <CryGame/IGameFramework.h>
void CNetChannel::GC_AddPingReadout(float elapsedRemote, float sinceSent, float ping)
{
	if (!CVARS.ShowPing)
		return;

	if (!m_pDebugHistory)
	{
		m_pDebugHistory = gEnv->pGameFramework->CreateDebugHistoryManager();
	}

	if (!m_pElapsedRemote)
	{
		m_pElapsedRemote = m_pDebugHistory->CreateHistory("ElapsedRemote");
		m_pElapsedRemote->SetupLayoutAbs(50, 110, 200, 120, 5);
		m_pElapsedRemote->SetupScopeExtent(-360, +360, -.01f, +.01f);
	}

	if (!m_pSinceSent)
	{
		m_pSinceSent = m_pDebugHistory->CreateHistory("TimeSinceSent");
		m_pSinceSent->SetupLayoutAbs(50, 240, 200, 120, 5);
		m_pSinceSent->SetupScopeExtent(-360, +360, -.01f, +.01f);
	}

	if (!m_pPing)
	{
		m_pPing = m_pDebugHistory->CreateHistory("FrameTime");
		m_pPing->SetupLayoutAbs(50, 370, 200, 120, 5);
		m_pPing->SetupScopeExtent(-360, +360, -.01f, +.01f);
	}

	m_pElapsedRemote->AddValue(elapsedRemote);
	m_pSinceSent->AddValue(sinceSent);
	m_pPing->AddValue(ping);

	m_pElapsedRemote->SetVisibility(true);
	m_pSinceSent->SetVisibility(true);
	m_pPing->SetVisibility(true);
}
#endif

void CNetChannel::OnEndpointEvent(const SCTPEndpointEvent& evt)
{
	switch (evt.event)
	{
	case eCEE_BecomeAlerted:
		OnChangedIdle();
		break;
	case eCEE_BackoffTooLong:
		if (m_nKeepAliveTimer != CTimeValue(0.0f) && (g_time - m_nKeepAliveTimer2).GetSeconds() < 20.0f)
			GotPacket(eGPT_Fake);
		break;
	}
}

void CNetChannel::OnChangedIdle()
{
#if USE_CHANNEL_TIMERS
	//#if CHANNEL_TIED_TO_NETWORK_TICK
	//if (gEnv->bMultiplayer == false)
	//#endif
	{
		UpdateTimer(g_time, true);
	}
#endif // USE_CHANNEL_TIMERS
}

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
CTimeValue CNetChannel::TimeSinceVoiceTransmission()
{
	SCOPED_GLOBAL_LOCK;
	return g_time - m_lastVoiceTransmission;
}

CTimeValue CNetChannel::TimeSinceVoiceReceipt(EntityId id)
{
	if (CVARS.useDeprecatedVoiceSystem)
	{
		SCOPED_GLOBAL_LOCK;
		if (m_pContextView)
			return m_pContextView->TimeSinceVoiceReceipt(id);
	}
	return 1000000.0f;
}

void CNetChannel::AllowVoiceTransmission(bool allow)
{
	SCOPED_GLOBAL_LOCK;
	if (!m_pContextView)
		return;
	m_pContextView->AllowVoiceTransmission(allow);
}
#endif
bool CNetChannel::IsPreordered() const
{
	return m_preordered;
}

int CNetChannel::GetProfileId() const
{
	return m_profileId;
}

string CNetChannel::GetCDKeyHash() const
{
	return m_CDKeyHash;
}

void CNetChannel::SetProfileId(int id)
{
	m_profileId = id;
	;
}

void CNetChannel::SetNickname(const char* name)
{
	m_nickname = name;
}

void CNetChannel::SetPreordered(bool p)
{
	m_preordered = p;
}

void CNetChannel::SetCDKeyHash(const char* hash)
{
	m_CDKeyHash = hash;
}

CrySessionHandle CNetChannel::GetSession() const
{
	return m_session;
}

bool CNetChannel::IsIdle()
{
	if (!m_isLocal)
		return false;
	return IsTimeReady() && (m_pContextView ? m_pContextView->IsIdle() : true);
}

void CNetChannel::NetDump(ENetDumpType type, INetDumpLogger& logger)
{
	if ((type == eNDT_ClientConnectionState) || (type == eNDT_ServerConnectionState))
	{
		CRYSOCKADDR_IN addr;
		string sockaddrStr;

		if (ConvertAddr(m_ip, &addr))
		{
			sockaddrStr.Format("%s:%u", CrySock::inet_ntoa(addr.sin_addr), addr.sin_port);
		}
		else
		{
			sockaddrStr = "unknown";
		}

		logger.Log("    Connection:");
		logger.Log("      Socket address: %s", sockaddrStr.c_str());

		if (type == eNDT_ServerConnectionState)
		{
			logger.Log("      Player Name:    %s", m_nickname.c_str());
		}
	}

	if (m_pContextView)
		m_pContextView->NetDump(type, logger);
}

bool CNetChannel::IsInTransition()
{
	SCOPED_GLOBAL_LOCK;
	if (!m_pContextView || !m_pContextView->Context() || !m_pContextView->ContextState())
		return false;
	return m_pContextView->Context()->GetCurrentState() != m_pContextView->ContextState();
}

void CNetChannel::SendSimplePacket(EHeaders header)
{
	Send(&Frame_IDToHeader[header], 1);
}

void CNetChannel::ProcessSimplePacket(EHeaders hdr)
{
	ASSERT_GLOBAL_LOCK;
	GotPacket(eGPT_KeepAlive);
	switch (hdr)
	{
	case eH_BackOff:
		NetLog("%s requests backoff due to congestion", GetName());
		m_ctpEndpoint.BackOff();
#if USE_CHANNEL_TIMERS
		//#if CHANNEL_TIED_TO_NETWORK_TICK
		//if (gEnv->bMultiplayer == false)
		//#endif
		{
			UpdateTimer(g_time, false);
		}
#endif // USE_CHANNEL_TIMERS
		break;
	case eH_KeepAlive:
		if (g_time - KEEPALIVE_REPLY_TIMEOUT > m_nKeepAliveReplyTimer)
		{
			SendSimplePacket(eH_KeepAliveReply);
			m_nKeepAliveReplyTimer = g_time;
		}
		break;
	case eH_KeepAliveReply:
		// do nothing
		break;
	}
}

TUpdateMessageBrackets CNetChannel::GetUpdateMessageBrackets()
{
	return m_pContextView->GetUpdateMessageBrackets();
}

bool CNetChannel::SendSyncedFiles()
{
#if SERVER_FILE_SYNC_MODE
	NET_ASSERT(ServerFileSyncEnabled());
	SCOPED_GLOBAL_LOCK;
	if (CSyncedFileSet* pSet = m_pContextView->Context()->GetSyncedFileSet(false))
		pSet->SendFilesTo(this);
#endif
	return true;
}

void CNetChannel::AddWaitForFileSyncComplete(IContextEstablisher* pEst, EContextViewState when)
{
	if (m_pContextView)
		m_pContextView->AddWaitForFileSyncComplete(pEst, when);
}

void CNetChannel::SetMigratingChannel(bool bIsMigrating)
{
	m_bIsMigratingChannel = bIsMigrating;
}

bool CNetChannel::IsMigratingChannel() const
{
	return m_bIsMigratingChannel;
}

#if ENABLE_RMI_BENCHMARK

void CNetChannel::LogRMIBenchmark(ERMIBenchmarkAction action, const SRMIBenchmarkParams& params, void (* pCallback)(ERMIBenchmarkLogPoint point0, ERMIBenchmarkLogPoint point1, int64 delay, void* pUserData), void* pUserData)
{
	SRMIBenchmarkRecord& record = m_RMIBenchmarkRecords[params.seq];
	uint32 point = (params.message << 2) | action;

	if ((point == eRMIBLP_ClientGameThreadQueuePing) || (point == eRMIBLP_ServerNetworkThreadReceivePing))
	{
		record.entity = params.entity;
		record.eventBits = 0;
	}

	if (params.entity == record.entity)
	{
		if (pCallback && pUserData)
		{
			record.pCallback = pCallback;
			record.pUserData = pUserData;
		}

		record.eventBits = (record.eventBits & (BIT(point + 1) - 1)) | BIT(point);
		record.eventTimes[point] = gEnv->pTimer->GetAsyncTime();

		if (record.pCallback)
		{
			bool invokeCallback = false;

			if (params.twoRoundTrips)
			{
				if ((record.eventBits == eRMIBLPB_ClientAllDoubleMask) || (record.eventBits == eRMIBLPB_ServerAllDoubleMask))
				{
					invokeCallback = true;
				}
			}
			else
			{
				if ((record.eventBits == eRMIBLPB_ClientAllSingleMask) || (record.eventBits == eRMIBLPB_ServerAllSingleMask))
				{
					invokeCallback = true;
				}
			}

			if (invokeCallback)
			{
				ERMIBenchmarkThread thread = RMIBenchmarkGetThread(point);

				if (thread == eRMIBT_Game)
				{
					LogRMIBenchmarkInvokeCallback(record);
				}
				else
				{
					TO_GAME(&CNetChannel::LogRMIBenchmarkInvokeCallback, this, record);
				}
			}
		}
	}
}

void CNetChannel::LogRMIBenchmarkInvokeCallback(SRMIBenchmarkRecord record)
{
	int64 firstTime;
	int64 lastTime;
	ERMIBenchmarkLogPoint firstPoint;
	ERMIBenchmarkLogPoint lastPoint;
	bool haveFirst = false;
	bool haveLast = false;

	for (ERMIBenchmarkLogPoint thisPoint = eRMIBLP_ClientGameThreadQueuePing; thisPoint < eRMIBLP_Num; thisPoint = (ERMIBenchmarkLogPoint)(thisPoint + 1))
	{
		if (record.eventBits & BIT(thisPoint))
		{
			int64 thisTime = record.eventTimes[thisPoint].GetMilliSecondsAsInt64();

			if (!haveFirst)
			{
				firstTime = thisTime;
				firstPoint = thisPoint;
				haveFirst = true;
			}

			if (haveLast)
			{
				record.pCallback(lastPoint, thisPoint, thisTime - lastTime, record.pUserData);
			}

			lastTime = thisTime;
			lastPoint = thisPoint;
			haveLast = true;
		}
	}

	record.pCallback(firstPoint, lastPoint, lastTime - firstTime, record.pUserData);
}

#endif
