// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IPlugin.h>
#include <IEditor.h>

#include "SystemAssetsManager.h"

#include <IEditorImpl.h>
#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CrySandbox/CrySignal.h>

namespace CryAudio
{
struct IObject;
} // namespace CryAudio

namespace ACE
{
class CImplementationManager;

class CAudioControlsEditorPlugin final : public IPlugin, public ISystemEventListener
{
public:

	explicit CAudioControlsEditorPlugin();
	virtual ~CAudioControlsEditorPlugin() override;

	int32                          GetPluginVersion() override     { return 1; }
	char const*                    GetPluginName() override        { return "Audio Controls Editor"; }
	char const*                    GetPluginDescription() override { return "The Audio Controls Editor enables browsing and configuring audio events exposed from the audio middleware"; }

	static void                    SaveModels();
	static void                    ReloadModels(bool const reloadImplementation);
	static void                    ReloadScopes();
	static CSystemAssetsManager*   GetAssetsManager();
	static CImplementationManager* GetImplementationManger();
	static IEditorImpl*            GetImplEditor();
	static void                    ExecuteTrigger(string const& sTriggerName);
	static void                    StopTriggerExecution();
	static EErrorCode              GetLoadingErrorMask() { return s_loadingErrorMask; }

	static CCrySignal<void()> signalAboutToLoad;
	static CCrySignal<void()> signalLoaded;
	static CCrySignal<void()> signalAboutToSave;
	static CCrySignal<void()> signalSaved;

private:

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

	static CSystemAssetsManager   s_assetsManager;
	static std::set<string>       s_currentFilenames;
	static CryAudio::IObject*     s_pIAudioObject;
	static CryAudio::ControlId    s_audioTriggerId;

	static CImplementationManager s_implementationManager;
	static EErrorCode             s_loadingErrorMask;
};
} // namespace ACE
