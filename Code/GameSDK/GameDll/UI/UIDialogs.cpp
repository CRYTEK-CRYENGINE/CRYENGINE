// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   UIDialogs.cpp
//  Version:     v1.00
//  Created:     22/6/2012 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "UIDialogs.h"
#include "UIManager.h"

#include <CryFlowGraph/IFlowBaseNode.h>

////////////////////////////////////////////////////////////////////////////
CUIDialogs::CUIDialogs()
	: m_pUIEvents(NULL)
	, m_pUIFunctions(NULL)
{
}

////////////////////////////////////////////////////////////////////////////
void CUIDialogs::InitEventSystem()
{
	if (!gEnv->pFlashUI) return;

	// events to send from this class to UI flowgraphs
	m_pUIFunctions = gEnv->pFlashUI->CreateEventSystem("Dialogs", IUIEventSystem::eEST_SYSTEM_TO_UI);
	m_eventSender.Init(m_pUIFunctions);

	{
		SUIEventDesc eventDesc("ShowDialogAsset", "Displays error dialog flash asset");
		m_eventSender.RegisterEvent<eUIE_DisplayDialogAsset>(eventDesc);
	}

	{
		SUIEventDesc eventDesc("HideDialogAsset", "Hides error dialog flash asset");
		m_eventSender.RegisterEvent<eUIE_HideDialogAsset>(eventDesc);
	}

	{
		SUIEventDesc eventDesc("DisplayWaitDialog", "Display wait dialog - this dialog should stay until it is removed via RemoveDialog event");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_Int>("DialogId", "Dialog ID");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Title", "Dialog title");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Message", "Dialog message");
		m_eventSender.RegisterEvent<eUIE_AddDialogWait>(eventDesc);
	}

	{
		SUIEventDesc eventDesc("DisplayWarning", "Display error dialog");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_Int>("DialogId", "Dialog ID");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Title", "Dialog title");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Message", "Dialog message");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Button1", "Button1 String");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Button2", "Button2 String");
		m_eventSender.RegisterEvent<eUIE_AddDialogWarning>(eventDesc);
	}

	{
		SUIEventDesc eventDesc("DisplayError", "Display error dialog");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_Int>("DialogId", "Dialog ID");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Title", "Dialog title");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Message", "Dialog message");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Button1", "Button1 String");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Button2", "Button2 String");
		m_eventSender.RegisterEvent<eUIE_AddDialogError>(eventDesc);
	}

	{
		SUIEventDesc eventDesc("DisplayAcceptDecline", "Display accept/decline dialog");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_Int>("DialogId", "Dialog ID");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Title", "Dialog title");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Message", "Dialog message");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Button1", "Button1 String");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Button2", "Button2 String");
		m_eventSender.RegisterEvent<eUIE_AddDialogAcceptDecline>(eventDesc);
	}

	{
		SUIEventDesc eventDesc("DisplayConfirm", "Display confirm dialog");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_Int>("DialogId", "Dialog ID");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Title", "Dialog title");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Message", "Dialog message");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Button1", "Button1 String");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Button2", "Button2 String");
		m_eventSender.RegisterEvent<eUIE_AddDialogConfirm>(eventDesc);
	}

	{
		SUIEventDesc eventDesc("DisplayOk", "Display ok dialog");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_Int>("DialogId", "Dialog ID");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Title", "Dialog title");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Message", "Dialog message");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Button1", "Button1 String");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Button2", "Button2 String");
		m_eventSender.RegisterEvent<eUIE_AddDialogOkay>(eventDesc);
	}

	{
		SUIEventDesc eventDesc("DisplayInputDialog", "Display input dialog");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_Int>("DialogId", "Dialog ID");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Title", "Dialog title");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Message", "Dialog message");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Default", "Input field default value");
		m_eventSender.RegisterEvent<eUIE_AddDialogInput>(eventDesc);
	}

	{
		SUIEventDesc eventDesc("RemoveDialog", "Removes a dialog from screen");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_Int>("DialogId", "Dialog ID");
		m_eventSender.RegisterEvent<eUIE_RemoveDialog>(eventDesc);
	}

	// events that can be sent from UI flowgraphs to this class
	m_pUIEvents = gEnv->pFlashUI->CreateEventSystem("Dialogs", IUIEventSystem::eEST_UI_TO_SYSTEM);
	m_eventDispatcher.Init(m_pUIEvents, this, "UIDialogs");

	{
		SUIEventDesc eventDesc("DialogResult", "Call this to return a dialog result");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_Int>("DialogId", "Dialog ID");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_Int>("DialogResult", "Result (0=Yes/Ok/Confirm, 1=No/Cancel/Decline)");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Message", "Dialog Message (e.g. for input dialog)");
		m_eventDispatcher.RegisterEvent(eventDesc, &CUIDialogs::OnDialogResult);
	}
}

