// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ScriptBind_UIAction.h
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __SCRIPTBIND_UIACTION_H__
#define __SCRIPTBIND_UIACTION_H__

#pragma once

#include <CryScriptSystem/IScriptSystem.h>
#include <CryScriptSystem/ScriptHelpers.h>
#include <CrySystem/Scaleform/IFlashUI.h>
#include <CryCore/Containers/CryListenerSet.h>

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
struct SUIToLuaConversationHelper
{
	static ScriptAnyValue UIValueToLuaValue(const TUIData& value, bool& ok);
	static bool           LuaTableToUIArgs(SmartScriptTable table, SUIArguments& args);
	static bool           LuaArgsToUIArgs(IFunctionHandler* pH, int startIdx, SUIArguments& args);
	static bool           LuaArgToUIArg(IFunctionHandler* pH, int idx, TUIData& value);
	static bool           UIArgsToLuaTable(const SUIArguments& args, SmartScriptTable table);
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
template<class T> struct SUILuaCallbackInfo
{
	bool Accept(const SUILuaCallbackInfo<T>& info) const                                                                                { assert(false); return false; }
	void CallScript(SmartScriptTable pScript, const char* functionName, SmartScriptTable args, const SUILuaCallbackInfo<T>& info) const { assert(false); }
};

////////////////////////////////////////////////////////////////////////////
template<class T>
struct SUILuaCallbackData
{
	SUILuaCallbackData(SmartScriptTable pTable, int id, const char* functionName, const SUILuaCallbackInfo<T>& cbinfo)
		: pScriptTable(pTable)
		, Id(id)
		, fctName(functionName)
		, info(cbinfo)
	{
	}
	int                   Id;
	SmartScriptTable      pScriptTable;
	string                fctName;
	SUILuaCallbackInfo<T> info;
};

////////////////////////////////////////////////////////////////////////////
struct SUISUILuaCallbackDataId
{
	static int GetNextId()
	{
		static int id = 0;
		++id;
		assert(id > 0);
		return id;
	}
};

template<class T>
struct SUILuaCallbackContainer : public SUISUILuaCallbackDataId
{
	SUILuaCallbackContainer() : m_Callbacks(10) {}
	bool AddCallback(SmartScriptTable pTable, const char* functionName, const SUILuaCallbackInfo<T>& cbinfo)
	{
		int id = -1;
		pTable->GetValue("__ui_callback_id", id);
		if (id > -1)
		{
			for (typename TCallbacks::Notifier it(m_Callbacks); it.IsValid(); it.Next())
			{
				SUILuaCallbackData<T>* pListener = *it;
				if (pListener->Id == id && pListener->fctName == functionName && pListener->info.Accept(cbinfo))
					return false;
			}
		}
		else
		{
			id = GetNextId();
			pTable->SetValue("__ui_callback_id", id);
		}
		m_Callbacks.Add(new SUILuaCallbackData<T>(pTable, id, functionName, cbinfo));
		return true;
	}

	bool RemoveCallbacks(SmartScriptTable pTable, const char* functionName = "")
	{
		bool bErased = false;
		int id = -1;
		pTable->GetValue("__ui_callback_id", id);
		if (id > -1)
		{
			for (typename TCallbacks::Notifier it(m_Callbacks); it.IsValid(); it.Next())
			{
				SUILuaCallbackData<T>* pListener = *it;
				assert(pListener);
				PREFAST_ASSUME(pListener);
				if (pListener->Id == id && (strlen(functionName) == 0 || pListener->fctName == functionName))
				{
					m_Callbacks.Remove(pListener);
					delete pListener;
				}
			}
		}
		return bErased;
	}

	bool IsEnabled(SmartScriptTable pTable)
	{
		bool bEnabled = true;
		const char* name;
		if (pTable->GetValue("__ui_action_name", name) && strlen(name) > 0)
			bEnabled &= pTable->GetValue("enabled", bEnabled);
		return bEnabled;
	}

