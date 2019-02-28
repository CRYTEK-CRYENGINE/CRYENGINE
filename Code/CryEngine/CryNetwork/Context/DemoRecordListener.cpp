// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Config.h"

#if INCLUDE_DEMO_RECORDING

#include "DemoRecordListener.h"
#include "NetContext.h"
#include <CrySystem/ITimer.h>
#include "Streams/CompressingStream.h"
#include "DemoDefinitions.h"
#include "Context/ServerContextView.h"
#include <CryGame/IGameFramework.h>

static const uint32 EventsNormal =
  eNOE_BindObject |
  eNOE_UnbindObject |
  eNOE_ObjectAspectChange |
  eNOE_ReconfiguredObject |
  eNOE_ChangeContext |
  eNOE_ContextDestroyed |
  eNOE_SetAspectProfile |
  eNOE_SyncWithGame_Start |
  eNOE_InGame;

const CDemoRecordListener::CDemoRecorderChannel::SScriptConfig CDemoRecordListener::CDemoRecorderChannel::s_defaultConfig =
{
	{
		CDemoRecordListener::CDemoRecorderChannel::RMI_ReliableOrdered,
		CDemoRecordListener::CDemoRecorderChannel::RMI_ReliableUnordered,
		CDemoRecordListener::CDemoRecorderChannel::RMI_UnreliableOrdered,
		NULL,
		// must be last
		CDemoRecordListener::CDemoRecorderChannel::RMI_Attachment
	}
};

CDemoRecordListener::CDemoRecorderChannel::CDemoRecorderChannel(CDemoRecordListener* pParent)
	: m_connected(true)
	, m_pGameChannel(nullptr)
	, m_pParent(pParent)
	, m_channelMask(0)
	, m_config(s_defaultConfig)
{
	string connectionString = pParent->m_pContext->GetGameContext()->GetConnectionString(NULL, true);

	// is there a more direct way to get the server nub???
	m_pGameChannel = static_cast<CNetNub*>(gEnv->pGameFramework->GetServerNetNub())->GetGameNub()->CreateChannel(this, connectionString.c_str()).pChannel;
}

CDemoRecordListener::CDemoRecorderChannel::~CDemoRecorderChannel()
{
	SAFE_RELEASE(m_pGameChannel);
}

void CDemoRecordListener::CDemoRecorderChannel::SendMsg(INetMessage* pMessage)
{
	m_pParent->SendMessage(pMessage);
}

bool CDemoRecordListener::CDemoRecorderChannel::AddSendable(INetSendablePtr pSendable, int numAfterHandle, const SSendableHandle* afterHandle, SSendableHandle* handle)
{
	m_pParent->SendMessage(pSendable);
	return true;
}

void CDemoRecordListener::CDemoRecorderChannel::DispatchRMI(IRMIMessageBodyPtr pBody)
{
	class CRMIMessage:public INetBaseSendable
	{
	public:
		CRMIMessage(IRMIMessageBodyPtr pBody) : m_pBody(pBody) {}

		EMessageSendResult Send(INetSender* pSender)
		{
			EntityId objId = m_pBody->objId;
			//pSender->BeginMessage(m_pBody->pMessageDef);

			string desc = m_pBody->pMessageDef->description;
			pSender->ser.Value("CppRMI", desc);
			pSender->ser.Value("RMI:obj", objId, 'eid');

			m_pBody->SerializeWith(pSender->ser);

			return eMSR_SentOk;
		}

		void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate bAck)
		{
			//m_pBody->Complete();
		}

		size_t GetSize()
		{
			return sizeof(*this) + m_pBody->GetSize();
		}

	private:
		IRMIMessageBodyPtr m_pBody;
	};

	class CRMIMessageScript:public INetBaseSendable
	{
	public:
		CRMIMessageScript(IRMIMessageBodyPtr pBody, const SNetMessageDef* pDef) : m_pBody(pBody), m_pDef(pDef) {}

		EMessageSendResult Send(INetSender* pSender)
		{
			EntityId objId = m_pBody->objId;
			uint8 funcId   = m_pBody->funcId;

			string desc = m_pDef->description;
			pSender->ser.Value("ScriptRMI", desc);
			pSender->ser.Value("RMI:obj", objId, 'eid');
			pSender->ser.Value("RMI:func", funcId);
			m_pBody->SerializeWith(pSender->ser);

			return eMSR_SentOk;
		}

		void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate bAck)
		{
			//m_pBody->Complete();
		}

		size_t GetSize()
		{
			return sizeof(*this) + m_pBody->GetSize();
		}

	private:
		IRMIMessageBodyPtr m_pBody;
		const SNetMessageDef* m_pDef;
	};

	if (pBody->pMessageDef)
	{
		CRMIMessage msg(pBody);
		m_pParent->SendMessage(&msg);
	}
	else
	{
		CRMIMessageScript msg(pBody, m_config.pRMIMsgs[pBody->reliability]);
		m_pParent->SendMessage(&msg);
	}
}

