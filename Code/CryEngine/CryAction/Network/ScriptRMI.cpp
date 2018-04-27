// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Provides remote method invocation to script

   -------------------------------------------------------------------------
   History:
   - 30:11:2004   11:30 : Created by Craig Tiller

*************************************************************************/
#include "StdAfx.h"
#include "ScriptRMI.h"
#include "ScriptSerialize.h"
#include "GameContext.h"
#include "GameClientNub.h"
#include "GameServerNub.h"
#include "GameClientChannel.h"
#include "GameServerChannel.h"
#include "CryAction.h"
#include <CrySystem/IConsole.h>

#define FLAGS_FIELD           "__sendto"
#define ID_FIELD              "__id"
#define CLIENT_DISPATCH_FIELD "__client_dispatch"
#define SERVER_DISPATCH_FIELD "__server_dispatch"
#define VALIDATED_FIELD       "__validated"
#define SERIALIZE_FUNCTION    "__serialize"
#define SERVER_SYNCHED_FIELD  "__synched"
#define HIDDEN_FIELD          "__hidden"

static ICVar* pLogRMIEvents = NULL;
static ICVar* pDisconnectOnError = NULL;

CScriptRMI* CScriptRMI::m_pThis = NULL;

// MUST be plain-old-data (no destructor called)
struct CScriptRMI::SSynchedPropertyInfo
{
	char                 name[MaxSynchedPropertyNameLength + 1];
	EScriptSerializeType type;
};

// MUST be plain-old-data (see CScriptTable::AddFunction)
struct CScriptRMI::SFunctionInfo
{
	char                format[MaxRMIParameters + 1];
	ENetReliabilityType reliability;
	ERMIAttachmentType  attachment;
	uint8               funcID;
};

struct SSerializeFunctionParams
{
	SSerializeFunctionParams(TSerialize s, IEntity* p) : ser(s), pEntity(p) {}
	TSerialize ser;
	IEntity*   pEntity;
};

// this class implements the body of a remote method invocation message
class CScriptRMI::CScriptMessage : public IRMIMessageBody, public CScriptRMISerialize
{
public:
	CScriptMessage(
	  ENetReliabilityType reliability,
	  ERMIAttachmentType attachment,
	  EntityId objId,
	  uint8 funcId,
	  const char* fmt) :
		IRMIMessageBody(reliability, attachment, objId, funcId, NULL, 0, 0),
		CScriptRMISerialize(fmt),
		m_refs(0)
	{
	}

	void SerializeWith(TSerialize pSerialize)
	{
		CScriptRMISerialize::SerializeWith(pSerialize);
	}

	int8 GetRefs()
	{
		return m_refs;
	}

	size_t GetSize()
	{
		return sizeof(*this);
	}

#if ENABLE_RMI_BENCHMARK
	virtual const SRMIBenchmarkParams* GetRMIBenchmarkParams()
	{
		return NULL;
	}
#endif

private:
	int8 m_refs;
};

CScriptRMI::CScriptRMI() :
	m_pParent(0)
{
	m_pThis = this;
}

bool CScriptRMI::Init()
{
	/*
	   CryAutoCriticalSection lkDispatch(m_dispatchMutex);

	   IScriptSystem * pSS = gEnv->pScriptSystem;
	   IEntityClassRegistry * pReg = gEnv->pEntitySystem->GetClassRegistry();
	   IEntityClass * pClass = NULL;
	   for (pReg->IteratorMoveFirst(); pClass = pReg->IteratorNext();)
	   {
	   pClass->LoadScript(false);
	   if (IEntityScript * pEntityScript = pClass->GetIEntityScript())
	   {
	    if (m_entityClassToEntityTypeID.find(pClass->GetName()) != m_entityClassToEntityTypeID.end())
	    {
	      SmartScriptTable entityTable;
	      pSS->GetGlobalValue( pClass->GetName(), entityTable );
	      if (!!entityTable)
	      {
	        //ValidateDispatchTable( pClass->GetName(), , , false );
	        //ValidateDispatchTable( pClass->GetName(), , , true );
	      }
	    }
	   }
	   }
	 */
	return true;
}

void CScriptRMI::UnloadLevel()
{
	CryAutoLock<CryCriticalSection> lock(m_dispatchMutex);
	m_entities.clear();
	m_entityClassToEntityTypeID.clear();

	const size_t numDispatches = m_dispatch.size();
	for (size_t i = 0; i < numDispatches; ++i)
	{
		SDispatch& dispatch = m_dispatch[i];
		if (dispatch.m_serverDispatchScriptTable)
		{
			dispatch.m_serverDispatchScriptTable->SetValue(VALIDATED_FIELD, false);
		}
		if (dispatch.m_clientDispatchScriptTable)
		{
			dispatch.m_clientDispatchScriptTable->SetValue(VALIDATED_FIELD, false);
		}
	}

	stl::free_container(m_dispatch);
}

