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
#ifndef __SCRIPTRMI_H__
#define __SCRIPTRMI_H__

#pragma once

#include "ScriptSerialize.h"
#include <CryThreading/CryAtomics.h>

class CGameContext;

// this class is a singleton that handles remote method invocation for
// script objects
class CScriptRMI : private CScriptSerialize
{
public:
	CScriptRMI();

	bool            Init();
	void            UnloadLevel();

	int             ExposeClass(IFunctionHandler* pFH);
	void            SetupEntity(EntityId id, IEntity* pEntity, bool client, bool server);
	void            RemoveEntity(EntityId id);
	INetAtSyncItem* HandleRMI(bool bClient, EntityId objID, uint8 funcID, TSerialize ser, INetChannel* pChannel);
	bool            SerializeScript(TSerialize ser, IEntity* pEntity);
	void            OnInitClient(uint16 channelId, IEntity* pEntity);
	void            OnPostInitClient(uint16 channelId, IEntity* pEntity);
	void            GetMemoryStatistics(ICrySizer* s);

	void            SetContext(CGameContext* pContext);

	static void     RegisterCVars();
	static void     UnregisterCVars();

private:
	enum EDispatchFlags
	{
		eDF_ToServer                = 0x01,
		eDF_ToClientOnChannel       = 0x02,
		eDF_ToClientOnOtherChannels = 0x04,
	};

	bool BuildDispatchTable(
	  SmartScriptTable methods,
	  SmartScriptTable methodTableFromCls,
	  SmartScriptTable cls,
	  const char* name);
	bool BuildSynchTable(
	  SmartScriptTable vars,
	  SmartScriptTable cls,
	  const char* name);

	void       AddProxyTable(IScriptTable* pEntityTable,
	                         ScriptHandle id, ScriptHandle flags, const char* name, SmartScriptTable dispatchTable);
	void       AddSynchedTable(IScriptTable* pEntityTable,
	                           ScriptHandle id, const char* name, SmartScriptTable metaTable);
	static int ProxyFunction(IFunctionHandler* pH, void* pBuffer, int nSize);
	static int SynchedNewIndexFunction(IFunctionHandler* pH);
	static int SynchedIndexFunction(IFunctionHandler* pH);
	static int SerializeFunction(IFunctionHandler* pH, void* pBuffer, int nSize);

	class CScriptMessage;
	class CCallHelper;
	static void DispatchRMI(INetChannel* pChannel, _smart_ptr<CScriptMessage> pMsg, bool bClient);

	CGameContext*      m_pParent;
	static CScriptRMI* m_pThis;

	struct SFunctionInfo;
	struct SSynchedPropertyInfo;

	static const size_t MaxRMIParameters = 31;             // must be a multiple of eight minus one
	static const size_t MaxSynchedPropertyNameLength = 31; // must be a multiple of eight minus one

	struct SFunctionDispatch
	{
		SFunctionDispatch()
		{
			format[0] = name[0] = 0;
		}
		void GetMemoryUsage(ICrySizer* pSizer) const {}
		char format[MaxRMIParameters + 1];
		char name[MaxSynchedPropertyNameLength + 1];
	};

	typedef std::vector<SFunctionDispatch> SFunctionDispatchTable;
	struct SDispatch
	{
		void GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(server);
			pSizer->AddObject(client);
		}

		SFunctionDispatchTable server;
		SFunctionDispatchTable client;

		SmartScriptTable       m_serverDispatchScriptTable;
		SmartScriptTable       m_clientDispatchScriptTable;
	};

	bool ValidateDispatchTable(const char* clazz, SmartScriptTable dispatch, SmartScriptTable methods, bool bServerTable);

	// everything under here is protected by the following mutex, and is for multithreaded serialization
	CryCriticalSection         m_dispatchMutex;
	std::map<string, size_t>   m_entityClassToEntityTypeID;
	std::map<EntityId, size_t> m_entities;
	std::vector<SDispatch>     m_dispatch;
};

#endif