////////////////////////////////////////////////////////////////////////////
void CUIDialogs::UnloadEventSystem()
{
}

////////////////////////////////////////////////////////////////////////////
uint32 CUIDialogs::DisplayDialog(EDialogType type, const char* title, const char* message, const char* paramMessage, IDialogCallback* pListener)
{
	if (m_dialogs.size() == 0)
		m_eventSender.SendEvent<eUIE_DisplayDialogAsset>();

	uint32 id = GetNextFreeId();
	m_dialogs[id] = pListener;
	SUIArguments params(paramMessage);
	string param1;
	string param2;
	params.GetArg(0, param1);
	params.GetArg(1, param2);
	switch (type)
	{
	case eDT_DialogWait:
		m_eventSender.SendEvent<eUIE_AddDialogWait>((int)id, title, message);
		break;
	case eDT_Warning:
		m_eventSender.SendEvent<eUIE_AddDialogWarning>((int)id, title, message, param1, param2);
		break;
	case eDT_Error:
		m_eventSender.SendEvent<eUIE_AddDialogError>((int)id, title, message, param1, param2);
		break;
	case eDT_AcceptDecline:
		m_eventSender.SendEvent<eUIE_AddDialogAcceptDecline>((int)id, title, message, param1, param2);
		break;
	case eDT_Confirm:
		m_eventSender.SendEvent<eUIE_AddDialogConfirm>((int)id, title, message, param1, param2);
		break;
	case eDT_Okay:
		m_eventSender.SendEvent<eUIE_AddDialogOkay>((int)id, title, message, param1, param2);
		break;
	case eDT_Input:
		m_eventSender.SendEvent<eUIE_AddDialogInput>((int)id, title, message, paramMessage);
		break;
	default:
		assert(false);
	}
	return id;
}

////////////////////////////////////////////////////////////////////////////
void CUIDialogs::CancelDialog(uint32 dialogId)
{
	OnDialogResult(dialogId, eDR_Canceled, "");
}

////////////////////////////////////////////////////////////////////////////
void CUIDialogs::CancelDialogs()
{
	for (TDialogs::const_iterator it = m_dialogs.begin(); it != m_dialogs.end(); ++it)
		OnDialogResult(it->first, eDR_Canceled, "");
}

////////////////////////////////////////////////////////////////////////////
// ui events
////////////////////////////////////////////////////////////////////////////
void CUIDialogs::OnDialogResult(int dialogid, int result, const char* message)
{
	TDialogs::iterator it = m_dialogs.find(dialogid);
	assert(it != m_dialogs.end());
	if (it != m_dialogs.end())
	{
		if (it->second)
			it->second->DialogCallback(it->first, EDialogResponse(result), message);

		m_eventSender.SendEvent<eUIE_RemoveDialog>(it->first);
		m_dialogs.erase(it);

		if (m_dialogs.size() == 0)
			m_eventSender.SendEvent<eUIE_HideDialogAsset>();
	}
}

////////////////////////////////////////////////////////////////////////////
REGISTER_UI_EVENTSYSTEM(CUIDialogs);

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