void CDemoRecordListener::CDemoRecorderChannel::DefineProtocol(IProtocolBuilder* pBuilder)
{
	pBuilder->AddMessageSink(this, CDemoRecordListener::CDemoRecorderChannel::GetProtocolDef(), CDemoRecordListener::CDemoRecorderChannel::GetProtocolDef());
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CDemoRecordListener::CDemoRecorderChannel, RMI_UnreliableOrdered, eNRT_UnreliableOrdered, eMPF_BlocksStateChange)
{
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CDemoRecordListener::CDemoRecorderChannel, RMI_ReliableUnordered, eNRT_ReliableUnordered, eMPF_BlocksStateChange)
{
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CDemoRecordListener::CDemoRecorderChannel, RMI_ReliableOrdered, eNRT_ReliableOrdered, eMPF_BlocksStateChange)
{
	return true;
}

NET_IMPLEMENT_IMMEDIATE_MESSAGE(CDemoRecordListener::CDemoRecorderChannel, RMI_Attachment, eNRT_UnreliableUnordered, eMPF_BlocksStateChange)
{
	return true;
}

static std::unique_ptr<CSimpleOutputStream> CreateOutput(const char* filename)
{
	std::unique_ptr<CCompressingOutputStream> stream(new CCompressingOutputStream());

	string path = string(NDemo::TopLevelDemoFilesFolder) + filename;

	char buf[128];
	const SFileVersion& ver = gEnv->pSystem->GetFileVersion();
	cry_sprintf(buf, "_Build%04d_", ver.v[0]);
	path += buf;

	time_t ltime;
	time(&ltime);
	tm* today = localtime(&ltime);
	strftime(buf, 128, "%Y%m%d_%H%M%S", today);
	path += buf;

	if (!stream->Init(path.c_str()))
		stream.reset();
	return std::move(stream);
}

#pragma warning(push)
#pragma warning(disable:4355) //'this' : used in base member initializer list
CDemoRecordListener::CDemoRecordListener(CNetContext* pContext, const char* filename) :
	m_pContext(pContext), m_output(CreateOutput(filename)), m_channel(this)
#pragma warning(pop)
{
	//currently only the compressed version is implemented
	if (!m_output.get())
	{
		NET_ASSERT(m_output.get());
		NetWarning("[demo] Unable to open %s for writing", filename);
	}

	m_bInGame = false;
	m_bIsDead = false;

	m_pContext->GetCurrentState()->ChangeSubscription(this, EventsNormal);
	//m_pContext->AttachRMILogger( this );

	m_boundObjects.insert(0); // 0 is a bound entity ID

	char version[32];
	gEnv->pSystem->GetProductVersion().ToString(version);
	m_output->Put("version", version);
}

CDemoRecordListener::~CDemoRecordListener()
{
	if (!m_pendingWrites.empty())
		FlushPendingWrites();
}