void CScriptRMI::SetContext(CGameContext* pContext)
{
	m_pParent = pContext;
}

void CScriptRMI::RegisterCVars()
{
	pLogRMIEvents = REGISTER_INT(
	  "net_log_remote_methods", 0, VF_DUMPTODISK,
	  "Log remote method invocations");
	pDisconnectOnError = REGISTER_INT(
	  "net_disconnect_on_rmi_error", 0, VF_DUMPTODISK,
	  "Disconnect remote connections on script exceptions during RMI calls");
}

void CScriptRMI::UnregisterCVars()
{
	SAFE_RELEASE(pLogRMIEvents);
	SAFE_RELEASE(pDisconnectOnError);
}

// implementation of Net.Expose() - exposes a class
int CScriptRMI::ExposeClass(IFunctionHandler* pFH)
{
	SmartScriptTable params, cls, clientMethods, serverMethods;
	SmartScriptTable clientTable, serverTable;
	SmartScriptTable serverProperties;

	IScriptSystem* pSS = pFH->GetIScriptSystem();

	if (pFH->GetParamCount() != 1)
	{
		pSS->RaiseError("Net.Expose takes only one parameter - a table");
		return pFH->EndFunction(false);
	}

	if (!pFH->GetParam(1, params))
	{
		pSS->RaiseError("Net.Expose takes only one parameter - a table");
		return pFH->EndFunction(false);
	}
	if (!params->GetValue("Class", cls))
	{
		pSS->RaiseError("No 'Class' parameter to Net.Expose");
		return pFH->EndFunction(false);
	}

	if (!params->GetValue("ClientMethods", clientMethods))
	{
		GameWarning("No 'ClientMethods' parameter to Net.Expose");
	}
	else if (!cls->GetValue("Client", clientTable))
	{
		pSS->RaiseError("'ClientMethods' exposed, but no 'Client' table in class");
		return pFH->EndFunction(false);
	}
	if (!params->GetValue("ServerMethods", serverMethods))
	{
		GameWarning("No 'ServerMethods' parameter to Net.Expose");
	}
	else if (!cls->GetValue("Server", serverTable))
	{
		pSS->RaiseError("'ServerMethods' exposed, but no 'Server' table in class");
		return pFH->EndFunction(false);
	}
	params->GetValue("ServerProperties", serverProperties);

	if (clientMethods.GetPtr())
	{
		CRY_ASSERT(clientTable.GetPtr());
		if (!BuildDispatchTable(clientMethods, clientTable, cls, CLIENT_DISPATCH_FIELD))
			return pFH->EndFunction(false);
	}

	if (serverMethods.GetPtr())
	{
		CRY_ASSERT(serverTable.GetPtr());
		if (!BuildDispatchTable(serverMethods, serverTable, cls, SERVER_DISPATCH_FIELD))
			return pFH->EndFunction(false);
	}

	if (serverProperties.GetPtr())
	{
		if (!BuildSynchTable(serverProperties, cls, SERVER_SYNCHED_FIELD))
			return pFH->EndFunction(false);
	}

	return pFH->EndFunction(true);
}

