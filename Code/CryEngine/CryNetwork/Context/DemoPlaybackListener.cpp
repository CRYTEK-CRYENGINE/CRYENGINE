// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Config.h"

#if INCLUDE_DEMO_RECORDING

	#include "Context/ContextView.h"
	#include "DemoPlaybackListener.h"
	#include "Streams/CompressingStream.h"
	#include "DemoDefinitions.h"
	#include "NetContext.h"
	#include "Protocol/NetChannel.h"
	#include <CrySystem/ITimer.h>
	#include <CryNetwork/SimpleSerialize.h>
	#include <CryEntitySystem/IEntitySystem.h>
	#include <CryString/StringUtils.h>
	#include <CryString/StringUtils.h>
	#include <CryGame/IGameFramework.h>     // LOCAL_PLAYER_ENTITY_ID

static const uint32 Events =
  eNOE_ChangeContext |
  eNOE_ContextDestroyed |
  eNOE_SyncWithGame_Start |
  eNOE_InGame;

bool DecodeString(const char* buf, SNetObjectID& value)
{
	int id, salt;
	if (2 == sscanf(buf, "%d:%d", &id, &salt) && id < 65536 && salt < 65536 && id >= 0 && salt >= 0)
	{
		value.id = id;
		value.salt = salt;
		return true;
	}
	return false;
}

bool DecodeString(const char* buf, SSerializeString& value)
{
	value = buf;
	return true;
}

bool DecodeString(const char* buf, EntityId& value)
{
	value = 0;
	return 1 == sscanf(buf, "%x", &value);
}

bool DecodeString(const char* buf, bool& value)
{
	value = false;
	if (0 == strcmp(buf, "0"))
		value = false;
	else if (0 == strcmp(buf, "1"))
		value = true;
	else
		return false;
	return true;
}

bool DecodeString(const char* buf, float& value)
{
	value = 0.0f;
	return 1 == sscanf(buf, "%f", &value);
}

bool DecodeString(const char* buf, int& value)
{
	value = 0;
	return 1 == sscanf(buf, "%d", &value);
}

	#if defined(_MSC_VER)
bool DecodeString(const char* buf, int64& value)
{
	value = 0;
	return 1 == sscanf(buf, "%I64d", &value);
}

bool DecodeString(const char* buf, uint64& value)
{
	value = 0;
	return 1 == sscanf(buf, "%I64x", &value);
}
	#else
bool DecodeString(const char* buf, int64& value)
{
	long long value_ll = 0;
	if (1 == sscanf(buf, "%lld", &value_ll))
	{
		value = (int64)value_ll;
		return true;
	}
	return false;
}

bool DecodeString(const char* buf, uint64& value)
{
	unsigned long long value_ull = 0;
	if (1 == sscanf(buf, "%llx", &value_ull))
	{
		value = (uint64)value_ull;
		return true;
	}
	return false;
}
	#endif

bool DecodeString(const char* buf, CTimeValue& value)
{
	value = 0.0f;
	float temp;
	bool ok = sscanf(buf, "%f", &temp) != 0;
	if (ok)
		value = temp;
	return ok;
}

bool DecodeString(const char* buf, ScriptAnyValue& value)
{
	return false;
}

	#define KINDA_INT_TYPE(type)                       \
	  bool DecodeString(const char* buf, type & value) \
	  {                                                \
	    int temp = 0;                                  \
	    bool ok = 1 == sscanf(buf, "%d", &temp);       \
	    value = temp;                                  \
	    return ok;                                     \
	  }
KINDA_INT_TYPE(int16);
KINDA_INT_TYPE(uint16);
KINDA_INT_TYPE(int8);
KINDA_INT_TYPE(uint8);

bool DecodeString(const char* buf, Ang3& value)
{
	value = Ang3(0, 0, 0);
	return 3 == sscanf(buf, "%f,%f,%f", &value.x, &value.y, &value.z);
}

bool DecodeString(const char* buf, Vec3& value)
{
	value = Vec3(0, 0, 0);
	return 3 == sscanf(buf, "%f,%f,%f", &value.x, &value.y, &value.z);
}

bool DecodeString(const char* buf, Vec2& value)
{
	value = Vec2(0, 0);
	return 3 == sscanf(buf, "%f,%f", &value.x, &value.y);
}

bool DecodeString(const char* buf, Quat& value)
{
	value = Quat(1, 0, 0, 0);
	return 4 == sscanf(buf, "%f,%f,%f,%f", &value.w, &value.v.x, &value.v.y, &value.v.z);
}

