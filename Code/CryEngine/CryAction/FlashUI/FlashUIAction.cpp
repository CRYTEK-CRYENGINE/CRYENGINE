// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIAction.cpp
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "FlashUIAction.h"
#include "FlashUI.h"
#include "ScriptBind_UIAction.h"

//------------------------------------------------------------------------------------
CFlashUIAction::CFlashUIAction(EUIActionType type)
	: m_bIsValid(false)
	, m_bEnabled(true)
	, m_type(type)
{
	if (m_type == eUIAT_FlowGraph)
	{
		CRY_ASSERT_MESSAGE(gEnv->pFlowSystem, "FlowSystem not initialized, crash will follow!");
		m_pFlowGraph = gEnv->pFlowSystem->CreateFlowGraph();
		m_pFlowGraph->UnregisterFromFlowSystem();
		m_pFlowGraph->AddRef();
		m_pFlowGraph->SetType(IFlowGraph::eFGT_UIAction);
		m_pFlowGraph->SetDebugName("[UI Action] ");
	}
	else
	{
		CRY_ASSERT_MESSAGE(gEnv->pScriptSystem, "ScriptSystem not initialized, crash will follow!");
		m_pScript = gEnv->pScriptSystem->CreateTable();
	}
}

//------------------------------------------------------------------------------------
CFlashUIAction::~CFlashUIAction()
{
	if (m_type == eUIAT_FlowGraph)
	{
		assert(m_pFlowGraph != NULL);
		m_pFlowGraph->Release();
	}
}

//------------------------------------------------------------------------------------
bool CFlashUIAction::Init()
{
	if (!m_bIsValid) return false;

	UIACTION_LOG("UIAction %s Init (%s)", GetName(), m_type == eUIAT_FlowGraph ? "FG Action" : "Lua Action");
	if (m_type == eUIAT_FlowGraph)
	{
		assert(m_pFlowGraph != NULL);
		m_pFlowGraph->SetEnabled(true);
		m_pFlowGraph->InitializeValues();
		m_pFlowGraph->SetEnabled(m_bEnabled);
	}
	return true;
}

//------------------------------------------------------------------------------------
void CFlashUIAction::SetEnabled(bool bEnabled)
{
	if (!m_bIsValid) return;

	m_bEnabled = bEnabled;
	if (m_type == eUIAT_FlowGraph)
	{
		assert(m_pFlowGraph != NULL);
		m_pFlowGraph->SetEnabled(bEnabled);
	}
	else
	{
		assert(m_pScript.GetPtr() != NULL);
		if (m_scriptAvail[eSF_OnInit] && bEnabled)
		{
			Script::CallMethod(m_pScript, "OnInit");
			m_scriptAvail[eSF_OnInit] = false;
		}
		if (m_scriptAvail[eSF_OnEnabled])
			Script::CallMethod(m_pScript, "OnEnabled", bEnabled);
		m_pScript->SetValue("enabled", bEnabled);
	}
}

//------------------------------------------------------------------------------------
bool CFlashUIAction::Serialize(XmlNodeRef& xmlNode, bool bIsLoading)
{
	CRY_ASSERT_MESSAGE(m_type == eUIAT_FlowGraph, "Try to serialize Flowgraph of Lua UI Action");
	bool ok = m_pFlowGraph->SerializeXML(xmlNode, bIsLoading);
	SetValid(ok);
	return ok;
}

//------------------------------------------------------------------------------------
bool CFlashUIAction::Serialize(const char* scriptFile, bool bIsLoading)
{
	CRY_ASSERT_MESSAGE(m_type == eUIAT_LuaScript, "Try to serialize Script of FG UI Action");
	m_sScriptFile = scriptFile;
	return ReloadScript();
}

//-------------------------------------------------------------------
bool CFlashUIAction::ReloadScript()
{
	if (m_type == eUIAT_LuaScript)
	{
		bool ok = gEnv->pScriptSystem->ExecuteFile(m_sScriptFile.c_str(), true, true);
		if (ok)
			ok = gEnv->pScriptSystem->GetGlobalValue(GetName(), m_pScript);
		m_scriptAvail[eSF_OnInit] = ok && m_pScript->GetValueType("OnInit") == svtFunction;
		m_scriptAvail[eSF_OnStart] = ok && m_pScript->GetValueType("OnStart") == svtFunction;
		m_scriptAvail[eSF_OnUpdate] = ok && m_pScript->GetValueType("OnUpdate") == svtFunction;
		m_scriptAvail[eSF_OnEnd] = ok && m_pScript->GetValueType("OnEnd") == svtFunction;
		m_scriptAvail[eSF_OnEnabled] = ok && m_pScript->GetValueType("OnEnabled") == svtFunction;
		SetValid(ok);
		if (ok)
		{
			m_pScript->GetValue("enabled", m_bEnabled);
			m_pScript->SetValue("__ui_action_name", GetName());
		}
	}
	return m_bIsValid;
}