// build a script dispatch table - this table is the metatable for all
// dispatch proxy tables used (onClient, allClients, otherClients)
bool CScriptRMI::BuildDispatchTable(
  SmartScriptTable methods,
  SmartScriptTable classMethodTable,
  SmartScriptTable cls,
  const char* name)
{
	IScriptSystem* pSS = methods->GetScriptSystem();
	SmartScriptTable dispatch(pSS);

	uint8 funcID = 0;

	IScriptTable::SUserFunctionDesc fd;
	SFunctionInfo info;

	IScriptTable::Iterator iter = methods->BeginIteration();
	while (methods->MoveNext(iter))
	{
		if (iter.sKey)
		{
			const char* function = iter.sKey;

			if (strlen(function) >= 2 && function[0] == '_' && function[1] == '_')
			{
				methods->EndIteration(iter);
				pSS->RaiseError("In Net.Expose: can't expose functions beginning with '__' (function was %s)",
				                function);
				return false;
			}

			SmartScriptTable specTable;
			if (!methods->GetValue(function, specTable))
			{
				methods->EndIteration(iter);
				pSS->RaiseError("In Net.Expose: function %s entry is not a table (in %s)",
				                function, name);
				return false;
			}

			// fetch format
			int count = specTable->Count();
			if (count < 1)
			{
				methods->EndIteration(iter);
				pSS->RaiseError("In Net.Expose: function %s entry is an empty table (in %s)",
				                function, name);
				return false;
			}
			else if (count - 1 > MaxRMIParameters)
			{
				methods->EndIteration(iter);
				pSS->RaiseError("In Net.Expose: function %s has too many parameters (%d) (in %s)",
				                function, count - 1, name);
				return false;
			}
			int tempReliability;
			if (!specTable->GetAt(1, tempReliability) || tempReliability < 0 || tempReliability >= eNRT_NumReliabilityTypes)
			{
				methods->EndIteration(iter);
				pSS->RaiseError("In Net.Expose: function %s has invalid reliability type %d (in %s)",
				                function, tempReliability, name);
				return false;
			}
			ENetReliabilityType reliability = (ENetReliabilityType) tempReliability;
			if (!specTable->GetAt(2, tempReliability) || tempReliability < 0 || tempReliability >= eRAT_NumAttachmentTypes)
			{
				methods->EndIteration(iter);
				pSS->RaiseError("In Net.Expose: function %s has invalid attachment type %d (in %s)",
				                function, tempReliability, name);
			}
			ERMIAttachmentType attachment = (ERMIAttachmentType) tempReliability;
			string format;
			format.reserve(count - 1);
			for (int i = 3; i <= count; i++)
			{
				int type = 666;
				if (!specTable->GetAt(i, type) || type < -128 || type > 127)
				{
					methods->EndIteration(iter);
					pSS->RaiseError("In Net.Expose: function %s has invalid serialization policy %d at %d (in %s)",
					                function, type, i, name);
					return false;
				}
				format.push_back((char) type);
			}

			CRY_ASSERT(format.length() <= MaxRMIParameters);
			cry_strcpy(info.format, format.c_str());
			info.funcID = funcID;
			info.reliability = reliability;
			info.attachment = attachment;

			fd.pUserDataFunc = ProxyFunction;
			fd.sFunctionName = function;
			fd.nDataSize = sizeof(SFunctionInfo);
			fd.pDataBuffer = &info;
			fd.sGlobalName = "<net-dispatch>";
			fd.sFunctionParams = "(...)";

			dispatch->AddFunction(fd);

			string lookupData = function;
			lookupData += ":";
			lookupData += format;
			dispatch->SetAt(funcID + 1, lookupData.c_str());

			funcID++;
			if (funcID == 0)
			{
				funcID--;
				methods->EndIteration(iter);
				pSS->RaiseError("Too many functions... max is %d", funcID);
				return false;
			}
		}
		else
		{
			GameWarning("In Net.Expose: non-string key ignored");
		}
	}
	methods->EndIteration(iter);

	dispatch->SetValue(VALIDATED_FIELD, false);
	cls->SetValue(name, dispatch);

	return true;
}

// setup the meta-table for synched variables
bool CScriptRMI::BuildSynchTable(SmartScriptTable vars, SmartScriptTable cls, const char* name)
{
	IScriptSystem* pSS = vars->GetScriptSystem();
	SmartScriptTable synched(pSS);
	SmartScriptTable defaultValues(pSS);

	// TODO: Improve
	IScriptTable::SUserFunctionDesc fd;
	fd.pFunctor = functor_ret(SynchedNewIndexFunction);
	fd.sFunctionName = "__newindex";
	fd.sGlobalName = "<net-dispatch>";
	fd.sFunctionParams = "(...)";
	synched->AddFunction(fd);

	std::vector<SSynchedPropertyInfo> properties;

	IScriptTable::Iterator iter = vars->BeginIteration();
	while (vars->MoveNext(iter))
	{
		if (iter.sKey)
		{
			int type;
			if (!vars->GetValue(iter.sKey, type))
			{
				vars->EndIteration(iter);
				pSS->RaiseError("No type for %s", iter.sKey);
				return false;
			}
			size_t len = strlen(iter.sKey);
			if (len > MaxSynchedPropertyNameLength)
			{
				vars->EndIteration(iter);
				pSS->RaiseError("Synched var name '%s' too long (max is %d)",
				                iter.sKey, (int)MaxSynchedPropertyNameLength);
				return false;
			}
			SSynchedPropertyInfo info;
			cry_strcpy(info.name, iter.sKey);
			info.type = (EScriptSerializeType) type;
			properties.push_back(info);

			if (info.type == eSST_String)
				defaultValues->SetValue(iter.sKey, "");
			else
				defaultValues->SetValue(iter.sKey, 0);
		}
	}
	vars->EndIteration(iter);

	if (properties.empty())
		return true;

	fd.pFunctor = NULL;
	fd.pUserDataFunc = SerializeFunction;
	fd.nDataSize = sizeof(SSynchedPropertyInfo) * properties.size();
	fd.pDataBuffer = &properties[0];
	fd.sFunctionName = SERIALIZE_FUNCTION;
	fd.sFunctionParams = "(...)";
	fd.sGlobalName = "<net-dispatch>";
	synched->AddFunction(fd);

	cls->SetValue(SERVER_SYNCHED_FIELD, synched);
	cls->SetValue("synched", defaultValues);

	return true;
}

