// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  implements a communications channel between two computers
   -------------------------------------------------------------------------
   History:
   - 26/07/2004   : Created by Craig Tiller
*************************************************************************/
#ifndef __NETCHANNEL_H__
#define __NETCHANNEL_H__

#pragma once

#include <CryNetwork/NetHelpers.h>
#include "CTPEndpoint.h"
#include "NetNub.h"
#include "INubMember.h"
#include "Config.h"
#include <CryAction/IDebugHistory.h>
#include <queue>

class CNetContext;
class CDisconnectMessage;
class CContextView;

typedef std::pair<const SNetMessageDef*, const SNetMessageDef*> TUpdateMessageBrackets;

struct SSetRemoteChannelID
{
	SSetRemoteChannelID() : m_id(0) {}
	SSetRemoteChannelID(TNetChannelID id) : m_id(id) {}
	TNetChannelID m_id;
	void          SerializeWith(TSerialize ser)
	{
		ser.Value("id", m_id);
	}
};

#if NEW_BANDWIDTH_MANAGEMENT
struct SSerialisePerformanceMetrics : public INetChannel::SPerformanceMetrics
{
	SSerialisePerformanceMetrics() {}
	SSerialisePerformanceMetrics(INetChannel::SPerformanceMetrics& metrics)
		: INetChannel::SPerformanceMetrics(metrics)
	{}

	void SerializeWith(TSerialize ser)
	{
		ser.Value("bwsh", m_bandwidthShares);
		ser.Value("nrpr", m_packetRate);
		ser.Value("idpr", m_packetRateIdle);
	}
};
#endif // NEW_BANDWIDTH_MANAGEMENT

class CNetChannel : public CNetMessageSinkHelper<CNetChannel, INetChannel>, public INubMember, public ICTPEndpointListener
{
public:
	CNetChannel(const TNetAddress& ipRemote, const CExponentialKeyExchange::KEY_TYPE& key, uint32 remoteSocketCaps, CrySessionHandle session);
	~CNetChannel();

	// INetChannel
	virtual void               SetPerformanceMetrics(SPerformanceMetrics* pMetrics);
	virtual ChannelMaskType    GetChannelMask()                        { return m_channelMask; }
	virtual void               SetChannelMask(ChannelMaskType newMask) { m_channelMask = newMask; }
	virtual void               SetClient(INetContext* pNetContext);
	virtual void               SetServer(INetContext* pNetContext);
	virtual void               Disconnect(EDisconnectionCause cause, const char* fmt, ...);
	virtual void               SendMsg(INetMessage*);
	virtual bool               AddSendable(INetSendablePtr pSendable, int numAfterHandle, const SSendableHandle* afterHandle, SSendableHandle* handle);
	virtual bool               SubstituteSendable(INetSendablePtr pSendable, int numAfterHandle, const SSendableHandle* afterHandle, SSendableHandle* handle);
	virtual bool               RemoveSendable(SSendableHandle handle);
	virtual const SStatistics& GetStatistics();
	virtual void               SetPassword(const char* password);
	virtual CrySessionHandle   GetSession() const;
	virtual IGameChannel*      GetGameChannel()
	{
		SCOPED_GLOBAL_LOCK;
		return m_pGameChannel;
	}
	virtual CTimeValue    GetRemoteTime() const;
	virtual float         GetPing(bool smoothed) const;
	virtual bool          IsSufferingHighLatency(CTimeValue nTime) const;
	virtual CTimeValue    GetTimeSinceRecv() const;
	virtual void          DispatchRMI(IRMIMessageBodyPtr pBody);
	virtual void          DeclareWitness(EntityId id);
	virtual bool          IsLocal() const;
	virtual bool          IsFakeChannel() const;
	virtual bool          IsConnectionEstablished() const;
	virtual string        GetRemoteAddressString() const;
	virtual bool          GetRemoteNetAddress(uint32& uip, uint16& port, bool firstLocal = true);
	virtual const char*   GetName();
	virtual const char*   GetNickname();
	virtual void          SetNickname(const char* name);
	virtual TNetChannelID GetLocalChannelID()  { return m_localChannelID; }
	virtual TNetChannelID GetRemoteChannelID() { return m_remoteChannelID; }