class CDemoPlaybackSerializeImpl : public CSimpleSerializeImpl<true, eST_Network>
{
public:
	CDemoPlaybackSerializeImpl(CSimpleInputStream& input, CDemoPlaybackListener* listener) : m_input(input), m_pListener(listener)
	{
		m_buffer.key[0] = 0;
	}

	~CDemoPlaybackSerializeImpl()
	{
		CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

		// NOTE: this piece of code skips the un-serialized data until the end mark is reached
		// it relies on the fact that serImpl is declared on the stack, so when it destructs while
		// going out of scope, it does the job (see code below)
		if (0 != strcmp(m_buffer.key, NDemo::EndOfSerializationBlock))
		{
			while (const SStreamRecord* pRec = m_input.Next())
			{
				if (0 == strcmp(pRec->key, NDemo::EndOfSerializationBlock))
					break;
			}
		}
	}

	template<class T_Value>
	void ValueImpl(const char* name, T_Value& value)
	{
		CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

		value = T_Value();

		if (!m_buffer.key[0])
		{
			const SStreamRecord* pRec = m_input.Next();
			if (!pRec)
				return;
			m_buffer = *pRec;
		}
		//if (0 == strcmp(m_buffer.key, NDemo::EndOfSerializationBlock))
		//	return;
		if (0 == strcmp(m_buffer.key, name))
		{
			DecodeString(m_buffer.value, value);
			m_buffer.key[0] = 0;
		}
	}
	void ValueImpl(const char* name, XmlNodeRef& value)
	{
	}
	template<class T_Value>
	ILINE void Value(const char* szName, T_Value& value)
	{
		ValueImpl(szName, value);
	}
	template<class T_Value, class T_Policy>
	ILINE void Value(const char* szName, T_Value& value, const T_Policy& policy)
	{
		ValueImpl(szName, value);
	}
	ILINE void Value(const char* szName, EntityId& value, uint32 policy)   //, const NSerPolicy::AC_EntityId& policy )
	{
		//assert(policy == 'eid');
		ValueImpl(szName, value);
		if (policy == 'eid')
			value = m_pListener->MapID(value);
	}

	bool BeginOptionalGroup(const char* szName, bool bCond)
	{
		Value(szName, bCond);
		return bCond;
	}

private:
	CSimpleInputStream&    m_input;
	SStreamRecord          m_buffer;
	CDemoPlaybackListener* m_pListener;
};

CDemoPlaybackListener::TInputHandlerMap CDemoPlaybackListener::m_defaultHandlers;

CDemoPlaybackListener::CDemoPlaybackListener(CNetContext* pContext, const char* filename, CNetChannel* pClientChannel, CNetChannel* pServerChannel)
{
	m_pContext = pContext;
	m_pClientChannel = pClientChannel;
	m_pServerChannel = pServerChannel;
	m_bInGame = false;
	m_bIsDead = false;

	std::auto_ptr<CCompressingInputStream> pComp;
	pComp.reset(new CCompressingInputStream());
	string path = string(NDemo::TopLevelDemoFilesFolder) + filename;
	if (!pComp->Init(path.c_str()))
		pComp.reset();
	else
		m_pInput = pComp;

	if (!m_pInput.get())
	{
		TO_GAME(&CDemoPlaybackListener::GC_Disconnect, this, eDC_DemoPlaybackFileNotFound, string("demo playback file not found"));
		return;
	}

	char version[32];
	gEnv->pSystem->GetProductVersion().ToString(version);
	const SStreamRecord* pRecord = m_pInput->Next();
	/*
	   if ( strcmp(pRecord->key, "version") != 0 || strcmp(pRecord->value, version) != 0 )
	   {
	   m_pInput.reset();
	   TO_GAME(&CDemoPlaybackListener::GC_Disconnect, this, eDC_VersionMismatch, string("demo playback wrong version"));
	   return;
	   }
	 */

	if (m_defaultHandlers.empty())
		InitHandlers();

	m_pState = &m_defaultHandlers;
	m_buffer.key[0] = 0;
	m_startTime = -1.0f;

	m_currentlyBinding = 0;
	m_currentlyBindingStatic = false;

	m_currentProfile = 255;

	m_pContext->GetCurrentState()->ChangeSubscription(this, Events);
	m_pContext->GetCurrentState()->ChangeSubscription(this, pClientChannel, eNCE_ChannelDestroyed);
	m_pContext->GetCurrentState()->ChangeSubscription(this, pServerChannel, eNCE_ChannelDestroyed);
}

CDemoPlaybackListener::~CDemoPlaybackListener()
{
}