// initialize an entity for networked methods
void CScriptRMI::SetupEntity(EntityId eid, IEntity* pEntity, bool client, bool server)
{
	if (!m_pParent)
	{
		GameWarning("Trying to setup an entity for network with no game started... failing");
		return;
	}

	IEntityClass* pClass = pEntity->GetClass();
	stack_string className = pClass->GetName();

	IScriptTable* pEntityTable = pEntity->GetScriptTable();
	IScriptSystem* pSS = pEntityTable->GetScriptSystem();

	SmartScriptTable clientDispatchTable, serverDispatchTable, serverSynchedTable;
	pEntityTable->GetValue(CLIENT_DISPATCH_FIELD, clientDispatchTable);
	pEntityTable->GetValue(SERVER_DISPATCH_FIELD, serverDispatchTable);
	pEntityTable->GetValue(SERVER_SYNCHED_FIELD, serverSynchedTable);

	bool validated;
	if (clientDispatchTable.GetPtr())
	{
		if (!clientDispatchTable->GetValue(VALIDATED_FIELD, validated))
			return;
		if (!validated)
		{
			SmartScriptTable methods;
			if (!pEntityTable->GetValue("Client", methods))
			{
				GameWarning("No Client table, but has a client dispatch on class %s",
				            pEntity->GetClass()->GetName());
				return;
			}
			if (!ValidateDispatchTable(pEntity->GetClass()->GetName(), clientDispatchTable, methods, false))
				return;
		}
	}
	if (serverDispatchTable.GetPtr())
	{
		if (!serverDispatchTable->GetValue(VALIDATED_FIELD, validated))
			return;
		if (!validated)
		{
			SmartScriptTable methods;
			if (!pEntityTable->GetValue("Server", methods))
			{
				GameWarning("No Server table, but has a server dispatch on class %s",
				            pEntity->GetClass()->GetName());
				return;
			}
			if (!ValidateDispatchTable(pEntity->GetClass()->GetName(), serverDispatchTable, methods, true))
				return;
		}
	}

	ScriptHandle id;
	id.n = eid;

	ScriptHandle flags;

	if (client && serverDispatchTable.GetPtr())
	{
		flags.n = eDF_ToServer;
		AddProxyTable(pEntityTable, id, flags, "server", serverDispatchTable);
	}

	if (server && clientDispatchTable.GetPtr())
	{
		// only expose ownClient, otherClients for actors with a channelId
		flags.n = eDF_ToClientOnChannel;
		AddProxyTable(pEntityTable, id, flags, "onClient", clientDispatchTable);
		flags.n = eDF_ToClientOnOtherChannels;
		AddProxyTable(pEntityTable, id, flags, "otherClients", clientDispatchTable);
		flags.n = eDF_ToClientOnChannel | eDF_ToClientOnOtherChannels;
		AddProxyTable(pEntityTable, id, flags, "allClients", clientDispatchTable);
	}

	if (serverSynchedTable.GetPtr())
	{
		AddSynchedTable(pEntityTable, id, "synched", serverSynchedTable);
	}

	CryAutoCriticalSection lkDispatch(m_dispatchMutex);
	std::map<string, size_t>::iterator iter = m_entityClassToEntityTypeID.find(CONST_TEMP_STRING(pEntity->GetClass()->GetName()));
	if (iter == m_entityClassToEntityTypeID.end())
	{
		//[Timur] commented out as spam.
		//GameWarning("[scriptrmi] unable to find class %s", pEntity->GetClass()->GetName());
	}
	else
		m_entities[eid] = iter->second;
}

void CScriptRMI::RemoveEntity(EntityId id)
{
	CryAutoCriticalSection lkDispatch(m_dispatchMutex);
	m_entities.erase(id);
}

bool CScriptRMI::SerializeScript(TSerialize ser, IEntity* pEntity)
{
	SSerializeFunctionParams p(ser, pEntity);
	ScriptHandle hdl(&p);
	IScriptTable* pTable = pEntity->GetScriptTable();
	if (pTable)
	{
		SmartScriptTable serTable;
		SmartScriptTable synchedTable;
		pTable->GetValue("synched", synchedTable);
		if (synchedTable.GetPtr())
		{
			synchedTable->GetValue(HIDDEN_FIELD, serTable);
			if (serTable.GetPtr())
			{
				IScriptSystem* pScriptSystem = pTable->GetScriptSystem();
				pScriptSystem->BeginCall(serTable.GetPtr(), SERIALIZE_FUNCTION);
				pScriptSystem->PushFuncParam(serTable);
				pScriptSystem->PushFuncParam(hdl);
				return pScriptSystem->EndCall();
			}
		}
	}
	return true;
}

int CScriptRMI::SerializeFunction(IFunctionHandler* pH, void* pBuffer, int nSize)
{
	SmartScriptTable serTable;
	ScriptHandle hdl;
	pH->GetParam(1, serTable);
	pH->GetParam(2, hdl);
	SSerializeFunctionParams* p = (SSerializeFunctionParams*) hdl.ptr;

	SSynchedPropertyInfo* pInfo = (SSynchedPropertyInfo*) pBuffer;
	int nProperties = nSize / sizeof(SSynchedPropertyInfo);

	if (p->ser.IsReading())
	{
		for (int i = 0; i < nProperties; i++)
			if (!m_pThis->ReadValue(pInfo[i].name, pInfo[i].type, p->ser, serTable.GetPtr()))
				return pH->EndFunction(false);
	}
	else
	{
		for (int i = 0; i < nProperties; i++)
			if (!m_pThis->WriteValue(pInfo[i].name, pInfo[i].type, p->ser, serTable.GetPtr()))
				return pH->EndFunction(false);
	}
	return pH->EndFunction(true);
}