	virtual void          CallUpdate(CTimeValue time);
#if !USE_CHANNEL_TIMERS
	virtual void          CallUpdateIfNecessary(CTimeValue time, bool force);
#endif // !USE_CHANNEL_TIMERS

	virtual EChannelConnectionState GetChannelConnectionState() const;
	virtual EContextViewState       GetContextViewState() const;
	virtual int                     GetContextViewStateDebugCode() const;
	virtual bool                    IsTimeReady() const { return m_ctpEndpoint.IsTimeReady(); }

	virtual bool                    IsInTransition();

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	virtual CTimeValue TimeSinceVoiceTransmission();
	virtual CTimeValue TimeSinceVoiceReceipt(EntityId id);
	virtual void       AllowVoiceTransmission(bool allow);
#endif

	virtual bool IsPreordered() const;
	virtual int  GetProfileId() const;

	virtual bool SendSyncedFiles();
	virtual void AddWaitForFileSyncComplete(IContextEstablisher* pEst, EContextViewState when);

	virtual void SetMigratingChannel(bool bIsMigrating);
	virtual bool IsMigratingChannel() const;

	// ~INetChannel

#if ENABLE_RMI_BENCHMARK
	virtual void LogRMIBenchmark(ERMIBenchmarkAction action, const SRMIBenchmarkParams& params, void (* pCallback)(ERMIBenchmarkLogPoint point0, ERMIBenchmarkLogPoint point1, int64 delay, void* pUserData), void* pUserData);
#endif

#if NEW_BANDWIDTH_MANAGEMENT
	const INetChannel::SPerformanceMetrics* GetPerformanceMetrics(void) const;
#endif // NEW_BANDWIDTH_MANAGEMENT

	void       GetBandwidthStatistics(uint32 channelIndex, SBandwidthStats* const pStats);

	string     GetCDKeyHash() const;

	ILINE bool NetAddSendable(INetSendable* pMsg, int numAfterHandle, const SSendableHandle* afterHandle, SSendableHandle* handle)
	{
		ASSERT_GLOBAL_LOCK;

		if (m_bDead)
		{
			FailAddSendableDead(pMsg);
			return false;
		}

		return m_ctpEndpoint.AddSendable(pMsg, numAfterHandle, afterHandle, handle);
	}
	ILINE bool NetSubstituteSendable(INetSendable* pMsg, int numAfterHandle, const SSendableHandle* afterHandle, SSendableHandle* handle)
	{
		ASSERT_GLOBAL_LOCK;

		if (m_bDead)
		{
			FailAddSendableDead(pMsg);
			return false;
		}

		return m_ctpEndpoint.SubstituteSendable(pMsg, numAfterHandle, afterHandle, handle);
	}
	ILINE bool NetRemoveSendable(const SSendableHandle& handle)
	{
		ASSERT_GLOBAL_LOCK;
		return m_ctpEndpoint.RemoveSendable(handle);
	}

	INetSendablePtr NetFindSendable(SSendableHandle handle);
	void            ChangeSubscription(ICTPEndpointListener* pListener, uint32 eventMask) { m_ctpEndpoint.ChangeSubscription(pListener, eventMask); }

	// INetMessageSink
	virtual void DefineProtocol(IProtocolBuilder* pBuilder);
	// ~INetMessageSink

	// INubMember
	virtual CNetChannel* GetNetChannel();
	virtual bool         IsDead();
	virtual void         ProcessPacket(bool forceWaitForSync, const uint8* pData, uint32 nLength);
	virtual void         ProcessSimplePacket(EHeaders hdr);
	virtual void         PerformRegularCleanup();
	virtual void         SyncWithGame(ENetworkGameSync type);
	virtual void         Die(EDisconnectionCause cause, string msg);
	virtual void         RemovedFromNub();
	virtual bool         IsSuicidal();
	virtual void         NetDump(ENetDumpType type, INetDumpLogger& logger);
	// ~INubMember

