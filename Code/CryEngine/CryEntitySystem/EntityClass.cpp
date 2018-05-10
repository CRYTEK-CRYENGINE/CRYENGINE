// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EntityClass.h"
#include "EntityScript.h"

#include <CrySchematyc/ICore.h>
#include <CryFlowGraph/IFlowSystem.h>

//////////////////////////////////////////////////////////////////////////
CEntityClass::CEntityClass()
{
	m_pfnUserProxyCreate = NULL;
	m_pUserProxyUserData = NULL;
	m_pEventHandler = NULL;
	m_pScriptFileHandler = NULL;

	m_pEntityScript = NULL;
	m_bScriptLoaded = false;
}

//////////////////////////////////////////////////////////////////////////
CEntityClass::~CEntityClass()
{
	SAFE_RELEASE(m_pIFlowNodeFactory);
	SAFE_RELEASE(m_pEntityScript);
}

//////////////////////////////////////////////////////////////////////////
IScriptTable* CEntityClass::GetScriptTable() const
{
	IScriptTable* pScriptTable = NULL;

	if (m_pEntityScript)
	{
		CEntityScript* pScript = static_cast<CEntityScript*>(m_pEntityScript);
		pScriptTable = (pScript ? pScript->GetScriptTable() : NULL);
	}

	return pScriptTable;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityClass::LoadScript(bool bForceReload)
{
	bool bRes = true;
	if (m_pEntityScript)
	{
		CEntityScript* pScript = static_cast<CEntityScript*>(m_pEntityScript);
		bRes = pScript->LoadScript(bForceReload);

		m_bScriptLoaded = true;
	}

	if (m_pScriptFileHandler && bForceReload)
		m_pScriptFileHandler->ReloadScriptFile();

	if (m_pEventHandler && bForceReload)
		m_pEventHandler->RefreshEvents();

	return bRes;
}

/////////////////////////////////////////////////////////////////////////
int CEntityClass::GetEventCount()
{
	if (m_pEventHandler)
		return m_pEventHandler->GetEventCount();

	if (!m_bScriptLoaded)
		LoadScript(false);

	if (!m_pEntityScript)
		return 0;

	return static_cast<CEntityScript*>(m_pEntityScript)->GetEventCount();
}

//////////////////////////////////////////////////////////////////////////
CEntityClass::SEventInfo CEntityClass::GetEventInfo(int nIndex)
{
	SEventInfo info;

	if (m_pEventHandler)
	{
		IEntityEventHandler::SEventInfo eventInfo;

		if (m_pEventHandler->GetEventInfo(nIndex, eventInfo))
		{
			info.name = eventInfo.name;
			info.bOutput = (eventInfo.type == IEntityEventHandler::Output);

			switch (eventInfo.valueType)
			{
			case IEntityEventHandler::Int:
				info.type = EVT_INT;
				break;
			case IEntityEventHandler::Float:
				info.type = EVT_FLOAT;
				break;
			case IEntityEventHandler::Bool:
				info.type = EVT_BOOL;
				break;
			case IEntityEventHandler::Vector:
				info.type = EVT_VECTOR;
				break;
			case IEntityEventHandler::Entity:
				info.type = EVT_ENTITY;
				break;
			case IEntityEventHandler::String:
				info.type = EVT_STRING;
				break;
			default:
				assert(0);
				break;
			}
		}
		else
		{
			info.name = "";
			info.bOutput = false;
		}

		return info;
	}

	if (!m_bScriptLoaded)
		LoadScript(false);

	CRY_ASSERT(nIndex >= 0 && nIndex < GetEventCount());

	if (m_pEntityScript)
	{
		const SEntityScriptEvent& scriptEvent = static_cast<CEntityScript*>(m_pEntityScript)->GetEvent(nIndex);
		info.name = scriptEvent.name.c_str();
		info.bOutput = scriptEvent.bOutput;
		info.type = scriptEvent.valueType;
	}
	else
	{
		info.name = "";
		info.bOutput = false;
	}

	return info;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityClass::FindEventInfo(const char* sEvent, SEventInfo& event)
{
	if (!m_bScriptLoaded)
		LoadScript(false);

	if (!m_pEntityScript)
		return false;

	const SEntityScriptEvent* pScriptEvent = static_cast<CEntityScript*>(m_pEntityScript)->FindEvent(sEvent);
	if (!pScriptEvent)
		return false;

	event.name = pScriptEvent->name.c_str();
	event.bOutput = pScriptEvent->bOutput;
	event.type = pScriptEvent->valueType;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntityClass::SetClassDesc(const IEntityClassRegistry::SEntityClassDesc& classDesc)
{
	m_sName = classDesc.sName;
	m_nFlags = classDesc.flags;
	m_guid = classDesc.guid;
	m_schematycRuntimeClassGuid = classDesc.schematycRuntimeClassGuid;
	m_onSpawnCallback = classDesc.onSpawnCallback;
	m_sScriptFile = classDesc.sScriptFile;
	m_pfnUserProxyCreate = classDesc.pUserProxyCreateFunc;
	m_pUserProxyUserData = classDesc.pUserProxyData;
	m_pScriptFileHandler = classDesc.pScriptFileHandler;
	m_EditorClassInfo = classDesc.editorClassInfo;
	m_pEventHandler = classDesc.pEventHandler;

	if (m_pIFlowNodeFactory)
	{
		m_pIFlowNodeFactory->Release();
	}
	m_pIFlowNodeFactory = classDesc.pIFlowNodeFactory;
	if (m_pIFlowNodeFactory)
	{
		m_pIFlowNodeFactory->AddRef();
	}
}

void CEntityClass::SetName(const char* sName)
{
	m_sName = sName;
}

void CEntityClass::SetGUID(const CryGUID& guid)
{
	m_guid = guid;
}

void CEntityClass::SetScriptFile(const char* sScriptFile)
{
	m_sScriptFile = sScriptFile;
}

void CEntityClass::SetEntityScript(IEntityScript* pScript)
{
	m_pEntityScript = pScript;
}

void CEntityClass::SetUserProxyCreateFunc(UserProxyCreateFunc pFunc, void* pUserData /*=NULL */)
{
	m_pfnUserProxyCreate = pFunc;
	m_pUserProxyUserData = pUserData;
}

void CEntityClass::SetEventHandler(IEntityEventHandler* pEventHandler)
{
	m_pEventHandler = pEventHandler;
}

void CEntityClass::SetScriptFileHandler(IEntityScriptFileHandler* pScriptFileHandler)
{
	m_pScriptFileHandler = pScriptFileHandler;
}

void CEntityClass::SetOnSpawnCallback(const OnSpawnCallback& callback)
{
	m_onSpawnCallback = callback;
}

Schematyc::IRuntimeClassConstPtr CEntityClass::GetSchematycRuntimeClass() const
{
	if (!m_pSchematycRuntimeClass && !m_schematycRuntimeClassGuid.IsNull())
	{
		// Cache Schematyc runtime class pointer
		m_pSchematycRuntimeClass = gEnv->pSchematyc->GetRuntimeRegistry().GetClass(m_schematycRuntimeClassGuid);
	}

	return m_pSchematycRuntimeClass;
}

IEntityEventHandler* CEntityClass::GetEventHandler() const
{
	return m_pEventHandler;
}

IEntityScriptFileHandler* CEntityClass::GetScriptFileHandler() const
{
	return m_pScriptFileHandler;
}

const SEditorClassInfo& CEntityClass::GetEditorClassInfo() const
{
	return m_EditorClassInfo;
}

void CEntityClass::SetEditorClassInfo(const SEditorClassInfo& editorClassInfo)
{
	m_EditorClassInfo = editorClassInfo;
}