void CDemoPlaybackListener::Die()
{
	if (m_bIsDead)
		return;
	m_bIsDead = true;

	if (m_pContext)
		m_pContext->GetCurrentState()->ChangeSubscription(this, 0);
}

//void CDemoPlaybackListener::GC_EnterInGameState()
//{
//	CContextEstablisherPtr pEstablisher = new CContextEstablisher();
//	m_pContext->GetGameContext()->InitChannelEstablishmentTasks(pEstablisher, NULL);
//	m_pContext->RegisterEstablisher(this, pEstablisher);
//	m_pContext->SetEstablisherState(this, eCVS_InGame);
//	SNetChannelEvent e; e.event = eNCE_InGame;
//	m_pContext->OnChannelEvent(NULL, &e);
//}

void CDemoPlaybackListener::OnObjectEvent(CNetContextState* pState, SNetObjectEvent* pEvent)
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	SCOPED_GLOBAL_LOCK;

	if (pState != m_pContext->GetCurrentState())
		return;

	switch (pEvent->event)
	{
	case eNOE_ChangeContext:
		//TO_GAME(&CDemoPlaybackListener::GC_EnterInGameState, this);
		pEvent->pNewState->ChangeSubscription(this, Events);
		break;
	case eNOE_ContextDestroyed:
		m_pContext = NULL;
		break;
	case eNOE_SyncWithGame_Start:
		DoUpdate();
		break;
	case eNOE_InGame:
		m_bInGame = true;
		m_initTime = gEnv->pTimer->GetFrameStartTime();

		m_pContext->GetGameContext()->PassDemoPlaybackMappedOriginalServerPlayer(MapID(LOCAL_PLAYER_ENTITY_ID));
		break;
	}
}

void CDemoPlaybackListener::DoUpdate()
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	NET_ASSERT(m_pState);

	if (!m_pInput.get())
		return;

	while (true)
	{
		if (!m_buffer.key[0])
		{
			const SStreamRecord* pRec = m_pInput->Next();
			if (!pRec)
			{
				TO_GAME(&CDemoPlaybackListener::GC_Disconnect, this, eDC_DemoPlaybackFinished, string("demo playback finished"));
				m_pInput.reset();
				return;
			}
			m_buffer = *pRec;
			NET_ASSERT(m_buffer.key[0]);
		}
		InputHandler handler;
		{
			CRY_PROFILE_REGION(PROFILE_NETWORK, "DoUpdate:Select");
			TInputHandlerMap::const_iterator iter = m_pState->find(CONST_TEMP_STRING(m_buffer.key));
			if (iter == m_pState->end())
				handler = &CDemoPlaybackListener::SkipLineWithWarning;
			else
				handler = iter->second;
		}
		{
			CRY_PROFILE_REGION(PROFILE_NETWORK, "DoUpdate:Dispatch");

			switch ((this->*handler)())
			{
			case eIR_AbortRead:
				TO_GAME(&CDemoPlaybackListener::GC_Disconnect, this, eDC_DemoPlaybackFinished, string("demo playback broken stream"));
				m_pInput.reset();
				m_buffer.key[0] = 0;
				return;
			case eIR_Ok:
				m_buffer.key[0] = 0;
				break;
			case eIR_TryLater:
				return;
			}
			NET_ASSERT(m_buffer.key[0] == 0);
		}
		CheckCurrentlyBinding();
	}
}

void CDemoPlaybackListener::InitHandlers()
{
	m_defaultHandlers["NetMessage"] = &CDemoPlaybackListener::NetMessage;
	m_defaultHandlers["BindObject"] = &CDemoPlaybackListener::BindObject;
	m_defaultHandlers["UnbindObject"] = &CDemoPlaybackListener::UnbindObject;
	m_defaultHandlers["BeginFrame"] = &CDemoPlaybackListener::BeginFrame;
	m_defaultHandlers["UpdateObject"] = &CDemoPlaybackListener::UpdateObject;
	m_defaultHandlers["BeginAspect"] = &CDemoPlaybackListener::BeginAspect;
	m_defaultHandlers["FinishUpdateObject"] = &CDemoPlaybackListener::FinishUpdateObject;
	m_defaultHandlers["CppRMI"] = &CDemoPlaybackListener::CppRMI;
	m_defaultHandlers["ScriptRMI"] = &CDemoPlaybackListener::ScriptRMI;
	m_defaultHandlers["SetAspectProfile"] = &CDemoPlaybackListener::SetAspectProfile;
}