	// ICTPEndpointListener
	virtual void OnEndpointEvent(const SCTPEndpointEvent& evt);
	// ~ICTPEndpointListener

	void Destroy(EDisconnectionCause cause, string msg);
	void ForcePacketSend();
#if !NEW_BANDWIDTH_MANAGEMENT
	void FragmentedPacket();
#endif // !NEW_BANDWIDTH_MANAGEMENT
	void Init(CNetNub* pNub, IGameChannel* pGameChannel);
	void Send(const uint8* pData, size_t nLength)
	{
		ASSERT_GLOBAL_LOCK;
		if (m_pNub)
		{
			if (!m_pNub->SendTo(pData, nLength, m_ip))
				Disconnect(eDC_ICMPError, "Send failed");
		}
	}
	// send a packet out of band (not via the channel) directly to some other network address
	// like CNetNub::SendTo
	void SendWithNubTo(const uint8* pData, size_t nLength, const TNetAddress& to)
	{
		if (m_pNub)
		{
			if (!m_pNub->SendTo(pData, nLength, to))
				Disconnect(eDC_ICMPError, "Send failed");
		}
		return;
	}
	void                     GetMemoryStatistics(ICrySizer* pSizer, bool countingThis = false);
	void                     SetEntityId(EntityId id)             { m_ctpEndpoint.SetEntityId(id); }
	uint32                   GetMostRecentAckedSeq() const        { return m_ctpEndpoint.GetMostRecentAckedSeq(); }
	uint32                   GetMostRecentSentSeq() const         { return m_ctpEndpoint.GetMostRecentSentSeq(); }
	bool                     IsServer();
	void                     SetAfterSpawning(bool afterSpawning) { m_ctpEndpoint.SetAfterSpawning(afterSpawning); }
	CTimeValue               GetIdleTime(bool realtime)           { return g_time - (realtime ? m_nKeepAliveTimer2 : m_nKeepAliveTimer); }
	CTimeValue               GetInactivityTimeout(bool backingOff);

	CMementoMemoryManagerPtr GetChannelMMM()   { return m_pMMM; }

	void                     UnblockMessages() { m_ctpEndpoint.UnblockMessages(); }
	bool                     OnMessageQueueEmpty();

	bool                     LookupMessage(const char* name, SNetMessageDef const** ppDef, INetMessageSink** ppSink)
	{
		return m_ctpEndpoint.LookupMessage(name, ppDef, ppSink);
	}
	bool IsConnected() const; // we are connected until we are disconnected

	void DisconnectGame(EDisconnectionCause dc, string msg);

#if FULL_ON_SCHEDULING
	bool GetWitnessPosition(Vec3& pos);
	bool GetWitnessDirection(Vec3& pos);
	bool GetWitnessFov(float& fov);
#endif

	EMessageSendResult     WriteHeader(INetSender* pSender);
	EMessageSendResult     WriteFooter(INetSender* pSender);

	bool                   IsIdle();
	void                   OnChangedIdle();

	TUpdateMessageBrackets GetUpdateMessageBrackets();

#if LOAD_NETWORK_CODE
	struct SLoadStructure
	{
		unsigned char buf[LOAD_NETWORK_PKTSIZE];
		void          SerializeWith(TSerialize ser)
		{
			for (int i = 0; i < LOAD_NETWORK_PKTSIZE; i++)
			{
				ser.Value("b", buf[i]);
			}
		}
	};
	std::set<SSendableHandle> m_loadHandles;
	class CLoadMsg;
	void AddLoadMsg();
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(LoadNetworkMessage, SLoadStructure);
#endif

	NET_DECLARE_IMMEDIATE_MESSAGE(Ping);
	NET_DECLARE_IMMEDIATE_MESSAGE(Pong);

	CNetwork*          GetNetwork() { return (CNetwork*)gEnv->pNetwork; }

	void               PunkDetected(EPunkType punkType);
	const TNetAddress& GetIP() const { return m_ip; }
	void               DoProcessPacket(TMemHdl hdl, bool inSync);

	void               GetLocalIPs(TNetAddressVec& vIPs)
	{
		if (m_pNub)
			m_pNub->GetLocalIPs(vIPs);
	}