	void ClearCallbacks()
	{
		for (typename TCallbacks::Notifier it(m_Callbacks); it.IsValid(); it.Next())
			delete *it;
		m_Callbacks.Clear();
	}

protected:
	void NotifyEvent(const SUILuaCallbackInfo<T>& cbinfo, const SUIArguments& args = SUIArguments())
	{
		SmartScriptTable table = gEnv->pScriptSystem->CreateTable();
		SUIToLuaConversationHelper::UIArgsToLuaTable(args, table);

		for (typename TCallbacks::Notifier it(m_Callbacks); it.IsValid(); it.Next())
		{
			SUILuaCallbackData<T>* pListener = *it;
			if (pListener->info.Accept(cbinfo) && IsEnabled(pListener->pScriptTable))
				pListener->info.CallScript(pListener->pScriptTable, pListener->fctName, table, cbinfo);
		}
	}

private:
	typedef CListenerSet<SUILuaCallbackData<T>*> TCallbacks;
	TCallbacks m_Callbacks;
};

////////////////////////////////////////////////////////////////////////////
/////////////////////////////// IUIElement /////////////////////////////////
////////////////////////////////////////////////////////////////////////////
template<>
struct SUILuaCallbackInfo<IUIElement>
{
	static SUILuaCallbackInfo<IUIElement> CreateInfo(IUIElement* pElement, const char* eventName, int instanceId = -2)
	{
		assert(pElement);
		PREFAST_ASSUME(pElement);
		SUILuaCallbackInfo<IUIElement> res;
		res.eventName = eventName;
		res.elementName = pElement->GetName();
		res.instanceId = instanceId > -2 ? instanceId : pElement->GetInstanceID();
		return res;
	}

	bool Accept(const SUILuaCallbackInfo<IUIElement>& info) const
	{
		return (elementName == "" || info.elementName == elementName) && (eventName == "" || info.eventName == eventName) && (instanceId == -1 || instanceId == info.instanceId);
	}

	void CallScript(SmartScriptTable pScript, const char* functionName, SmartScriptTable args, const SUILuaCallbackInfo<IUIElement>& info) const
	{
		Script::CallMethod(pScript, functionName, info.elementName.c_str(), info.instanceId, info.eventName.c_str(), args);
	}

	int    instanceId;
	string eventName;
	string elementName;
};

////////////////////////////////////////////////////////////////////////////
struct SUIElementLuaCallback : public SUILuaCallbackContainer<IUIElement>, public IUIElementEventListener
{
	~SUIElementLuaCallback();

	void         Init(IUIElement* pElement);
	void         Clear();

	virtual void OnUIEvent(IUIElement* pSender, const SUIEventDesc& event, const SUIArguments& args);
	virtual void OnUIEventEx(IUIElement* pSender, const char* fscommand, const SUIArguments& args, void* pUserData);

	virtual void OnInit(IUIElement* pSender, IFlashPlayer* pFlashPlayer);
	virtual void OnUnload(IUIElement* pSender);
	virtual void OnSetVisible(IUIElement* pSender, bool bVisible);

	virtual void OnInstanceCreated(IUIElement* pSender, IUIElement* pNewInstance);
	virtual void OnInstanceDestroyed(IUIElement* pSender, IUIElement* pDeletedInstance);
};

////////////////////////////////////////////////////////////////////////////
//////////////////////////////// IUIAction /////////////////////////////////
////////////////////////////////////////////////////////////////////////////
template<>
struct SUILuaCallbackInfo<IUIAction>
{
	static SUILuaCallbackInfo<IUIAction> CreateInfo(IUIAction* pAction, const char* eventName)
	{
		SUILuaCallbackInfo<IUIAction> res;
		res.eventName = eventName;
		res.actionName = pAction ? pAction->GetName() : "";
		return res;
	}

	bool Accept(const SUILuaCallbackInfo<IUIAction>& info) const
	{
		return (actionName == "" || info.actionName == actionName) && (eventName == "" || info.eventName == eventName);
	}

	void CallScript(SmartScriptTable pScript, const char* functionName, SmartScriptTable args, const SUILuaCallbackInfo<IUIAction>& info) const
	{
		Script::CallMethod(pScript, functionName, info.actionName.c_str(), info.eventName.c_str(), args);
	}

