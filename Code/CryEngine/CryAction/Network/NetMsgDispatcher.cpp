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

#include "StdAfx.h"
#include "NetMsgDispatcher.h"
#include <CryRenderer/IRenderer.h> // for debug render
#include "GameClientChannel.h"
#include "GameClientNub.h"
#include "GameServerChannel.h"
#include "GameServerNub.h"

// ------------------------------------------------------------------------
// Local CVARS
// ------------------------------------------------------------------------
int CNetMessageDistpatcher::CVars::net_netMsgDispatcherDebug = 0;
int CNetMessageDistpatcher::CVars::net_netMsgDispatcherLogging = 0;
int CNetMessageDistpatcher::CVars::net_netMsgDispatcherWarnThreshold = 10;

void CNetMessageDistpatcher::CVars::Register()
{
	REGISTER_CVAR_DEV_ONLY(net_netMsgDispatcherDebug, net_netMsgDispatcherDebug, VF_DEV_ONLY, "Enables (1) / Disables (0) the debug render for CNetMessageDistpatcher");
	REGISTER_CVAR_DEV_ONLY(net_netMsgDispatcherLogging, net_netMsgDispatcherLogging, VF_DEV_ONLY, "Enables (1) / Disables (0) console logging for CNetMessageDistpatcher");
	REGISTER_CVAR_DEV_ONLY(net_netMsgDispatcherWarnThreshold, net_netMsgDispatcherWarnThreshold, VF_DEV_ONLY, "If net_netMsgDispatcherDebug is enabled, the debug render will warn if in a frame the number messages dispatched by CNetMessageDistpatcher is bigger than net_netMsgDispatcherWarnThreshold");
	REGISTER_COMMAND_DEV_ONLY("net_netMsgDispatcherClearStats", &ClearStatsCommand, VF_DEV_ONLY, "Clears the stats gathered for CNetMessageDistpatcher");
}

void CNetMessageDistpatcher::CVars::UnRegister()
{
#if !defined(_RELEASE)
	if (IConsole* pConsole = gEnv->pConsole)
	{
		pConsole->RemoveCommand("net_netMsgDispatcherClearStats");
		pConsole->UnregisterVariable("net_netMsgDispatcherDebug", true);
		pConsole->UnregisterVariable("net_netMsgDispatcherLogging", true);
		pConsole->UnregisterVariable("net_netMsgDispatcherWarnThreshold", true);
	}
#endif
}

void CNetMessageDistpatcher::CVars::ClearStatsCommand(IConsoleCmdArgs* pArgs)
{
	if (CNetMessageDistpatcher* pNetMsgDispatcher = CCryAction::GetCryAction()->GetNetMessageDispatcher())
	{
		pNetMsgDispatcher->ClearStatsCommand();
	}
}

// ------------------------------------------------------------------------
// NetMsgDispatcher
// ------------------------------------------------------------------------

CNetMessageDistpatcher::CNetMessageDistpatcher() :
	m_listeners(8) // reserve some space for listeners
{
	CVars::Register();
}

// ------------------------------------------------------------------------
CNetMessageDistpatcher::~CNetMessageDistpatcher()
{
	CVars::UnRegister();
}

// ------------------------------------------------------------------------
void CNetMessageDistpatcher::DefineProtocol(IProtocolBuilder* pBuilder)
{
	pBuilder->AddMessageSink(this, CNetMessageDistpatcher::GetProtocolDef(), CNetMessageDistpatcher::GetProtocolDef());
}

// ------------------------------------------------------------------------
void CNetMessageDistpatcher::AddReceivedNetMsgToQueue(const SNetMsgData& msg, ENetMsgSrc src)
{
	{
		CryAutoLock<CryCriticalSectionNonRecursive> lock(m_criticalSectionReceivedQueue);
		m_receivedNetMsgQueue.push_back(msg);
	}

	if (src == eNMS_Network)
	{
		m_stats.Add(CNetStats::eT_Received, 1);
	}

	NET_MSG_DISPATCHER_LOG("[AddReceivedNetMsgToQueue]: %s received new message [%s->%s]-[%s/%s]",
	                       DbgTranslateLocationToString(), msg.DbgTranslateSourceToString(), msg.DbgTranslateTargetToString(), msg.key.c_str(), msg.value.c_str());
}

// ------------------------------------------------------------------------
// Functions exposed through the network
// ------------------------------------------------------------------------
NET_IMPLEMENT_SIMPLE_ATSYNC_MESSAGE(CNetMessageDistpatcher, OnIncomingMessage, eNRT_ReliableOrdered, eMPF_NoSendDelay)
{
	// Get the messages sent over the network and add them to the queue
	AddReceivedNetMsgToQueue(param, eNMS_Network);
	return true;
}