	CContextView* GetContextView()
	{
		if (m_pContextView == NULL)
			return 0;
		else
			return m_pContextView;
	}

	//string  GetRemoteAddressString()const;
	//bool GetRemoteNetAddress(uint32 &ip, uint16 &port);

	void BeginStrongCaptureRMIs()
	{
		m_nStrongCaptureRMIs++;
	}
	void EndStrongCaptureRMIs()
	{
		m_nStrongCaptureRMIs--;
	}

	void TransmittedVoice()
	{
		m_lastVoiceTransmission = g_time;
	}

	void ResetTimeSinceRecv() { m_timeSinceRecv = 0ll; }

	void SetPreordered(bool p);
	void SetProfileId(int id);
	void SetCDKeyHash(const char* hash);

	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(SetRemoteChannelID, SSetRemoteChannelID);

#if NEW_BANDWIDTH_MANAGEMENT
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(SetRemotePerformanceMetrics, SSerialisePerformanceMetrics);
#endif

#if ENABLE_DEBUG_KIT
	TNetChannelID GetLoggingChannelID();
	void          GC_AddPingReadout(float elapsedRemote, float sinceSent, float ping);
#endif

	void GC_SendableSink(INetSendablePtr) {}

	// queued functions from game
	void NC_DispatchRMI(IRMIMessageBodyPtr pBody);
	bool DoDispatchRMI(IRMIMessageBodyPtr pBody);
	void NC_SendMsg(INetMessage* pMsg);
	void NC_DeclareWitness(EntityId id);

#if ENABLE_URGENT_RMIS
	bool WriteUrgentMessages(void) const { return m_writeUrgentMessages; }
#endif // ENABLE_URGENT_RMIS

	const CExponentialKeyExchange::KEY_TYPE& GetPrivateKey() { return m_privateKey; }

	virtual bool                             HasGameRequestedUpdate()
	{
		bool requested = m_gameHasRequestedUpdate;
		m_gameHasRequestedUpdate = false;
		return requested;
	}

	virtual void RequestUpdate(CTimeValue time) { m_gameHasRequestedUpdate = true; }

private:
	friend class CDisconnectMessage;

	static void TimerCallback(NetTimerId, void*, CTimeValue);
	void Update(bool finalUpdate, CTimeValue time);
#if USE_CHANNEL_TIMERS
	void UpdateTimer(CTimeValue time, bool forceUpdate);
#endif // USE_CHANNEL_TIMERS
	void UpdateStats(CTimeValue time);

	bool SupportsBackoff()
	{
		return (m_remoteSocketCaps & eSIOMC_SupportsBackoff) != 0 && (CNetwork::Get()->GetExternalSocketIOManager().caps & eSIOMC_SupportsBackoff) != 0;
	}

	class CDontDieLock;
	void GC_DeleteGameChannel(CDontDieLock);
	void GC_OnDisconnect(EDisconnectionCause cause, string msg, CDontDieLock);

	void SendSimplePacket(EHeaders header);
	enum EGotPacketType
	{
		eGPT_Fake,
		eGPT_KeepAlive,
		eGPT_Normal
	};
	void GotPacket(EGotPacketType);

	class CPingMsg;
	class CPongMsg;

	void LockAlive()
	{
		++m_pendingStuffBeforeDead;
	}
	void UnlockAlive()
	{
		--m_pendingStuffBeforeDead;
	}

	class CDontDieLock
	{
	public:
		CDontDieLock() : m_pChannel(0) {}
		CDontDieLock(CNetChannel* pChannel) : m_pChannel(pChannel)
		{
			m_pChannel->LockAlive();
		}
		CDontDieLock(const CDontDieLock& lk) : m_pChannel(lk.m_pChannel)
		{
			if (m_pChannel)
				m_pChannel->LockAlive();
		}
		~CDontDieLock()
		{
			if (m_pChannel)
				m_pChannel->UnlockAlive();
		}
		void Swap(CDontDieLock& lk)
		{
			std::swap(m_pChannel, lk.m_pChannel);
		}
		CDontDieLock& operator=(CDontDieLock lk)
		{
			Swap(lk);
			return *this;
		}