// one-time validation of entity tables
bool CScriptRMI::ValidateDispatchTable(const char* clazz, SmartScriptTable dispatch, SmartScriptTable methods, bool bServerTable)
{
	CryAutoCriticalSection lkDispatch(m_dispatchMutex);

	std::map<string, size_t>::iterator iterN = m_entityClassToEntityTypeID.lower_bound(clazz);
	if (iterN == m_entityClassToEntityTypeID.end() || iterN->first != clazz)
	{
		iterN = m_entityClassToEntityTypeID.insert(iterN, std::make_pair(clazz, m_entityClassToEntityTypeID.size()));
		CRY_ASSERT(iterN->second == m_dispatch.size());
		m_dispatch.push_back(SDispatch());
	}
	SDispatch& dispatchTblCont = m_dispatch[iterN->second];
	SFunctionDispatchTable& dispatchTbl = bServerTable ? dispatchTblCont.server : dispatchTblCont.client;

	IScriptTable::Iterator iter = dispatch->BeginIteration();
	while (dispatch->MoveNext(iter))
	{
		if (iter.sKey)
		{
			if (iter.sKey[0] == '_' && iter.sKey[1] == '_')
				continue;

			ScriptVarType type = methods->GetValueType(iter.sKey);
			if (type != svtFunction)
			{
				GameWarning("In class %s: function %s is exposed but not defined",
				            clazz, iter.sKey);
				dispatch->EndIteration(iter);
				return false;
			}
		}
		else
		{
			int id = iter.nKey;
			CRY_ASSERT(id > 0);
			id--;

			if (id >= dispatchTbl.size())
				dispatchTbl.resize(id + 1);
			SFunctionDispatch& dt = dispatchTbl[id];
			if (iter.value.GetVarType() != svtString)
			{
				GameWarning("Expected a string in dispatch table, got type %d", iter.value.GetVarType());
				dispatch->EndIteration(iter);
				return false;
			}

			const char* funcData = iter.value.GetString();
			const char* colon = strchr(funcData, ':');
			if (colon == NULL)
			{
				dispatch->EndIteration(iter);
				return false;
			}
			if (colon - funcData > MaxSynchedPropertyNameLength)
			{
				dispatch->EndIteration(iter);
				return false;
			}
			memcpy(dt.name, funcData, colon - funcData);
			dt.name[colon - funcData] = 0;
			cry_strcpy(dt.format, colon + 1);
		}
	}
	dispatch->EndIteration(iter);
	dispatch->SetValue(VALIDATED_FIELD, true);

	if (bServerTable)
	{
		dispatchTblCont.m_serverDispatchScriptTable = dispatch;
	}
	else
	{
		dispatchTblCont.m_clientDispatchScriptTable = dispatch;
	}

	return true;
}

class CScriptRMI::CCallHelper
{
public:
	CCallHelper()
		: m_pScriptSystem(nullptr)
		, m_pEntitySystem(nullptr)
		, m_pEntity(nullptr)
		, m_format(nullptr)
	{
		m_scriptTable = 0;
		m_function[0] = 0;

	}

	bool Prologue(EntityId objID, bool bClient, uint8 funcID)
	{
		if (!InitGameMembers(objID))
			return false;

		SmartScriptTable dispatchTable;
		if (!m_scriptTable->GetValue(bClient ? CLIENT_DISPATCH_FIELD : SERVER_DISPATCH_FIELD, dispatchTable))
			return false;

		const char* funcData;
		if (!dispatchTable->GetAt(funcID + 1, funcData))
		{
			GameWarning("No such function dispatch index %d on entity %s (class %s)", funcID,
			            m_pEntity->GetName(), m_pEntity->GetClass()->GetName());
			return false;
		}

		const char* colon = strchr(funcData, ':');
		if (colon == NULL)
			return false;
		if (colon - funcData > BUFFER)
			return false;
		memcpy(m_function, funcData, colon - funcData);
		m_function[colon - funcData] = 0;
		m_format = colon + 1;
		return true;
	}

	void Prologue(const char* function, const char* format)
	{
		cry_strcpy(m_function, function);
		m_format = format;
	}

	const char* GetFormat() const { return m_format; }