class CDialogDisplayNode
	: public CFlowBaseNode<eNCT_Instanced>
	  , public IDialogCallback
{
public:
	CDialogDisplayNode(SActivationInfo* pActInfo)
		: m_bDisplayed(false)
		, m_id(~0)
	{
		m_pDialogMan = UIEvents::Get<CUIDialogs>();
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_AnyType("DisplayDialog", _HELP("Displays a dialog")),
			InputPortConfig_AnyType("CancelDialog",  _HELP("Cancels the current dialog")),
			InputPortConfig<int>("Type",             0,                                              _HELP("Dialog Type"),0, _UICONFIG("enum_int:WaitDialog=0,WarningDialog=1,ErrorDialog=2,AcceptDeclineDialog=3,ConfirmDialog=4,OkayDialog=5,InputDialog=5")),
			InputPortConfig<string>("Title",         _HELP("Dialog title")),
			InputPortConfig<string>("Message",       _HELP("Dialog message")),
			InputPortConfig<string>("Param",         _HELP("Special param (e.g. for input dialog)")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_AnyType("OnShow", _HELP("Triggers on Show")),
			OutputPortConfig<int>("OnResult",  _HELP("Returns once the dialog returns: 0=Yes/Ok/Confirm, 1=No/Cancel/Decline, 2=Canceled by system")),
			OutputPortConfig<string>("Param",  _HELP("Special param (e.g. for input dialog)")),
			{ 0 }
		};

		config.sDescription = _HELP("This node displays a dialog");
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (!m_pDialogMan)
			return;

		switch (event)
		{
		case eFE_Initialize:
			if (m_bDisplayed)
				m_pDialogMan->CancelDialog(m_id);
			m_ActInfo = *pActInfo;
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, eI_Display))
			{
				if (m_bDisplayed)
				{
					gEnv->pLog->LogWarning("FG: Dialog is already displayed!");
					ActivateOutput(pActInfo, eO_OnShow, true);
					return;
				}
				EDialogType type = (EDialogType) GetPortInt(pActInfo, eI_Type);
				const string title = GetPortString(pActInfo, eI_Title);
				const string message = GetPortString(pActInfo, eI_Message);
				const string param = GetPortString(pActInfo, eI_Param);
				m_id = m_pDialogMan->DisplayDialog(type, title.c_str(), message.c_str(), param.c_str(), this);
				m_bDisplayed = true;
				ActivateOutput(pActInfo, eO_OnShow, true);
			}
			if (IsPortActive(pActInfo, eI_Cancel))
			{
				if (!m_bDisplayed)
				{
					gEnv->pLog->LogWarning("FG: Dialog is not displayed!");
					ActivateOutput(&m_ActInfo, eO_Param, string(""));
					ActivateOutput(&m_ActInfo, eO_Result, 2);
					return;
				}
				m_pDialogMan->CancelDialog(m_id);
			}
			break;
		}
	}

	virtual void DialogCallback(uint32 dialogId, EDialogResponse response, const char* input)
	{
		assert(m_id == dialogId && m_bDisplayed);
		ActivateOutput(&m_ActInfo, eO_Param, string(input));
		ActivateOutput(&m_ActInfo, eO_Result, (int)response);
		m_bDisplayed = false;
	}

	virtual void         GetMemoryUsage(ICrySizer* s) const { s->Add(*this); }
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)   { return new CDialogDisplayNode(pActInfo); }

private:
	enum EInputs
	{
		eI_Display = 0,
		eI_Cancel,
		eI_Type,
		eI_Title,
		eI_Message,
		eI_Param
	};
	enum EOutputs
	{
		eO_OnShow = 0,
		eO_Result,
		eO_Param,
	};

	CUIDialogs*     m_pDialogMan;
	SActivationInfo m_ActInfo;
	bool            m_bDisplayed;
	uint32          m_id;
};
////////////////////////////////////////////////////////////////////////////
REGISTER_FLOW_NODE("UI:Functions:Dialogs:DisplayDialog", CDialogDisplayNode);
