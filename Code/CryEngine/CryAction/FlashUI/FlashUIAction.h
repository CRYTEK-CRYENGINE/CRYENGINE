// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIAction.h
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __FlashUIAction_H__
#define __FlashUIAction_H__

#include <CrySystem/Scaleform/IFlashUI.h>
#include <CryFlowGraph/IFlowSystem.h>
#include <CryCore/Containers/CryListenerSet.h>

class CFlashUIAction : public IUIAction
{
public:
	CFlashUIAction(EUIActionType type);
	virtual ~CFlashUIAction();

	virtual EUIActionType    GetType() const            { return m_type; }

	virtual const char*      GetName() const            { return m_sName.c_str(); }
	virtual void             SetName(const char* sName) { m_sName = sName; }

	virtual bool             Init();
	virtual bool             IsValid() const { return m_bIsValid; }

	virtual void             SetEnabled(bool bEnabled);
	virtual bool             IsEnabled() const    { return m_bEnabled && m_bIsValid; }

	virtual IFlowGraphPtr    GetFlowGraph() const { CRY_ASSERT_MESSAGE(m_type == eUIAT_FlowGraph, "Try to access Flowgraph of Lua UI Action"); return m_pFlowGraph; }
	virtual SmartScriptTable GetScript() const    { CRY_ASSERT_MESSAGE(m_type == eUIAT_LuaScript, "Try to access ScriptTable of FG UI Action"); return m_pScript; }

	virtual bool             Serialize(XmlNodeRef& xmlNode, bool bIsLoading);
	virtual bool             Serialize(const char* scriptFile, bool bIsLoading);

	virtual void             GetMemoryUsage(ICrySizer* s) const;

	bool                     ReloadScript();

	void                     SetValid(bool bValid) { m_bIsValid = bValid; }

	void                     Update();

	// for script actions
	void StartScript(const SUIArguments& args);
	void EndScript();

private:
	string           m_sName;
	string           m_sScriptFile;
	IFlowGraphPtr    m_pFlowGraph;
	SmartScriptTable m_pScript;
	bool             m_bIsValid;
	bool             m_bEnabled;
	EUIActionType    m_type;
	enum EScriptFunction
	{
		eSF_OnInit,
		eSF_OnStart,
		eSF_OnUpdate,
		eSF_OnEnd,
		eSF_OnEnabled
	};
	std::map<EScriptFunction, bool> m_scriptAvail;
};

//--------------------------------------------------------------------------------------------
struct CUIActionManager : public IUIActionManager
{
public:
	CUIActionManager() : m_listener(32), m_bAcceptRequests(true) {}
	void         Init();

	virtual void StartAction(IUIAction* pAction, const SUIArguments& args);
	virtual void EndAction(IUIAction* pAction, const SUIArguments& args);

	virtual void EnableAction(IUIAction* pAction, bool bEnable);

	virtual void AddListener(IUIActionListener* pListener, const char* name);
	virtual void RemoveListener(IUIActionListener* pListener);

	virtual void GetMemoryUsage(ICrySizer* s) const;

	void         Update();

private:
	void StartActionInt(IUIAction* pAction, const SUIArguments& args);
	void EndActionInt(IUIAction* pAction, const SUIArguments& args);
	void EnableActionInt(IUIAction* pAction, bool bEnable);

private:
	typedef CListenerSet<IUIActionListener*>   TActionListener;
	typedef std::map<IUIAction*, bool>         TActionMap;
	typedef std::map<IUIAction*, SUIArguments> TActionArgMap;

	TActionListener m_listener;
	TActionMap      m_actionStateMap;
	TActionMap      m_actionEnableMap;
	TActionArgMap   m_actionStartMap;
	TActionArgMap   m_actionEndMap;
	bool            m_bAcceptRequests;
};

#endif // #ifndef __FlashUIAction_H__