// ------------------------------------------------------------------------
void CNetMessageDistpatcher::SendMessage(const SNetMsgData& msg)
{
	uint32 cntDispatchedMsgs = 0;

	CRY_ASSERT_MESSAGE(msg.tgt != SNetMsgData::eT_Invalid, "Invalid message target");

	// Create network message
	INetSendablePtr netMsg = new CSimpleNetMessage<SNetMsgData>(msg, CNetMessageDistpatcher::OnIncomingMessage);
	const SNetMsgData::ELocation whereAmI = GetLocation();
	switch (whereAmI)
	{
	case SNetMsgData::eL_Client:
		{
			CGameClientNub* pClientNub = CCryAction::GetCryAction()->GetGameClientNub();
			// Dispatch message to the server
			if (pClientNub)
			{
				if (CGameClientChannel* pGCC = pClientNub->GetGameClientChannel())
				{
					++cntDispatchedMsgs;

					pGCC->GetNetChannel()->AddSendable(netMsg, 0, NULL, NULL);
					NET_MSG_DISPATCHER_LOG("[DispatchNetMessages]: %s dispatched [%s->%s]-[%s/%s] to Server",
					                       DbgTranslateLocationToString(), msg.DbgTranslateSourceToString(), msg.DbgTranslateTargetToString(), msg.key.c_str(), msg.value.c_str());
				}

				if (msg.tgt == SNetMsgData::eT_All)
				{
					// loop back and make local listeners aware of this message
					NET_MSG_DISPATCHER_LOG("[DispatchNetMessages]: %s loop back [%s->%s]-[%s/%s]",
					                       DbgTranslateLocationToString(), msg.DbgTranslateSourceToString(), msg.DbgTranslateTargetToString(), msg.key.c_str(), msg.value.c_str());

					AddReceivedNetMsgToQueue(msg, eNMS_Local);
				}
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_WARNING, "[CNetMessageDistpatcher::DispatchNetMessages] ClientNub == NULL");
			}
		}
		break;
	case SNetMsgData::eL_Server: // intentional fall through
	case SNetMsgData::eL_DedicatedServer:
		{
			// send message to all clients
			if (CGameServerNub* pServerNub = CCryAction::GetCryAction()->GetGameServerNub())
			{
				if (TServerChannelMap* m = pServerNub->GetServerChannelMap())
				{
					for (TServerChannelMap::iterator iter = m->begin(); iter != m->end(); ++iter)
					{
						if (INetChannel* pNetChannel = iter->second->GetNetChannel()) //channel can be on hold thus not having net channel currently
						{
							++cntDispatchedMsgs;

							pNetChannel->AddSendable(netMsg, 0, NULL, NULL);

							NET_MSG_DISPATCHER_LOG("[DispatchNetMessages]: %s dispatched [%s->%s]-[%s/%s] to Client",
							                       DbgTranslateLocationToString(), msg.DbgTranslateSourceToString(), msg.DbgTranslateTargetToString(), msg.key.c_str(), msg.value.c_str());
						}
					}
				}
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_WARNING, "[CNetMessageDistpatcher::DispatchNetMessages] pServerNub == NULL");
			}
		}
		break;
	default:
		CRY_ASSERT(0);
	}

	m_stats.Add(CNetStats::eT_Sent, cntDispatchedMsgs);

	NET_MSG_DISPATCHER_LOG("[SendMessage]: %s queued message [%s->%s]-[%s/%s]",
	                       DbgTranslateLocationToString(), msg.DbgTranslateSourceToString(), msg.DbgTranslateTargetToString(), msg.key.c_str(), msg.value.c_str());
}

