// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IEditor.h>
#include <Include/IPlugin.h>
#include "ATLControlsModel.h"
#include "QATLControlsTreeModel.h"
#include <IAudioSystemEditor.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

#include <QStandardItem>

struct IAudioProxy;
class CImplementationManager;

namespace ACE
{
enum EErrorCode
{
	eErrorCode_NoError        = 0,
	eErrorCode_UnkownPlatform = BIT(0),
};
}

//------------------------------------------------------------------
class CAudioControlsEditorPlugin : public IPlugin, public ISystemEventListener
{
public:
	explicit CAudioControlsEditorPlugin(IEditor* editor);

	void                            Release() override;
	void                            ShowAbout() override                                 {}
	const char*                     GetPluginGUID() override                             { return "{DFA4AFF7-2C70-4B29-B736-GRH00040314}"; }
	DWORD                           GetPluginVersion() override                          { return 1; }
	const char*                     GetPluginName() override                             { return "AudioControlsEditor"; }
	bool                            CanExitNow() override                                { return true; }
	void                            OnEditorNotify(EEditorNotifyEvent aEventId) override {}

	static void                     SaveModels();
	static void                     ReloadModels();
	static void                     ReloadScopes();
	static ACE::CATLControlsModel*  GetATLModel();
	static ACE::QATLTreeModel*      GetControlsTree();
	static CImplementationManager*  GetImplementationManger();
	static ACE::IAudioSystemEditor* GetAudioSystemEditorImpl();
	static void                     ExecuteTrigger(const string& sTriggerName);
	static void                     StopTriggerExecution();
	static uint                     GetLoadingErrorMask() { return ms_loadingErrorMask; }

private:
	///////////////////////////////////////////////////////////////////////////
	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	///////////////////////////////////////////////////////////////////////////

	static ACE::CATLControlsModel ms_ATLModel;
	static ACE::QATLTreeModel     ms_layoutModel;
	static std::set<string>       ms_currentFilenames;
	static IAudioProxy*           ms_pIAudioProxy;
	static AudioControlId         ms_nAudioTriggerID;

	static CImplementationManager ms_implementationManager;
	static uint                   ms_loadingErrorMask;
};