	bool        PerformCall(EntityId objID, CScriptRMISerialize* pSer, bool bClient, INetContext* pNetContext, INetChannel* pChannel)
	{
		if (!InitGameMembers(objID))
			return false;

		SmartScriptTable callTable;
		if (!m_scriptTable->GetValue(bClient ? "Client" : "Server", callTable))
			return false;

		if (!bClient)
			pNetContext->LogRMI(m_function, pSer);

		m_pScriptSystem->BeginCall(callTable, m_function);
		m_pScriptSystem->PushFuncParam(m_scriptTable);
		pSer->PushFunctionParams(m_pScriptSystem);
		// channel
		IGameChannel* pIGameChannel = pChannel->GetGameChannel();
		CGameServerChannel* pGameServerChannel = (CGameServerChannel*) pIGameChannel;
		uint16 channelId = pGameServerChannel->GetChannelId();
		m_pScriptSystem->PushFuncParam(channelId);
		//
		return m_pScriptSystem->EndCall();
	}

private:
	bool InitGameMembers(EntityId id)
	{
		if (!m_pScriptSystem)
		{
			ISystem* pSystem = GetISystem();
			m_pScriptSystem = pSystem->GetIScriptSystem();
			m_pEntitySystem = gEnv->pEntitySystem;

			IEntity* pEntity = m_pEntitySystem->GetEntity(id);
			if (!pEntity)
				return false;

			m_scriptTable = pEntity->GetScriptTable();
			if (!m_scriptTable.GetPtr())
				return false;
		}
		return true;
	}

	IScriptSystem*      m_pScriptSystem;
	IEntitySystem*      m_pEntitySystem;
	IEntity*            m_pEntity;
	SmartScriptTable    m_scriptTable;

	static const size_t BUFFER = 256;
	char                m_function[BUFFER];
	const char*         m_format;
};

// handle a call
INetAtSyncItem* CScriptRMI::HandleRMI(bool bClient, EntityId objID, uint8 funcID, TSerialize ser, INetChannel* pChannel)
{
	if (!m_pParent)
	{
		GameWarning("Trying to handle a remote method invocation with no game started... failing");
		return NULL;
	}

	CryAutoCriticalSection lkDispatch(m_dispatchMutex);

	class CHandleAnRMI : public INetAtSyncItem, private CCallHelper, private CScriptRMISerialize
	{
	public:
		CHandleAnRMI(bool bClient, EntityId objID, const SFunctionDispatch& info, TSerialize ser, CGameContext* pContext, INetChannel* pChannel) : CScriptRMISerialize(info.format), m_bClient(bClient), m_pContext(pContext), m_objID(objID), m_pChannel(pChannel)
		{
			Prologue(info.name, info.format);
			SerializeWith(ser);
		}

		bool Sync()
		{
			bool bDisconnect = PerformCall(m_objID, this, m_bClient, m_pContext->GetNetContext(), m_pChannel);
			if (bDisconnect && pDisconnectOnError && pDisconnectOnError->GetIVal() == 0)
				bDisconnect = false;
			return !bDisconnect;
		}

		bool SyncWithError(EDisconnectionCause& disconnectCause, string& disconnectMessage)
		{
			return Sync();
		}

		void DeleteThis()
		{
			delete this;
		}

	private:
		std::unique_ptr<CScriptRMISerialize> m_pSer;
		bool m_bClient;
		CGameContext*                        m_pContext;
		EntityId                             m_objID;
		INetChannel*                         m_pChannel;
	};

	// fetch the class of entity
	std::map<EntityId, size_t>::const_iterator iterEnt = m_entities.find(objID);
	if (iterEnt == m_entities.end())
		return 0;
	std::vector<SFunctionDispatch>& dispatchTable = bClient ? m_dispatch[iterEnt->second].client : m_dispatch[iterEnt->second].server;

	if (dispatchTable.size() <= funcID)
		return 0;

	return new CHandleAnRMI(bClient, objID, dispatchTable[funcID], ser, m_pParent, pChannel);
}

void CScriptRMI::OnInitClient(uint16 channelId, IEntity* pEntity)
{
	SmartScriptTable server;
	SmartScriptTable entity = pEntity->GetScriptTable();

	if (!entity.GetPtr())
		return;

	if (!entity->GetValue("Server", server) || !server.GetPtr())
		return;

	IScriptSystem* pSystem = entity->GetScriptSystem();
	if ((server->GetValueType("OnInitClient") == svtFunction) &&
	    pSystem->BeginCall(server, "OnInitClient"))
	{
		pSystem->PushFuncParam(entity);
		pSystem->PushFuncParam(channelId);
		pSystem->EndCall();
	}
}

void CScriptRMI::OnPostInitClient(uint16 channelId, IEntity* pEntity)
{
	SmartScriptTable server;
	SmartScriptTable entity = pEntity->GetScriptTable();

	if (!entity.GetPtr())
		return;

	if (!entity->GetValue("Server", server) || !server.GetPtr())
		return;

	IScriptSystem* pSystem = entity->GetScriptSystem();
	if ((server->GetValueType("OnPostInitClient") == svtFunction) &&
	    pSystem->BeginCall(server, "OnPostInitClient"))
	{
		pSystem->PushFuncParam(entity);
		pSystem->PushFuncParam(channelId);
		pSystem->EndCall();
	}
}