// ------------------------------------------------------------------------
void CNetMessageDistpatcher::DispatchQueuedMessagesToLocalListeners()
{
	if (m_receivedNetMsgQueue.empty() == true)
		return;

	m_criticalSectionReceivedQueue.Lock();
	m_copyOfReceivedNetMsgQueue.clear();
	m_copyOfReceivedNetMsgQueue.swap(m_receivedNetMsgQueue);
	m_criticalSectionReceivedQueue.Unlock();

	// We have received messages over the network. It is time to dispatch them to our local listeners
	for (size_t i = 0, iSize = m_copyOfReceivedNetMsgQueue.size(); i < iSize; ++i)
	{
		const SNetMsgData& msg = m_copyOfReceivedNetMsgQueue[i];
		for (TNetMsgListener::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
		{
			notifier->OnMessage(msg);
			NET_MSG_DISPATCHER_LOG("[DispatchQueuedMessagesToLocalListeners]: %s call [%s->%s] OnMessage(%s/%s) on Listener",
			                       DbgTranslateLocationToString(), msg.DbgTranslateSourceToString(), msg.DbgTranslateTargetToString(), msg.key.c_str(), msg.value.c_str());
		}
	}

}

// ------------------------------------------------------------------------
void CNetMessageDistpatcher::RegisterListener(INetMsgListener* pListener, const char* name)
{
	const bool ok = m_listeners.Add(pListener, name);
	NET_MSG_DISPATCHER_LOG("[RegisterListener]: %s Registering listener %s [total:%" PRISIZE_T "] - %s", DbgTranslateLocationToString(), name ? name : "[unnamed]", m_listeners.ValidListenerCount(), ok ? "OK" : "ALREADY REGISTERED");
}

// ------------------------------------------------------------------------
void CNetMessageDistpatcher::UnRegisterListener(INetMsgListener* pListener)
{
	m_listeners.Remove(pListener);
	NET_MSG_DISPATCHER_LOG("[UnRegisterListener]: %s removing listener [total:%" PRISIZE_T "]", DbgTranslateLocationToString(), m_listeners.ValidListenerCount());
}

// ------------------------------------------------------------------------
void CNetMessageDistpatcher::ClearListeners()
{
	m_listeners.Clear();
	NET_MSG_DISPATCHER_LOG("[CNetMessageDistpatcher]: %s ClearListeners", DbgTranslateLocationToString());
}

// ------------------------------------------------------------------------
void CNetMessageDistpatcher::Update()
{
	// Send messages received through the network to listeners
	DispatchQueuedMessagesToLocalListeners();

	// Stats
	if (CVars::net_netMsgDispatcherDebug)
	{
		DebugDraw();
		m_stats.ClearFrameStats();
	}
}

// ------------------------------------------------------------------------
/* static */ SNetMsgData::ELocation CNetMessageDistpatcher::GetLocation()
{
	return gEnv->bServer ? SNetMsgData::eL_Server :
	       gEnv->IsDedicated() ? SNetMsgData::eL_DedicatedServer :
	       gEnv->IsClient() ? SNetMsgData::eL_Client :
	       SNetMsgData::eL_Unknown;
}

// ------------------------------------------------------------------------
/* static */ const char* CNetMessageDistpatcher::DbgTranslateLocationToString()
{
	SNetMsgData msg(GetLocation(), SNetMsgData::eT_Default, "", "");
	return msg.DbgTranslateSourceToString();
}

// ------------------------------------------------------------------------
/* static */ void CNetMessageDistpatcher::Log(const char* szMessage)
{
	if (CVars::net_netMsgDispatcherLogging == 0)
	{
		return;
	}

	CryLog("%s", szMessage);
}

// ------------------------------------------------------------------------
void CNetMessageDistpatcher::DebugDraw()
{
	const float xPosLabel = 10.f, xPosData = 10.f, yPos = 80.f, yDelta = 20.f;
	float y = yPos;
	const ColorF fColorLabel(1.0f, 1.0f, 1.0f, 1.0f);
	const ColorF fColorData(1.0f, 1.0f, 0.0f, 1.0f);
	const ColorF fColorWarning(1.0f, 0.0f, 0.0f, 1.0f);

	const float kFontSize = m_stats.GetSentStats().m_max <= CVars::net_netMsgDispatcherWarnThreshold ? 1.3f : 1.7f;
	const ColorF& fColor = m_stats.GetSentStats().m_max <= CVars::net_netMsgDispatcherWarnThreshold ? fColorData : fColorWarning;
	IRenderAuxText::Draw2dLabel(xPosLabel, y, 1.3f, fColorLabel, false, "NetMsg Stats (Total: %" PRIu64 " / Local List.: %" PRISIZE_T ")", m_stats.GetSentStats().m_total, m_listeners.ValidListenerCount());
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, kFontSize, fColor, false, "Max frame cnt: %u/%d", m_stats.GetSentStats().m_max, CVars::net_netMsgDispatcherWarnThreshold);
}

// ------------------------------------------------------------------------
void CNetMessageDistpatcher::CNetStats::Add(ETypes type, uint32 value)
{
	if (CVars::net_netMsgDispatcherDebug)
	{
		switch (type)
		{
		case eT_Sent:
			{
				m_sent.Add(value);
			} break;
		case eT_Received:
			{
				/* not needed for the moment */ } break;
		default:
			CRY_ASSERT(0);
		}
	}
}