	string eventName;
	string actionName;
};

struct SUIActionLuaCallback : public SUILuaCallbackContainer<IUIAction>, public IUIActionListener
{
	SUIActionLuaCallback();
	~SUIActionLuaCallback();

	void         Clear();

	virtual void OnStart(IUIAction* pAction, const SUIArguments& args);
	virtual void OnEnd(IUIAction* pAction, const SUIArguments& args);
};

////////////////////////////////////////////////////////////////////////////
///////////////////////////// IUIEventSystem ///////////////////////////////
////////////////////////////////////////////////////////////////////////////
template<>
struct SUILuaCallbackInfo<IUIEventSystem>
{
	static SUILuaCallbackInfo<IUIEventSystem> CreateInfo(IUIEventSystem* pEventSystem, const char* eventName)
	{
		SUILuaCallbackInfo<IUIEventSystem> res;
		res.eventName = eventName;
		res.eventSystemName = pEventSystem ? pEventSystem->GetName() : "";
		return res;
	}

	bool Accept(const SUILuaCallbackInfo<IUIEventSystem>& info) const
	{
		return (eventSystemName == "" || info.eventSystemName == eventSystemName) && (eventName == "" || info.eventName == eventName);
	}

	void CallScript(SmartScriptTable pScript, const char* functionName, SmartScriptTable args, const SUILuaCallbackInfo<IUIEventSystem>& info) const
	{
		Script::CallMethod(pScript, functionName, info.eventSystemName.c_str(), info.eventName.c_str(), args);
	}

	string eventName;
	string eventSystemName;
};

struct SUIEventSystemLuaCallback : public SUILuaCallbackContainer<IUIEventSystem>
{
	~SUIEventSystemLuaCallback();

	void Init(IUIEventSystem* pEventSystem);
	void Clear();
	void OnEvent(IUIEventSystem* pEventSystem, const SUIEvent& event);

private:
	void AddNewListener(IUIEventSystem* pEventSystem);

	struct SEventSystemListener : public IUIEventListener
	{
		SEventSystemListener() : m_pEventSystem(nullptr), m_pOwner(nullptr) {}
		SEventSystemListener(IUIEventSystem* pEventSystem, SUIEventSystemLuaCallback* pOwner) { Init(pEventSystem, pOwner); }
		SEventSystemListener(const SEventSystemListener& other) { Init(other.m_pEventSystem, other.m_pOwner); }
		SEventSystemListener& operator=(const SEventSystemListener& other) { Clear(); Init(other.m_pEventSystem, other.m_pOwner); return *this; }
		~SEventSystemListener();

		virtual SUIArgumentsRet OnEvent(const SUIEvent& event);
		virtual void            OnEventSystemDestroyed(IUIEventSystem* pEventSystem);

		IUIEventSystem*         GetEventSystem() const { return m_pEventSystem; }