void CDemoRecordListener::Die()
{
	if (m_bIsDead)
		return;
	m_bIsDead = true;

	if (m_pContext)
		m_pContext->GetCurrentState()->ChangeSubscription(this, 0);
}

void CDemoRecordListener::SendMessage(INetBaseSendable* pSendable, bool immediate)
{
	class CDemoMessageSender:public INetSender
	{
	public:
		CDemoMessageSender(TSerialize& ser, CSimpleOutputStream* pOut) : INetSender(ser, 0, 0, 0, true), m_pOut(pOut) {}

		void BeginMessage(const SNetMessageDef* pDef)
		{
			m_pOut->Put("NetMessage", pDef->description);
		}
		void BeginUpdateMessage(SNetObjectID id)
		{
			m_pOut->Put("NetMessage", "CClientContextView:BeginUpdateMessage");
			ser.Value("netID", id, 'eid');
		}
		void EndUpdateMessage()
		{
			m_pOut->Put("NetMessage", "CClientContextView:EndUpdateMessage");
		}
		uint32 GetStreamSize()
		{
			return 0;
		}

	private:
		CSimpleOutputStream* m_pOut;
	};

	SCOPED_GLOBAL_LOCK;

	CSimpleMemoryOutputStream output;
	CSimpleOutputStream* pOut = immediate ? m_output.get() : &output;

	CDemoRecordSerializeImpl serImpl(pOut, m_boundObjects);
	CSimpleSerialize<CDemoRecordSerializeImpl> serSimple(serImpl);
	TSerialize ser(&serSimple);

	CDemoMessageSender sender(ser, pOut);
	pSendable->Send(&sender);
	pOut->Put(NDemo::EndOfSerializationBlock, "");

	if (!immediate)
	{
		if (serImpl.GetUnboundEntities().size() == 0)
			output.Serialize(m_output.get());
		else
		{
			output.AppendUnboundEntities(serImpl.GetUnboundEntities());
			m_pendingWrites.push_back(output);
		}
	}

	pSendable->UpdateState(0, eNSSU_Ack);
}

//void CDemoRecordListener::GC_GetEstablishmentOrder()
//{
//	CContextEstablisherPtr pEstablisher = new CContextEstablisher();
//	m_pContext->GetGameContext()->InitChannelEstablishmentTasks(pEstablisher, &m_channel, m_pContext->GetCurrentState()->GetToken());
//	m_pContext->GetCurrentState()->RegisterEstablisher(this, pEstablisher);
//	m_pContext->GetCurrentState()->SetEstablisherState(this, eCVS_InGame);
//}

void CDemoRecordListener::OnObjectEvent(CNetContextState* pState, SNetObjectEvent* pEvent)
{
	SCOPED_GLOBAL_LOCK;

	if (pState != m_pContext->GetCurrentState())
		return;

	if (!m_output.get())
		return;

	switch (pEvent->event)
	{
	case eNOE_BindObject:
		DoBindObject(pEvent->id);
		break;
	case eNOE_UnbindObject:
		DoUnbindObject(pEvent->id);
		break;
	case eNOE_ObjectAspectChange:
		DoObjectAspectChange(pEvent->pChanges);
		break;
	case eNOE_ReconfiguredObject:
		m_output->Put("ReconfiguredObject", pEvent->id);
		break;
	case eNOE_ContextDestroyed:
		m_pContext = NULL;
		break;
	case eNOE_SetAspectProfile:
		DoSetAspectProfile(pEvent->id, pEvent->aspects, pEvent->profile);
		break;
	case eNOE_SyncWithGame_Start:
		DoUpdate();
		break;
	case eNOE_ChangeContext:
		DoChangeContext(pEvent->pNewState);
		break;
	case eNOE_InGame:
		m_bInGame   = true;
		m_startTime = gEnv->pTimer->GetFrameStartTime();
		break;
	}
}

string CDemoRecordListener::GetName()
{
	return "DemoRecordListener";
}