//-------------------------------------------------------------------
void CFlashUIAction::GetMemoryUsage(ICrySizer* s) const
{
	SIZER_SUBCOMPONENT_NAME(s, "UIAction");
	s->AddObject(this, sizeof(*this));
	if (m_pFlowGraph != NULL)
		m_pFlowGraph->GetMemoryUsage(s);
}

//--------------------------------------------------------------------------------------------
void CFlashUIAction::Update()
{
	if (!m_bEnabled || !m_bIsValid) return;

	if (m_type == eUIAT_FlowGraph)
	{
		assert(m_pFlowGraph != NULL);
		m_pFlowGraph->Update();
	}
	else
	{
		assert(m_pScript.GetPtr() != NULL);
		if (m_scriptAvail[eSF_OnUpdate])
			Script::CallMethod(m_pScript, "OnUpdate");
	}
}

//--------------------------------------------------------------------------------------------
void CFlashUIAction::StartScript(const SUIArguments& args)
{
	if (!m_bIsValid) return;

	assert(m_pScript.GetPtr() != NULL);
	SmartScriptTable table = gEnv->pScriptSystem->CreateTable();
	SUIToLuaConversationHelper::UIArgsToLuaTable(args, table);
	if (m_scriptAvail[eSF_OnStart])
		Script::CallMethod(m_pScript, "OnStart", table);
}

//--------------------------------------------------------------------------------------------
void CFlashUIAction::EndScript()
{
	if (!m_bIsValid) return;

	assert(m_pScript.GetPtr() != NULL);
	if (m_scriptAvail[eSF_OnEnd])
		Script::CallMethod(m_pScript, "OnEnd");
}

//--------------------------------------------------------------------------------------------
void CUIActionManager::Init()
{
	for (TActionMap::iterator it = m_actionStateMap.begin(); it != m_actionStateMap.end(); ++it)
		it->second = false;

	m_bAcceptRequests = true;
	m_actionStartMap.clear();
	m_actionEndMap.clear();
	m_actionEnableMap.clear();
}

//------------------------------------------------------------------------------------
void CUIActionManager::StartAction(IUIAction* pAction, const SUIArguments& args)
{
	if (!m_bAcceptRequests) return;

	if (pAction && pAction->IsValid())
	{
#ifndef _RELEASE
		if (m_actionStartMap.find(pAction) != m_actionStartMap.end())
		{
			UIACTION_WARNING("UIAction %s already started! Old args will be discarded!", pAction->GetName());
		}
#endif
		m_actionStartMap[pAction] = args;
		stl::member_find_and_erase(m_actionEndMap, pAction);
		UIACTION_LOG("UIAction %s start request (Args: %s)", pAction->GetName(), args.GetAsString());
		return;
	}
	UIACTION_ERROR("StartAction failed! UIAction %s not valid!", pAction ? pAction->GetName() : "NULL");
}

//------------------------------------------------------------------------------------
void CUIActionManager::EndAction(IUIAction* pAction, const SUIArguments& args)
{
	if (!m_bAcceptRequests) return;

	if (pAction && pAction->IsValid())
	{
#ifndef _RELEASE
		if (m_actionEndMap.find(pAction) != m_actionEndMap.end())
		{
			UIACTION_WARNING("UIAction %s already ended! Old args will be discarded!", pAction->GetName());
		}
#endif
		m_actionEndMap[pAction] = args;
		stl::member_find_and_erase(m_actionStartMap, pAction);
		UIACTION_LOG("UIAction %s end request (Args: %s)", pAction->GetName(), args.GetAsString());
		return;
	}
	UIACTION_ERROR("EndAction failed! UIAction %s not valid!", pAction ? pAction->GetName() : "NULL");
}