	private:
		void Init(IUIEventSystem* pEventSystem, SUIEventSystemLuaCallback* pOwner);
		void Clear();
		IUIEventSystem*            m_pEventSystem;
		SUIEventSystemLuaCallback* m_pOwner;
	};
	typedef std::vector<SEventSystemListener> TListener;
	TListener m_Listener;
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

class CScriptBind_UIAction
	: public CScriptableBase
	  , public IUIModule
{
public:
	CScriptBind_UIAction();
	virtual ~CScriptBind_UIAction();

	void         Release() { delete this; };

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	//! <code>UIAction.ReloadElement( elementName, instanceID )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//! <description>Reloads the UI flash asset.</description>
	int ReloadElement(IFunctionHandler* pH, const char* elementName, int instanceID);

	//! <code>UIAction.UnloadElement( elementName, instanceID )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//! <description>Unloads the UI flash asset.</description>
	int UnloadElement(IFunctionHandler* pH, const char* elementName, int instanceID);

	//! <code>UIAction.ShowElement( elementName, instanceID )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//! <description>Displays the UI flash asset.</description>
	int ShowElement(IFunctionHandler* pH, const char* elementName, int instanceID);

	//! <code>UIAction.HideElement( elementName, instanceID )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//! <description>Hide the UI flash asset.</description>
	int HideElement(IFunctionHandler* pH, const char* elementName, int instanceID);

	// <title SoftHideElement>
	//! <code>UIAction.RequestHide( elementName, instanceID )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//! <description>Send the fade out signal to the UI flash asset.</description>
	int RequestHide(IFunctionHandler* pH, const char* elementName, int instanceID);

	//! <code>UIAction.CallFunction( elementName, instanceID, functionName, [arg1], [arg2], [...] )</code>
	//!		<param name="elementName">UI Element name as defined in the xml or UIEventSystem name as defined via cpp.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances. If used on UIEventSystem no instance id is ignored.</param>
	//!		<param name="functionName">Function or event name.</param>
	//!		<param name="args">List of arguments (optional).</param>
	//! <description>Calls a function of the UI flash asset or the UIEventSystem.</description>
	int CallFunction(IFunctionHandler* pH, const char* elementName, int instanceID, const char* functionName);

	//! <code>UIAction.SetVariable( elementName, instanceID, varName, value )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="varName">Variable name as defined in the xml.</param>
	//!		<param name="value">Value to set.</param>
	//! <description>Sets a variable of the UI flash asset.</description>
	int SetVariable(IFunctionHandler* pH, const char* elementName, int instanceID, const char* varName);

	//! <code>UIAction.GetVariable( elementName, instanceID, varName )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="varName">Variable name as defined in the xml.</param>
	//! <description>Gets a variable of the UI flash asset.</description>
	int GetVariable(IFunctionHandler* pH, const char* elementName, int instanceID, const char* varName);

	//! <code>UIAction.SetArray( elementName, instanceID, arrayName, values )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="arrayName">Array name as defined in the xml.</param>
	//!		<param name="values">Table of values for the array.</param>
	//! <description>Sets an array of the UI flash asset.</description>
	int SetArray(IFunctionHandler* pH, const char* elementName, int instanceID, const char* arrayName, SmartScriptTable values);

	//! <code>UIAction.GetArray( elementName, instanceID, arrayName )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="arrayName">Array name as defined in the xml.</param>
	//! <description>Returns a table with values of the array.</description>
	int GetArray(IFunctionHandler* pH, const char* elementName, int instanceID, const char* arrayName);

	//! <code>UIAction.GotoAndPlay( elementName, instanceID, mcName, frameNum )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="mcName">MovieClip name as defined in the xml.</param>
	//!		<param name="frameNum">frame number.</param>
	//! <description>Call GotoAndPlay on a MovieClip.</description>
	int GotoAndPlay(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, int frameNum);

	//! <code>UIAction.GotoAndStop( elementName, instanceID, mcName, frameNum )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="mcName">MovieClip name as defined in the xml.</param>
	//!		<param name="frameNum">frame number.</param>
	//! <description>Call GotoAndStop on a MovieClip.</description>
	int GotoAndStop(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, int frameNum);

	//! <code>UIAction.GotoAndPlayFrameName( elementName, instanceID, mcName, frameName )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="mcName">MovieClip name as defined in the xml.</param>
	//!		<param name="frameName">frame name.</param>
	//! <description>Call GotoAndPlay on a MovieClip by frame name.</description>
	int GotoAndPlayFrameName(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, const char* frameName);

	//! <code>UIAction.GotoAndStopFrameName( elementName, instanceID, mcName, frameName )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="mcName">MovieClip name as defined in the xml.</param>
	//!		<param name="frameName">frame name.</param>
	//! <description>Call GotoAndStop on a MovieClip by frame name.</description>
	int GotoAndStopFrameName(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, const char* frameName);

	//! <code>UIAction.SetAlpha( elementName, instanceID, mcName, fAlpha )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="mcName">MovieClip name as defined in the xml.</param>
	//!		<param name="fAlpha">alpha value (0-1).</param>
	//! <description>Set MovieClip alpha value.</description>
	int SetAlpha(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, float fAlpha);

	//! <code>UIAction.GetAlpha( elementName, instanceID, mcName )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="mcName">MovieClip name as defined in the xml.</param>
	//! <description>Get MovieClip alpha value.</description>
	int GetAlpha(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName);

	//! <code>UIAction.SetVisible( elementName, instanceID, mcName, bVisible )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="mcName">MovieClip name as defined in the xml.</param>
	//!		<param name="bVisible">visible.</param>
	//! <description>Set MovieClip visible state.</description>
	int SetVisible(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, bool bVisible);

	//! <code>UIAction.IsVisible( elementName, instanceID, mcName )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="mcName">MovieClip name as defined in the xml.</param>
	//! <description>Get MovieClip visible state.</description>
	int IsVisible(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName);

	//! <code>UIAction.SetPos( elementName, instanceID, mcName, vPos )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="mcName">MovieClip name as defined in the xml.</param>
	//!		<param name="vPos">position.</param>
	//! <description>Set MovieClip position.</description>
	int SetPos(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, Vec3 vPos);

	//! <code>UIAction.GetPos( elementName, instanceID, mcName )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="mcName">MovieClip name as defined in the xml.</param>
	//! <description>Get MovieClip position.</description>
	int GetPos(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName);

	//! <code>UIAction.SetRotation( elementName, instanceID, mcName, vRotation )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="mcName">MovieClip name as defined in the xml.</param>
	//!		<param name="vRotation">rotation.</param>
	//! <description>Set MovieClip rotation.</description>
	int SetRotation(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, Vec3 vRotation);

	//! <code>UIAction.GetRotation( elementName, instanceID, mcName )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances</param>
	//!		<param name="mcName">MovieClip name as defined in the xml.</param>
	//! <description>Get MovieClip rotation.</description>
	int GetRotation(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName);

	//! <code>UIAction.SetScale( elementName, instanceID, mcName, vScale )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="mcName">MovieClip name as defined in the xml.</param>
	//!		<param name="vScale">scale.</param>
	//! <description>Set MovieClip scale.</description>
	int SetScale(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName, Vec3 vScale);

	//! <code>UIAction.GetScale( elementName, instanceID, mcName  )</code>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="mcName">MovieClip name as defined in the xml.</param>
	//! <description>Get MovieClip scale.</description>
	int GetScale(IFunctionHandler* pH, const char* elementName, int instanceID, const char* mcName);

	//! <code>UIAction.StartAction( actionName, arguments )</code>
	//!		<param name="actionName">UI Action name.</param>
	//!		<param name="arguments">arguments to pass to this action.</param>
	//! <description>Starts a UI Action.</description>
	int StartAction(IFunctionHandler* pH, const char* actionName, SmartScriptTable arguments);

	//! <code>UIAction.EndAction( table, disable, arguments )</code>
	//!		<param name="table">must be "self".</param>
	//!		<param name="disable">if true this action gets disabled on end.</param>
	//!		<param name="arguments">arguments to return from this action.</param>
	//! <description>Ends a UI Action. This can be only used withing a UIAction Lua script!</description>
	int EndAction(IFunctionHandler* pH, SmartScriptTable pTable, bool disable, SmartScriptTable arguments);

	//! <code>UIAction.EnableAction( actionName )</code>
	//!		<param name="actionName">UI Action name.</param>
	//! <description>Enables the UI Action.</description>
	int EnableAction(IFunctionHandler* pH, const char* actionName);

	//! <code>UIAction.DisableAction( actionName )</code>
	//!		<param name="actionName">UI Action name.</param>
	//! <description>Disables the UI Action.</description>
	int DisableAction(IFunctionHandler* pH, const char* actionName);

	//! <code>UIAction.RegisterElementListener( table, elementName, instanceID, eventName, callbackFunctionName )</code>
	//!		<param name="table">the script that receives the callback (can be "self" to refer the current script).</param>
	//!		<param name="elementName">UI Element name as defined in the xml.</param>
	//!		<param name="instanceID">ID of the instance (if instance with id does not exist, it will be created). '-1' for all instances.</param>
	//!		<param name="eventName">Name of the event that is fired from the UI Element - if empty string it will receive all events!</param>
	//!		<param name="callbackFunctionName">name of the script function that will receive the callback.</param>
	//! <description>
	//!		Register a callback function for a UIElement event.
	//!		Callback Function must have form: CallbackName(elementName, instanceId, eventName, argTable)
	//! </description>
	int RegisterElementListener(IFunctionHandler* pH, SmartScriptTable pTable, const char* elementName, int instanceID, const char* eventName, const char* callback);

	//! <code>UIAction.RegisterActionListener( table, actionName, eventName, callbackFunctionName )</code>
	//!		<param name="table">the script that receives the callback (can be "self" to refer the current script).</param>
	//!		<param name="actionName">UI Action name.</param>
	//!		<param name="eventName">Name of the event that is fired from the UI Action (can be "OnStart" or "OnEnd") - if empty string it will receive all events!</param>
	//!		<param name="callbackFunctionName">name of the script function that will receive the callback.</param>
	//! <description>
	//!		Register a callback function for a UIAction event.
	//!		Callback Function must have form: CallbackName(actionName, eventName, argTable)
	//! </description>
	int RegisterActionListener(IFunctionHandler* pH, SmartScriptTable pTable, const char* actionName, const char* eventName, const char* callback);

	//! <code>UIAction.RegisterEventSystemListener( table, eventSystem, eventName, callbackFunctionName )</code>
	//!		<param name="table">the script that receives the callback (can be "self" to refer the current script).</param>
	//!		<param name="eventSystem">UI Event System name</param>
	//!		<param name="eventName">Name of the event that is fired from the UI EventSystem - if empty string it will receive all events!</param>
	//!		<param name="callbackFunctionName">name of the script function that will receive the callback.</param>
	//! <description>
	//!		Register a callback function for a UIEventSystem event.
	//!		Callback Function must have form: CallbackName(actionName, eventName, argTable)
	//! </description>
	int RegisterEventSystemListener(IFunctionHandler* pH, SmartScriptTable pTable, const char* eventSystem, const char* eventName, const char* callback);

	//! <code>UIAction.UnregisterElementListener( table, callbackFunctionName )</code>
	//!		<param name="table">the script that receives the callback (can be "self" to refer the current script).</param>
	//!		<param name="callbackFunctionName">name of the script function that receives the callback. if "" all callbacks for this script will be removed.</param>
	//! <description>Unregister callback functions for a UIElement event.</description>
	int UnregisterElementListener(IFunctionHandler* pH, SmartScriptTable pTable, const char* callback);

	//! <code>UIAction.UnregisterActionListener( table, callbackFunctionName )</code>
	//!		<param name="table">the script that receives the callback (can be "self" to refer the current script).</param>
	//!		<param name="callbackFunctionName">name of the script function that receives the callback. if "" all callbacks for this script will be removed.</param>
	//! <description>Unregister callback functions for a UIAction event.</description>
	int UnregisterActionListener(IFunctionHandler* pH, SmartScriptTable pTable, const char* callback);

	//! <code>UIAction.UnregisterEventSystemListener( table, callbackFunctionName )</code>
	//!		<param name="table">the script that receives the callback (can be "self" to refer the current script).</param>
	//!		<param name="callbackFunctionName">name of the script function that receives the callback. if "" all callbacks for this script will be removed.</param>
	//! <description>Unregister callback functions for a UIEventSystem event.</description>
	int UnregisterEventSystemListener(IFunctionHandler* pH, SmartScriptTable pTable, const char* callback);

	// IUIModule
	virtual void Reload();
	virtual void Reset();
	// ~IUIModule

private:
	void            RegisterMethods();

	IUIElement*     GetElement(const char* sName, int instanceID, bool bSupressWarning = false);
	IUIAction*      GetAction(const char* sName);
	IUIEventSystem* GetEventSystem(const char* sName, IUIEventSystem::EEventSystemType type);

	SUIElementLuaCallback     m_ElementCallbacks;
	SUIActionLuaCallback      m_ActionCallbacks;
	SUIEventSystemLuaCallback m_EventSystemCallbacks;
};

#endif // #ifndef __SCRIPTBIND_UIACTION_H__