void CDemoRecordListener::DoChangeContext(CNetContextState* pNewState)
{
	//TO_GAME(&CDemoRecordListener::GC_GetEstablishmentOrder, this);
	CContextEstablisherPtr pEstablisher = new CContextEstablisher();
	m_pContext->GetGameContext()->InitChannelEstablishmentTasks(pEstablisher, &m_channel, pNewState->GetToken());
	pNewState->RegisterEstablisher(this, pEstablisher);
	pNewState->SetEstablisherState(this, eCVS_InGame);
	pNewState->ChangeSubscription(this, EventsNormal);
}

void CDemoRecordListener::DoUpdate()
{
	if (!m_bInGame)
		return;

	SCOPED_GLOBAL_LOCK;

	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	// finish last frame... hacky
	//while (!m_toSpawn.empty())
	//{
	//	ActuallyBind( *m_toSpawn.begin() );
	//	m_toSpawn.erase( m_toSpawn.begin() );
	//}

	// write frame header
	m_output->Put("BeginFrame", (gEnv->pTimer->GetFrameStartTime() - m_startTime).GetSeconds());
	m_output->Sync();

	//static std::vector<TChangedObjects::iterator> collections;

	// iterate through the set of changed objects... and update them
	for (TChangedObjects::iterator iter = m_changedObjects.begin(); iter != m_changedObjects.end(); ++iter)
	{
		//const SContextObject * pObject = m_pContext->GetContextObject( iter->first );
		SContextObjectRef object = m_pContext->GetCurrentState()->GetContextObject(iter->first);
		if (!object.main || !object.main->userID)
		{
			//collections.push_back( iter );
			continue;
		}

		DoUpdateObject(object, iter->second);
		//iter->second &= ~m_pContext->RegularlyUpdatedAspects();
		//if (!iter->second)
		//	collections.push_back( iter );
	}

	m_changedObjects.clear();

	//while (!collections.empty())
	//{
	//	m_changedObjects.erase( collections.back() );
	//	collections.pop_back();
	//}
}

void CDemoRecordListener::DoUpdateObject(const SContextObjectRef& obj, NetworkAspectType aspects)
{
	SCOPED_GLOBAL_LOCK;

	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	IGameContext* pGameContext = m_pContext->GetGameContext();

	//if (obj.main->userID == LOCAL_PLAYER_ENTITY_ID)
	//	DEBUG_BREAK;

	CSimpleMemoryOutputStream output;
	CDemoRecordSerializeImpl  serImpl(&output, m_boundObjects);
	CSimpleSerialize<CDemoRecordSerializeImpl> serSimple(serImpl);
	TSerialize ser(&serSimple);

	output.Put("UpdateObject", obj.main->userID);

	for (NetworkAspectID i = 0; i < NumAspects; i++)
	{
		NetworkAspectType aspect = 1 << i;
		if (aspect & aspects)
		{
			uint8 profile = obj.main->vAspectProfiles[i];
			output.Put("BeginAspect", i);
			output.Put(".profile", profile);  // physics only
			pGameContext->SynchObject(obj.main->userID, aspect, profile, ser, false);
			output.Put(NDemo::EndOfSerializationBlock, "");
		}
	}

	output.Put("FinishUpdateObject", "");

	if (m_boundObjects.find(obj.main->userID) == m_boundObjects.end() || serImpl.GetUnboundEntities().size() != 0)
	{
		output.AppendUnboundEntity(obj.main->userID);
		output.AppendUnboundEntities(serImpl.GetUnboundEntities());
		m_pendingWrites.push_back(output);
	}
	else
		output.Serialize(m_output.get());
}

void CDemoRecordListener::DoBindObject(SNetObjectID nid)
{
	// NOTE: must delay the operation to the toGame queue, so SetNetworkParent calls in the fromGame queue gets executed
	// first in CNetwork::SyncWithGame(frameEnd) in the current frame, so parents objects get setup properly when
	// GC_DoBindObject calls in the toGame queue gets executed in CNetwork::SyncWithGame(frameStart) in the next frame!
	TO_GAME(&CDemoRecordListener::GC_DoBindObject, this, nid);
}