CDemoPlaybackListener::EInputResult CDemoPlaybackListener::SkipLineWithWarning()
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	NetWarning("[demo] Cannot interpret message: '%s' '%s'", m_buffer.key, m_buffer.value);
	return eIR_Ok;
}

CDemoPlaybackListener::EInputResult CDemoPlaybackListener::MessageHandler(EntityId rmiObj)
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	INetMessageSink* pSink = NULL;
	const SNetMessageDef* pDef = NULL;
	INetChannel* pChannel = m_pClientChannel;

	if (!m_pClientChannel->LookupMessage(m_buffer.value, &pDef, &pSink))
	{
		pChannel = m_pServerChannel;
		if (!m_pServerChannel->LookupMessage(m_buffer.value, &pDef, &pSink))
			return SkipLineWithWarning();
	}

	//EContextViewState cvs = pChannel->GetContextViewState();
	//if ( pDef->description == string("CGameClientChannel:DefaultSpawn") && cvs < eCVS_SpawnEntities )
	//	return eIR_TryLater;

	CDemoPlaybackSerializeImpl serImpl(*m_pInput, this);
	CSimpleSerialize<CDemoPlaybackSerializeImpl> ser(serImpl);
	TNetMessageCallbackResult r = pDef->handler(pDef->nUser, pSink, TSerialize(&ser), DEMO_PLAYBACK_SEQ_NUMBER, DEMO_PLAYBACK_SEQ_NUMBER, 0, &rmiObj, pChannel);

	if (!r.first)
	{
		if (r.second)
			r.second->DeleteThis();
		return eIR_AbortRead;
	}
	else if (r.second)
	{
		r.second->Sync();
		r.second->DeleteThis();
	}

	return eIR_Ok;
}

CDemoPlaybackListener::EInputResult CDemoPlaybackListener::NetMessage()
{
	return MessageHandler(0); // m_buffer.key = "NetMessage", m_buffer.value = <description>
}

CDemoPlaybackListener::EInputResult CDemoPlaybackListener::CppRMI()
{
	EntityId rmiObj = 0;
	if (!DecodeValue("RMI:obj", rmiObj))   // m_buffer is not touched, so m_buffer.key = "CppRMI", m_buffer.value = <description>
		return eIR_AbortRead;
	rmiObj = MapID(rmiObj);
	return MessageHandler(rmiObj);
}

CDemoPlaybackListener::EInputResult CDemoPlaybackListener::ScriptRMI()
{
	EntityId rmiObj = 0;
	if (!DecodeValue("RMI:obj", rmiObj))
		return eIR_AbortRead;
	rmiObj = MapID(rmiObj);
	uint8 funcId = 0;
	if (!DecodeValue("RMI:func", funcId))
		return eIR_AbortRead;
	CDemoPlaybackSerializeImpl serImpl(*m_pInput, this);
	CSimpleSerialize<CDemoPlaybackSerializeImpl> ser(serImpl);
	INetAtSyncItem* pAtSyncItem = m_pContext->GetGameContext()->HandleRMI(true, rmiObj, funcId, TSerialize(&ser), m_pClientChannel);
	if (!pAtSyncItem)
		return eIR_AbortRead;
	TO_GAME(pAtSyncItem, m_pClientChannel);
	return eIR_Ok;
}

CDemoPlaybackListener::EInputResult CDemoPlaybackListener::BeginFrame()
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	if (!m_bInGame)
		return eIR_TryLater;

	if (m_startTime < 0.0f)
	{
		if (!DecodeString(m_buffer.value, m_startTime))
			return eIR_AbortRead;
	}

	ITimer* pTimer = gEnv->pTimer;
	float time = (pTimer->GetFrameStartTime() - m_initTime).GetSeconds();

	if (time < m_startTime)
		return eIR_TryLater;

	m_startTime = -1.0f;
	return eIR_Ok;
}

CDemoPlaybackListener::EInputResult CDemoPlaybackListener::BindObject()
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	if (m_pClientChannel->GetContextViewState() < eCVS_SpawnEntities)
		return eIR_TryLater;

	if (!DecodeString(m_buffer.value, m_currentlyBinding))
		return eIR_AbortRead;

	bool bStatic = false;
	NetworkAspectType aspectsEnabled = 0;

	if (!DecodeValue(".static", bStatic))
		return eIR_AbortRead;
	if (!DecodeValue(".aspectsEnabled", aspectsEnabled))
		return eIR_AbortRead;

	//NetLog("[demo] BindObject: nid=%04x, eid=%08x, static=%s, aspects=%02x", id, m_currentlyBinding, bStatic?"true":"false", aspectsEnabled);

	m_currentlyBindingStatic = bStatic;

	return eIR_Ok;
}

