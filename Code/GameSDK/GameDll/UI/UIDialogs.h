// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   UIDialogs.h
//  Version:     v1.00
//  Created:     22/6/2012 by Paul Reindell.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __UIDialogs_H__
#define __UIDialogs_H__

#include "IUIGameEventSystem.h"
#include <CrySystem/Scaleform/IFlashUI.h>
#include <CryGame/IGameFramework.h>

enum EDialogType
{
	eDT_DialogWait = 0,	// displays a wait dialog (no user action)
	eDT_Warning,				// displays warning string
	eDT_Error,					// displays error string
	eDT_AcceptDecline,	// displayes accept/decline dialog
	eDT_Confirm,				// displays confirmation dialog
	eDT_Okay,						// displays ok dialog
	eDT_Input,					// displays input string dialog
};

enum EDialogResponse
{
	eDR_Yes = 0,
	eDR_No,
	eDR_Canceled,
};

struct IDialogCallback
{
	virtual ~IDialogCallback() {}
	virtual void DialogCallback(uint32 dialogId, EDialogResponse response, const char* param) = 0;
};

class CUIDialogs
	: public IUIGameEventSystem
{
public:
	CUIDialogs();

	// IUIGameEventSystem
	UIEVENTSYSTEM( "UIDialogs" );
	virtual void InitEventSystem() override;
	virtual void UnloadEventSystem() override;

	// displays dialog
	uint32 DisplayDialog(EDialogType type, const char* title, const char* message, const char* paramMessage, IDialogCallback* pListener = NULL);
	void CancelDialog(uint32 dialogId);
	void CancelDialogs();

private:
	// ui events
	void OnDialogResult( int dialogid, int result, const char* message );

private:
	enum EUIEvent
	{
		eUIE_DisplayDialogAsset,
		eUIE_HideDialogAsset,

		eUIE_AddDialogWait,
		eUIE_AddDialogWarning,
		eUIE_AddDialogError,
		eUIE_AddDialogAcceptDecline,
		eUIE_AddDialogConfirm,
		eUIE_AddDialogOkay,
		eUIE_AddDialogInput,

		eUIE_RemoveDialog
	};

	SUIEventReceiverDispatcher<CUIDialogs> m_eventDispatcher;
	SUIEventSenderDispatcher<EUIEvent> m_eventSender;
	IUIEventSystem* m_pUIEvents;
	IUIEventSystem* m_pUIFunctions;
	typedef std::map<uint32, IDialogCallback*> TDialogs;
	TDialogs m_dialogs;

	inline uint32 GetNextFreeId() const
	{
		uint32 id = 0;
		for (TDialogs::const_iterator it = m_dialogs.begin(), end = m_dialogs.end(); it != end && id == it->first; ++it, ++id);
		return id;
	}
};


#endif // __UISettings_H__