void CDemoRecordListener::GC_DoBindObject(SNetObjectID nid)
{
	//SCOPED_GLOBAL_LOCK;

	// NOTE: when the control comes here, the parent object of the network object should have been setup properly
	// we should enforce the dependency that an object should not be recorded spawned until its parent object gets
	// recorded bound first! I am adopting a recursive binding scheme here: we always try to bind the parent object
	// (recursively) before binding (recording) the current object, relying on the fact that NetworkParent heirarcy
	// has been setup properly - when there is a parent object specified, it must get allocated in NetContext already
	// so we can (should be able to) always record parent object before the child object safely.

	//const SContextObject * pObject = m_pContext->GetContextObject( id );
	SContextObjectRef obj = m_pContext->GetCurrentState()->GetContextObject(nid);
	if (!obj.main || !obj.main->userID)
		return;

	//if (obj.main->userID == LOCAL_PLAYER_ENTITY_ID)
	//	DEBUG_BREAK;

	if (m_boundObjects.find(obj.main->userID) != m_boundObjects.end())
		// this object has already been bound, probably bound previously as parent
		return;

	if (obj.main->parent)
		GC_DoBindObject(obj.main->parent);

	if (obj.main->spawnType != eST_Static)
	{
		INetSendableHookPtr pSpawnHook = m_pContext->GetGameContext()->CreateObjectSpawner(obj.main->userID, &m_channel);
		SendMessage(pSpawnHook, true);
	}

	// !!! BindObject messages always get written immediately
	m_output->Put("BindObject", obj.main->userID);
	m_output->Put(".static", obj.main->spawnType == eST_Static);
	m_output->Put(".aspectsEnabled", obj.xtra->nAspectsEnabled);

	m_boundObjects.insert(obj.main->userID);

	DoUpdateObject(obj, obj.xtra->nAspectsEnabled);

	FlushPendingWrites();
}

void CDemoRecordListener::DoUnbindObject(SNetObjectID id)
{
	SCOPED_GLOBAL_LOCK;

	m_changedObjects.erase(id);

	SContextObjectRef obj = m_pContext->GetCurrentState()->GetContextObject(id);
	if (!obj.main)
		return;

	m_boundObjects.erase(obj.main->refUserID);

	m_output->Put("UnbindObject", obj.main->refUserID);
}

//void CDemoRecordListener::DoEstablishedContext()
//{
////	m_output->Put("EstablishedContext", "");
////	CDemoMessageSender sender( m_output.get() );
////	m_pContext->GetGameContext()->ConfigureContext( &sender, false, true );
//}

void CDemoRecordListener::DoObjectAspectChange(const std::pair<SNetObjectID, SNetObjectAspectChange>* pChanges)
{
	SCOPED_GLOBAL_LOCK;

	while (pChanges->first)
	{
		// TODO: resetAspects?
		m_changedObjects[pChanges->first] |= pChanges->second.aspectsChanged;
		pChanges++;
	}
}

void CDemoRecordListener::DoSetAspectProfile(SNetObjectID id, NetworkAspectType aspects, uint8 profile)
{
	SCOPED_GLOBAL_LOCK;

	SContextObjectRef obj = m_pContext->GetCurrentState()->GetContextObject(id);
	if (!obj.main)
		return;

	//if (obj.main->userID == LOCAL_PLAYER_ENTITY_ID)
	//	DEBUG_BREAK;

	bool immediate = m_boundObjects.find(obj.main->userID) != m_boundObjects.end();

	CSimpleMemoryOutputStream output;
	CSimpleOutputStream* pOut = immediate ? m_output.get() : &output;

	pOut->Put("SetAspectProfile", obj.main->userID);
	pOut->Put(".aspects", aspects);  // which is the bit index
	pOut->Put(".profile", profile);

	if (!immediate)
	{
		output.AppendUnboundEntity(obj.main->userID);
		m_pendingWrites.push_back(output);
	}
	// otherwise m_output was used for serialization
}

#endif
