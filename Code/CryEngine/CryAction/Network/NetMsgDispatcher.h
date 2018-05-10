// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   NetMsgDispatcher.h
//  Description: Central Hub for bidirectional communication of simple messages
//  Uses: Can be used to send messages from client to server and vice versa.
//  - Thread Safety:
//		* Listeners
//			- Are all local.
//			- Registration / de-registration / OnMessage callback must happen on the same thread
//		* Received messages through the network (m_receivedNetMsgQueue)
//			- Thread safe via m_criticalSectionReceivedQueue
//		* Messages sent through the network
//			- Only the stats are not thread safe (but they are debug only)
//			- If needed they could be guarded but currently all messages are sent from main thread (Flow Graph)
// -------------------------------------------------------------------------
//  History: Dario Sancho. 15.10.2014. Created
//
////////////////////////////////////////////////////////////////////////////

#ifndef __NETMSGDISPATCHER__
#define __NETMSGDISPATCHER__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryCore/Containers/CryListenerSet.h>
#include <CryNetwork/INetwork.h>

#if !defined(_RELEASE)
	#define NET_MSG_DISPATCHER_LOG(...) CNetMessageDistpatcher::Log(stack_string().Format("[NetMsg] " __VA_ARGS__).c_str())
#else
	#define NET_MSG_DISPATCHER_LOG(...) ((void)0)
#endif

// ------------------------------------------------------------------------
// Data to send over the network
// ------------------------------------------------------------------------
struct SNetMsgData : public ISerializable
{
	enum ELocation
	{
		eL_Server = 0,
		eL_DedicatedServer,
		eL_Client,
		eL_Unknown,
	};

	enum ETarget
	{
		eT_Default = 0,
		eT_All,
		eT_Invalid
	};

	SNetMsgData() : src(eL_Unknown), tgt(eT_Invalid) {}
	SNetMsgData(ELocation src_, ETarget tgt_, const string& key_, const string& value_) : src(src_), tgt(tgt_), key(key_), value(value_) {}

	ELocation src;
	ETarget   tgt;
	string    key, value;

	virtual void SerializeWith(TSerialize ser)
	{
		ser.EnumValue("src", src, eL_Server, eL_Unknown);
		ser.EnumValue("tgt", tgt, eT_Default, eT_All);
		ser.Value("key", key, 'stab'); // 'stab' we assume here that there are not many different string values
		ser.Value("value", value);     // 'stab' we assume here that there are not many different string values
	}

	static ETarget ConvertTargetIntToEnum(int i)
	{
		if (i >= eT_Default && i <= eT_All)
		{
			return static_cast<ETarget>(i);
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_ERROR, "Invalid conversion from int to ETarget");
			return eT_Invalid;
		}
	}

	const char* DbgTranslateTargetToString() const
	{
		return tgt == eT_Default ? "Default" :
		       tgt == eT_All ? "All" :
		       tgt == eT_Invalid ? "Invalid" :
		       "Error";
	}

	const char* DbgTranslateSourceToString() const
	{
		return src == SNetMsgData::eL_Server ? "Server" :
		       src == SNetMsgData::eL_DedicatedServer ? "Dedicated Server" :
		       src == SNetMsgData::eL_Client ? "Client" :
		       src == SNetMsgData::eL_Unknown ? "Unknown" :
		       "Error";
	}
};

// ------------------------------------------------------------------------
// INetMsgListener
// ------------------------------------------------------------------------
struct INetMsgListener
{
	virtual void OnMessage(const SNetMsgData& data) = 0;
};

// ------------------------------------------------------------------------
// NetMsgDispatcher
// ------------------------------------------------------------------------

class CNetMessageDistpatcher :
	public CNetMessageSinkHelper<CNetMessageDistpatcher, INetMessageSink>
{
public:
	CNetMessageDistpatcher();
	virtual ~CNetMessageDistpatcher();

	// INetMessageSink
	virtual void DefineProtocol(IProtocolBuilder* pBuilder) override;
	// ~INetMessageSink

	void Update();
	void SendMessage(const SNetMsgData& msg);

	void RegisterListener(INetMsgListener* pListener, const char* name = NULL);
	void UnRegisterListener(INetMsgListener* pListener);
	void ClearListeners();
	void ClearStatsCommand() { m_stats.ClearStats(); }

	// exposed methods that can be called through the network
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(OnIncomingMessage, SNetMsgData);

	// static functions
	static SNetMsgData::ELocation GetLocation();
	static const char*            DbgTranslateLocationToString();
	static void                   Log(const char* szMessage);

	// ------------------------------------------------------------------------
	class CNetStats
	{
		struct SStats
		{
			SStats() { Clear(); }
			void Clear()           { m_max = 0; m_total = 0; m_cntInFrame = 0; }
			void ClearFrame()      { m_cntInFrame = 0; }
			void Add(uint32 value) { m_total += value; m_cntInFrame += value; m_max = m_cntInFrame > m_max ? m_cntInFrame : m_max; }

			uint32 m_cntInFrame, m_max;
			uint64 m_total;
		};

	public:
		enum ETypes { eT_Sent = 0, eT_Received };
		CNetStats() { ClearStats(); }

		void          Add(ETypes type, uint32 value);
		void          ClearFrameStats()    { m_sent.ClearFrame(); }
		void          ClearStats()         { m_sent.Clear(); }
		const SStats& GetSentStats() const { return m_sent; }

	private:
		SStats m_sent;
	};

private:
	struct CVars
	{
		static int  net_netMsgDispatcherDebug;
		static int  net_netMsgDispatcherLogging;
		static int  net_netMsgDispatcherWarnThreshold;
		static void Register();
		static void UnRegister();
		static void ClearStatsCommand(IConsoleCmdArgs* pArgs);
	};

	enum ENetMsgSrc { eNMS_Local = 0, eNMS_Network };
	void DispatchQueuedMessagesToLocalListeners();
	void AddReceivedNetMsgToQueue(const SNetMsgData& msg, ENetMsgSrc src);
	void DebugDraw();

	typedef CListenerSet<INetMsgListener*> TNetMsgListener;
	TNetMsgListener                m_listeners;
	std::vector<SNetMsgData>       m_receivedNetMsgQueue;
	std::vector<SNetMsgData>       m_copyOfReceivedNetMsgQueue;

	CryCriticalSectionNonRecursive m_criticalSectionReceivedQueue;
	CNetStats                      m_stats;
};

#endif