CDemoPlaybackListener::EInputResult CDemoPlaybackListener::UnbindObject()
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	EntityId eid = 0;
	if (!DecodeString(m_buffer.value, eid))
		return eIR_AbortRead;

	std::map<EntityId, EntityId>::iterator it = m_recIdToPlayId.find(eid);
	if (it != m_recIdToPlayId.end())
	{
		m_pContext->GetGameContext()->UnboundObject(it->second);
		m_recIdToPlayId.erase(it);
	}

	return eIR_Ok;
}

CDemoPlaybackListener::EInputResult CDemoPlaybackListener::UpdateObject()
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	EntityId id = 0;
	if (!DecodeString(m_buffer.value, id))
		return eIR_AbortRead;
	m_currentlyUpdating = MapID(id); //stl::find_in_map( m_recIdToPlayId, id, 0x20000 );

	//NetLog("[demo] UpdateObject: original=%08x, current=%08x", id, m_currentlyUpdating);

	return eIR_Ok;
}

void CDemoPlaybackListener::CheckCurrentlyBinding()
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	if (m_currentlyBinding == 0)
		return;

	//if (m_currentlyBinding == LOCAL_PLAYER_ENTITY_ID)
	//	DEBUG_BREAK;

	//NetLog("[demo] CheckCurrentlyBinding: static=%s, currentlyBinding=%08x, spawnedObject=%08x",
	//	m_currentlyBindingStatic?"true":"false", m_currentlyBinding, m_pContext->GetSpawnedObjectId(false));

	if (m_currentlyBindingStatic)
	{
		m_recIdToPlayId[m_currentlyBinding] = m_currentlyBinding;
	}
	else
	{
		EntityId boundId = 0;
		if (m_currentlyBinding && (boundId = m_pContext->GetCurrentState()->GetSpawnedObjectId(true)))
			m_recIdToPlayId[m_currentlyBinding] = boundId;
	}

	m_currentlyBinding = 0;
	m_currentlyBindingStatic = false;
}

CDemoPlaybackListener::EInputResult CDemoPlaybackListener::BeginAspect()
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	NetworkAspectID aspectIdx;
	if (!DecodeString(m_buffer.value, aspectIdx))
		return eIR_AbortRead;

	if (!DecodeValue(".profile", m_currentProfile))
		return eIR_AbortRead;

	CDemoPlaybackSerializeImpl serImpl(*m_pInput, this);
	CSimpleSerialize<CDemoPlaybackSerializeImpl> ser(serImpl);
	if (m_currentlyUpdating)
		m_pContext->GetGameContext()->SynchObject(m_currentlyUpdating, 1 << aspectIdx, m_currentProfile, TSerialize(&ser), false);

	return eIR_Ok;
}

string CDemoPlaybackListener::GetName()
{
	return "DemoPlaybackListener";
}

CDemoPlaybackListener::EInputResult CDemoPlaybackListener::FinishUpdateObject()
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	m_currentlyUpdating = 0;
	m_currentProfile = 255;
	return eIR_Ok;
}

CDemoPlaybackListener::EInputResult CDemoPlaybackListener::SetAspectProfile()
{
	EntityId recorded = 0;
	if (!DecodeString(m_buffer.value, recorded))
		return eIR_AbortRead;

	NetworkAspectID aspectIdx = 0;
	uint8 profile = 255;

	if (!DecodeValue(".aspects", aspectIdx))   // which is the bit index
		return eIR_AbortRead;
	if (!DecodeValue(".profile", profile))
		return eIR_AbortRead;

	//if (recorded == LOCAL_PLAYER_ENTITY_ID)
	//	DEBUG_BREAK;

	EntityId spawned = MapID(recorded); //stl::find_in_map( m_recIdToPlayId, recorded, 0x20000 );
	if (spawned)
		m_pContext->SetAspectProfile(spawned, 1 << aspectIdx, profile);

	return eIR_Ok;
}

void CDemoPlaybackListener::GC_Disconnect(EDisconnectionCause cause, string description)
{
	NetLog("[demo] demo playback stopped - %s", description.c_str());
	m_pServerChannel->Disconnect(cause, description.c_str());
}

template<typename T>
bool CDemoPlaybackListener::DecodeValue(const char* name, T& value, bool peek)
{
	if (const SStreamRecord* pRec = m_pInput->Next(peek))
		if (0 == strcmp(pRec->key, name))
			return DecodeString(pRec->value, value);
	return false;
}

#endif