//------------------------------------------------------------------------------------
void CUIActionManager::EnableAction(IUIAction* pAction, bool bEnable)
{
	if (!m_bAcceptRequests) return;

	if (pAction && pAction->IsValid())
	{
		m_actionEnableMap[pAction] = bEnable;
		UIACTION_LOG("UIAction %s %s request", pAction->GetName(), bEnable ? "enable" : "disable");
		return;
	}
	UIACTION_ERROR("EnableAction failed! UIAction %s not valid!", pAction ? pAction->GetName() : "NULL");
}

//------------------------------------------------------------------------------------
void CUIActionManager::AddListener(IUIActionListener* pListener, const char* name)
{
	const bool ok = m_listener.Add(pListener, name);
	CRY_ASSERT_MESSAGE(ok, "Listener already registered!");
}

//------------------------------------------------------------------------------------
void CUIActionManager::RemoveListener(IUIActionListener* pListener)
{
	CRY_ASSERT_MESSAGE(m_listener.Contains(pListener), "Listener was never registered or already unregistered!");
	m_listener.Remove(pListener);
}

//-------------------------------------------------------------------
void CUIActionManager::GetMemoryUsage(ICrySizer* s) const
{
	SIZER_SUBCOMPONENT_NAME(s, "UIActionManager");
	s->AddObject(this, sizeof(*this));
	s->AddObject(&m_listener, m_listener.MemSize());
	s->AddContainer(m_actionStateMap);
	s->AddContainer(m_actionStartMap);
	s->AddContainer(m_actionEndMap);
	s->AddContainer(m_actionEnableMap);
}

//------------------------------------------------------------------------------------
void CUIActionManager::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!m_actionEnableMap.empty())
	{
		TActionMap actionEnableMap = m_actionEnableMap;
		m_actionEnableMap.clear();
		for (TActionMap::iterator it = actionEnableMap.begin(); it != actionEnableMap.end(); ++it)
		{
			EnableActionInt(it->first, it->second);
		}
	}

	if (!m_actionEndMap.empty())
	{
		TActionArgMap actionEndMap = m_actionEndMap;
		m_actionEndMap.clear();
		for (TActionArgMap::iterator it = actionEndMap.begin(); it != actionEndMap.end(); ++it)
		{
			EndActionInt(it->first, it->second);
		}
	}

	if (!m_actionStartMap.empty())
	{
		TActionArgMap actionStartMap = m_actionStartMap;
		m_actionStartMap.clear();
		for (TActionArgMap::iterator it = actionStartMap.begin(); it != actionStartMap.end(); ++it)
		{
			StartActionInt(it->first, it->second);
		}
	}
}

//------------------------------------------------------------------------------------
void CUIActionManager::StartActionInt(IUIAction* pAction, const SUIArguments& args)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (pAction && pAction->IsValid())
	{
		m_actionStateMap[pAction] = true;

		EnableActionInt(pAction, true);

		UIACTION_LOG("UIAction %s start (Args: %s)", pAction->GetName(), args.GetAsString());

		if (pAction->GetType() == IUIAction::eUIAT_LuaScript)
			((CFlashUIAction*)pAction)->StartScript(args);

		for (TActionListener::Notifier notifier(m_listener); notifier.IsValid(); notifier.Next())
			notifier->OnStart(pAction, args);
	}
}

//------------------------------------------------------------------------------------
void CUIActionManager::EndActionInt(IUIAction* pAction, const SUIArguments& args)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (pAction && m_actionStateMap[pAction])     // only allow to end actions that are started
	{
		m_actionStateMap[pAction] = false;

		UIACTION_LOG("UIAction %s end (Args: %s)", pAction->GetName(), args.GetAsString());

		if (pAction->GetType() == IUIAction::eUIAT_LuaScript)
			((CFlashUIAction*)pAction)->EndScript();

		for (TActionListener::Notifier notifier(m_listener); notifier.IsValid(); notifier.Next())
			notifier->OnEnd(pAction, args);
	}
}

//------------------------------------------------------------------------------------
void CUIActionManager::EnableActionInt(IUIAction* pAction, bool bEnable)
{
	if (pAction && pAction->IsValid() && pAction->IsEnabled() != bEnable)
	{
		UIACTION_LOG("UIAction %s %s", pAction->GetName(), bEnable ? "enabled" : "disabled");
		m_bAcceptRequests = false;
		pAction->Init();
		m_bAcceptRequests = true;
		pAction->SetEnabled(bEnable);
	}
}

//------------------------------------------------------------------------------------