// add a dispatch proxy table to an entity
void CScriptRMI::AddProxyTable(IScriptTable* pEntityTable,
                               ScriptHandle id, ScriptHandle flags,
                               const char* name, SmartScriptTable dispatchTable)
{
	SmartScriptTable proxyTable(pEntityTable->GetScriptSystem());
	proxyTable->Delegate(dispatchTable.GetPtr());
	proxyTable->SetValue(FLAGS_FIELD, flags);
	proxyTable->SetValue(ID_FIELD, id);
	proxyTable->SetValue("__nopersist", true);
	pEntityTable->SetValue(name, proxyTable);
}

// add a synch proxy table to an entity
void CScriptRMI::AddSynchedTable(IScriptTable* pEntityTable,
                                 ScriptHandle id,
                                 const char* name, SmartScriptTable dispatchTable)
{
	SmartScriptTable synchedTable(pEntityTable->GetScriptSystem());
	SmartScriptTable hiddenTable(pEntityTable->GetScriptSystem());
	SmartScriptTable origTable;
	pEntityTable->GetValue(name, origTable);

	hiddenTable->Clone(dispatchTable);
	hiddenTable->SetValue("__index", hiddenTable);

	IScriptTable::Iterator iter = origTable->BeginIteration();
	while (origTable->MoveNext(iter))
	{
		if (iter.sKey)
		{
			if (hiddenTable->GetValueType(iter.sKey) != svtNull)
				GameWarning("Replacing non-null value %s", iter.sKey);

			ScriptAnyValue value;
			origTable->GetValueAny(iter.sKey, value);
			hiddenTable->SetValueAny(iter.sKey, value);
		}
	}
	origTable->EndIteration(iter);

	synchedTable->Delegate(hiddenTable);
	synchedTable->SetValue(ID_FIELD, id);
	synchedTable->SetValue(HIDDEN_FIELD, hiddenTable);
	pEntityTable->SetValue(name, synchedTable);
}

// send a single call to a channel - used for implementation of ProxyFunction
void CScriptRMI::DispatchRMI(INetChannel* pChannel, _smart_ptr<CScriptMessage> pMsg, bool bClient)
{
	if (!m_pThis->m_pParent)
	{
		GameWarning("Trying to dispatch a remote method invocation with no game started... failing");
		return;
	}

	if (!pChannel->IsLocal())
	{
		NET_PROFILE_SCOPE_RMI("RMI::CScriptRMI", false);
		pChannel->DispatchRMI(&*pMsg);
	}
	else
	{
		CCallHelper helper;
		if (!helper.Prologue(pMsg->objId, bClient, pMsg->funcId))
		{
			CRY_ASSERT(!"This should never happen");
			return;
		}
		helper.PerformCall(pMsg->objId, pMsg, bClient, m_pThis->m_pParent->GetNetContext(), pChannel);
	}
}