	private:
		CNetChannel* m_pChannel;
	};

#if ENABLE_RMI_BENCHMARK

	enum ERMIBenchmarkLogPointBit
	{
		eRMIBLPB_ClientGameThreadQueuePing      = BIT(eRMIBLP_ClientGameThreadQueuePing),
		eRMIBLPB_ClientNetworkThreadSendPing    = BIT(eRMIBLP_ClientNetworkThreadSendPing),
		eRMIBLPB_ServerNetworkThreadReceivePing = BIT(eRMIBLP_ServerNetworkThreadReceivePing),
		eRMIBLPB_ServerGameThreadHandlePing     = BIT(eRMIBLP_ServerGameThreadHandlePing),
		eRMIBLPB_ServerGameThreadQueuePong      = BIT(eRMIBLP_ServerGameThreadQueuePong),
		eRMIBLPB_ServerNetworkThreadSendPong    = BIT(eRMIBLP_ServerNetworkThreadSendPong),
		eRMIBLPB_ClientNetworkThreadReceivePong = BIT(eRMIBLP_ClientNetworkThreadReceivePong),
		eRMIBLPB_ClientGameThreadHandlePong     = BIT(eRMIBLP_ClientGameThreadHandlePong),
		eRMIBLPB_ClientGameThreadQueuePang      = BIT(eRMIBLP_ClientGameThreadQueuePang),
		eRMIBLPB_ClientNetworkThreadSendPang    = BIT(eRMIBLP_ClientNetworkThreadSendPang),
		eRMIBLPB_ServerNetworkThreadReceivePang = BIT(eRMIBLP_ServerNetworkThreadReceivePang),
		eRMIBLPB_ServerGameThreadHandlePang     = BIT(eRMIBLP_ServerGameThreadHandlePang),
		eRMIBLPB_ServerGameThreadQueuePeng      = BIT(eRMIBLP_ServerGameThreadQueuePeng),
		eRMIBLPB_ServerNetworkThreadSendPeng    = BIT(eRMIBLP_ServerNetworkThreadSendPeng),
		eRMIBLPB_ClientNetworkThreadReceivePeng = BIT(eRMIBLP_ClientNetworkThreadReceivePeng),
		eRMIBLPB_ClientGameThreadHandlePeng     = BIT(eRMIBLP_ClientGameThreadHandlePeng),
		eRMIBLPB_ClientAllSingleMask            = eRMIBLPB_ClientGameThreadQueuePing | eRMIBLPB_ClientNetworkThreadSendPing | eRMIBLPB_ClientNetworkThreadReceivePong | eRMIBLPB_ClientGameThreadHandlePong,
		eRMIBLPB_ServerAllSingleMask            = eRMIBLPB_ServerNetworkThreadReceivePing | eRMIBLPB_ServerGameThreadHandlePing | eRMIBLPB_ServerGameThreadQueuePong | eRMIBLPB_ServerNetworkThreadSendPong,
		eRMIBLPB_ClientAllDoubleMask            = eRMIBLPB_ClientAllSingleMask | eRMIBLPB_ClientGameThreadQueuePang | eRMIBLPB_ClientNetworkThreadSendPang | eRMIBLPB_ClientNetworkThreadReceivePeng | eRMIBLPB_ClientGameThreadHandlePeng,
		eRMIBLPB_ServerAllDoubleMask            = eRMIBLPB_ServerAllSingleMask | eRMIBLPB_ServerNetworkThreadReceivePang | eRMIBLPB_ServerGameThreadHandlePang | eRMIBLPB_ServerGameThreadQueuePeng | eRMIBLPB_ServerNetworkThreadSendPeng
	};

	struct SRMIBenchmarkRecord
	{
		SRMIBenchmarkRecord()
			: pCallback(NULL),
			pUserData(NULL),
			entity(0),
			eventBits(0)
		{
		}