// send a call
int CScriptRMI::ProxyFunction(IFunctionHandler* pH, void* pBuffer, int nSize)
{
	if (!m_pThis->m_pParent)
	{
		pH->GetIScriptSystem()->RaiseError("Trying to proxy a remote method invocation with no game started... failing");
		return pH->EndFunction();
	}

	string funcInfo;
	string dispatchInfo;
	bool gatherDebugInfo = pLogRMIEvents && pLogRMIEvents->GetIVal() != 0;

	IScriptSystem* pSS = pH->GetIScriptSystem();
	const SFunctionInfo* pFunctionInfo = static_cast<const SFunctionInfo*>(pBuffer);

	SmartScriptTable proxyTable;
	if (!pH->GetParam(1, proxyTable))
		return pH->EndFunction(false);

	ScriptHandle flags;
	ScriptHandle id;
	if (!proxyTable->GetValue(FLAGS_FIELD, flags))
		return pH->EndFunction(false);
	if (!proxyTable->GetValue(ID_FIELD, id))
		return pH->EndFunction(false);

	int formatStart = 2;
	if (uint32 flagsMask = flags.n & (eDF_ToClientOnChannel | eDF_ToClientOnOtherChannels))
	{
		if (flagsMask != (eDF_ToClientOnChannel | eDF_ToClientOnOtherChannels))
			formatStart++;
	}

	_smart_ptr<CScriptMessage> pMsg = new CScriptMessage(
	  pFunctionInfo->reliability,
	  pFunctionInfo->attachment,
	  (EntityId)id.n,
	  pFunctionInfo->funcID,
	  pFunctionInfo->format);
	if (!pMsg->SetFromFunction(pH, formatStart))
	{
		return pH->EndFunction(false);
	}

	if (gatherDebugInfo)
		funcInfo = pMsg->DebugInfo();

	INetContext* pNetContext = m_pThis->m_pParent->GetNetContext();
	CCryAction* pFramework = m_pThis->m_pParent->GetFramework();

	if (flags.n & eDF_ToServer)
	{
		CGameClientNub* pClientNub = pFramework->GetGameClientNub();
		bool called = false;
		if (pClientNub)
		{
			CGameClientChannel* pChannel = pClientNub->GetGameClientChannel();
			if (pChannel)
			{
				DispatchRMI(pChannel->GetNetChannel(), pMsg, false);
				called = true;
			}
		}
		if (!called)
		{
			pSS->RaiseError("RMI via client (to server) requested but we are not a client");
		}

		if (gatherDebugInfo)
			dispatchInfo = "server";
	}
	else if (flags.n & (eDF_ToClientOnChannel | eDF_ToClientOnOtherChannels))
	{
		CGameServerNub* pServerNub = pFramework->GetGameServerNub();
		if (pServerNub)
		{
			int myChannelId = 0;
			bool ok = true;
			if (flags.n != (eDF_ToClientOnChannel | eDF_ToClientOnOtherChannels))
			{
				//Allow the local channel id to be null for otherClients RMIs, because the local channel id is set to null on dedicated servers
				if (!((flags.n & eDF_ToClientOnOtherChannels) != 0 && pH->GetParamType(2) == svtNull))
				{
					if (!pH->GetParam(2, myChannelId))
					{
						pSS->RaiseError("RMI onClient or otherClients must have a valid channel id for its first argument");
						ok = false;
					}
					else if (pServerNub->GetServerChannelMap()->find(myChannelId) == pServerNub->GetServerChannelMap()->end())
					{
						pSS->RaiseError("RMI No such server channel %d", myChannelId);
						ok = false;
					}
				}
			}
			if (ok)
			{
				TServerChannelMap* pChannelMap = pServerNub->GetServerChannelMap();
				for (TServerChannelMap::iterator iter = pChannelMap->begin(); iter != pChannelMap->end(); ++iter)
				{
					bool isOwn = iter->first == myChannelId;
					if (isOwn && !(flags.n & eDF_ToClientOnChannel))
						continue;
					if (!isOwn && !(flags.n & eDF_ToClientOnOtherChannels))
						continue;

					if (iter->second->GetNetChannel())
						DispatchRMI(iter->second->GetNetChannel(), pMsg, true);
				}
				if (gatherDebugInfo)
				{
					dispatchInfo = "client: ";
					bool appendChannel = false;
					switch (flags.n)
					{
					case eDF_ToClientOnChannel:
						dispatchInfo += "specificChannel";
						appendChannel = true;
						break;
					case eDF_ToClientOnOtherChannels:
						dispatchInfo += "otherChannels";
						appendChannel = true;
						break;
					case eDF_ToClientOnChannel | eDF_ToClientOnOtherChannels:
						dispatchInfo += "allChannels";
						break;
					default:
						dispatchInfo += "UNKNOWN(BUG)";
						appendChannel = true;
						CRY_ASSERT(false);
					}
					if (appendChannel)
					{
						string temp;
						temp.Format(" %d", myChannelId);
						dispatchInfo += temp;
					}
				}
			}
		}
		else
		{
			pSS->RaiseError("RMI via server (to client) requested but we are not a server");
		}
	}

	if (gatherDebugInfo)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity((EntityId)id.n);
		const char* name = "<invalid>";
		if (pEntity) name = pEntity->GetName();
		CryLogAlways("[rmi] %s(%s) [%s] on entity[%d] %s",
		             pH->GetFuncName(), funcInfo.c_str(), dispatchInfo.c_str(), (int)id.n, name);
	}

	return pH->EndFunction(true);
}

int CScriptRMI::SynchedNewIndexFunction(IFunctionHandler* pH)
{
	if (!m_pThis->m_pParent)
	{
		pH->GetIScriptSystem()->RaiseError("Trying to set a synchronized variable with no game started... failing");
		return pH->EndFunction();
	}

	SmartScriptTable table;
	SmartScriptTable hidden;
	const char* key;
	ScriptAnyValue value;

	if (!pH->GetParam(1, table))
		return pH->EndFunction(false);

	ScriptHandle id;
	if (!table->GetValue(ID_FIELD, id))
		return pH->EndFunction(false);
	if (!table->GetValue(HIDDEN_FIELD, hidden))
		return pH->EndFunction(false);

	if (!pH->GetParam(2, key))
		return pH->EndFunction(false);
	if (!pH->GetParamAny(3, value))
		return pH->EndFunction(false);

	hidden->SetValueAny(key, value);

	m_pThis->m_pParent->ChangedScript((EntityId) id.n);

	return pH->EndFunction(true);
}

void CScriptRMI::GetMemoryStatistics(ICrySizer* s)
{
	CryAutoCriticalSection lk(m_dispatchMutex);

	s->AddObject(this, sizeof(*this));
	s->AddObject(m_entityClassToEntityTypeID);
	s->AddObject(m_entities);
	s->AddObject(m_dispatch);
}