		void       (* pCallback)(ERMIBenchmarkLogPoint point0, ERMIBenchmarkLogPoint point1, int64 delay, void* pUserData);
		void*      pUserData;
		EntityId   entity;
		uint32     eventBits;
		CTimeValue eventTimes[eRMIBLP_Num];
	};

	void LogRMIBenchmarkInvokeCallback(SRMIBenchmarkRecord record);

#endif

	void FailAddSendableDead(INetSendable* pMsg)
	{
		NetWarning("Attempting to send message %s after disconnection", pMsg->GetDescription());
		pMsg->UpdateState(0, eNSSU_Rejected);
	}

	std::queue<TMemHdl>               m_queuedForSyncPackets;
	std::priority_queue<CTimeValue, std::vector<CTimeValue>, std::greater<CTimeValue>> m_pings;
	CExponentialKeyExchange::KEY_TYPE m_privateKey;
	TNetAddress                       m_ip;
	SStatistics                       m_statistics;
	SSendableHandle                   m_pongHdl;

	CTimeValue                        m_nKeepAliveTimer;
	CTimeValue                        m_nKeepAliveTimer2;
	CTimeValue                        m_nKeepAlivePingTimer;
	CTimeValue                        m_nKeepAliveReplyTimer;
	CTimeValue                        m_lastVoiceTransmission;
	CTimeValue                        m_lastPingSent;
	CTimeValue                        m_remoteTime;
	CTimeValue                        m_timeSinceRecv;
#if USE_CHANNEL_TIMERS
	NetTimerId                        m_timer;
#endif // USE_CHANNEL_TIMERS
	NetTimerId                        m_statsTimer;

	CTimeValue                        m_lastUpdateTime;
#if !USE_CHANNEL_TIMERS
	CTimeValue                        m_channelUpdateAccumulator;
	CTimeValue                        m_lastSyncTime;
#endif // !USE_CHANNEL_TIMERS

	CMementoMemoryManagerPtr m_pMMM;
	IGameChannel*            m_pGameChannel;
	_smart_ptr<CContextView> m_pContextView;
	CNetNub*                 m_pNub;

#if ENABLE_DEBUG_KIT
	IDebugHistoryManager* m_pDebugHistory;
	IDebugHistory*        m_pElapsedRemote;
	IDebugHistory*        m_pSinceSent;
	IDebugHistory*        m_pPing;
#endif

	CCTPEndpoint         m_ctpEndpoint;

	string               m_password;
	string               m_CDKeyHash;
	string               m_nickname;

	const TNetChannelID  m_localChannelID;
	TNetChannelID        m_remoteChannelID;
	static TNetChannelID m_nextLocalChannelID;

	CrySessionHandle     m_session;
	int32                m_nStrongCaptureRMIs;
	uint32               m_remoteSocketCaps;
	int32                m_profileId;
	int32                m_pendingStuffBeforeDead;

public:
	int32 m_fastLookupId;
private:

	ChannelMaskType m_channelMask;

	bool            m_bDead;
	bool            m_pingLock;
	bool            m_isSufferingHighLatency;
	bool            m_preordered;
	// is the connection established (has the other end sent us a packet)
	bool            m_bConnectionEstablished;
	// should we force a packet output this frame?
	bool            m_bForcePacketSend;
	bool            m_sentDisconnect;
	bool            m_gotFakePacket;
	const bool      m_isLocal;
	bool            m_gameHasRequestedUpdate;
	bool            m_bIsMigratingChannel;
#if ENABLE_URGENT_RMIS
	bool            m_writeUrgentMessages;
	bool            m_haveUrgentMessages;
#endif // ENABLE_URGENT_RMIS

#if ENABLE_RMI_BENCHMARK

	SRMIBenchmarkRecord m_RMIBenchmarkRecords[RMI_BENCHMARK_MAX_RECORDS];

#endif
};

struct StrongCaptureRMIs
{
	StrongCaptureRMIs(CNetChannel* pNC) : m_pNC(pNC) { m_pNC->BeginStrongCaptureRMIs(); }
	~StrongCaptureRMIs() { m_pNC->EndStrongCaptureRMIs(); }
	_smart_ptr<CNetChannel> m_pNC;
};

#endif
