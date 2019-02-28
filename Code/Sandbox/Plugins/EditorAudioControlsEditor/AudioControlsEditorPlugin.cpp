// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControlsEditorPlugin.h"

#include "MainWindow.h"
#include "AudioControlsLoader.h"
#include "FileWriter.h"
#include "FileLoader.h"
#include "ImplementationManager.h"
#include "AssetIcons.h"

#include <CryAudio/IObject.h>
#include <CryMath/Cry_Camera.h>
#include <CryCore/Platform/platform_impl.inl>
#include <IUndoManager.h>
#include <QtViewPane.h>
#include <ConfigurationManager.h>

REGISTER_PLUGIN(ACE::CAudioControlsEditorPlugin);

namespace ACE
{
CAssetsManager g_assetsManager;
CImplementationManager g_implementationManager;
Platforms g_platforms;
FileNames CAudioControlsEditorPlugin::s_currentFilenames;
CryAudio::IObject* CAudioControlsEditorPlugin::s_pIAudioObject = nullptr;
CryAudio::ControlId CAudioControlsEditorPlugin::s_audioTriggerId = CryAudio::InvalidControlId;
EErrorCode CAudioControlsEditorPlugin::s_loadingErrorMask;
CCrySignal<void()> CAudioControlsEditorPlugin::SignalAboutToLoad;
CCrySignal<void()> CAudioControlsEditorPlugin::SignalLoaded;
CCrySignal<void()> CAudioControlsEditorPlugin::SignalAboutToSave;
CCrySignal<void()> CAudioControlsEditorPlugin::SignalSaved;

REGISTER_VIEWPANE_FACTORY(CMainWindow, "Audio Controls Editor", "Tools", true)

//////////////////////////////////////////////////////////////////////////
CAudioControlsEditorPlugin::CAudioControlsEditorPlugin()
{
	CryAudio::SCreateObjectData const objectData("Audio trigger preview", CryAudio::EOcclusionType::Ignore);
	s_pIAudioObject = gEnv->pAudioSystem->CreateObject(objectData);

	InitPlatforms();
	InitAssetIcons();

	g_assetsManager.Initialize();
	g_implementationManager.LoadImplementation();

	ReloadData(EReloadFlags::ReloadSystemControls | EReloadFlags::SendSignals);

	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CAudioControlsEditorPlugin");
}

//////////////////////////////////////////////////////////////////////////
CAudioControlsEditorPlugin::~CAudioControlsEditorPlugin()
{
	g_implementationManager.Release();

	if (s_pIAudioObject != nullptr)
	{
		StopTriggerExecution();
		gEnv->pAudioSystem->ReleaseObject(s_pIAudioObject);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsEditorPlugin::SaveData()
{
	SignalAboutToSave();

	if (g_pIImpl != nullptr)
	{
		CFileWriter writer(s_currentFilenames);
		writer.WriteAll();
	}

	s_loadingErrorMask = EErrorCode::None;
	SignalSaved();
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsEditorPlugin::ReloadData(EReloadFlags const flags)
{
	if ((flags& EReloadFlags::SendSignals) != 0)
	{
		SignalAboutToLoad();
	}

	if ((flags& EReloadFlags::ReloadSystemControls) != 0)
	{
		GetIEditor()->GetIUndoManager()->Suspend();

		g_assetsManager.UpdateFolderPaths();
		g_assetsManager.Clear();

		if (g_pIImpl != nullptr)
		{
			if ((flags& EReloadFlags::ReloadImplData) != 0)
			{
				ReloadImplData(flags);
			}

			CFileLoader loader;
			loader.CreateInternalControls();

			// CAudioControlsLoader is deprecated and only used for backwards compatibility. It will be removed before March 2019.
			CAudioControlsLoader loaderForBackwardsCompatibility;
			loaderForBackwardsCompatibility.LoadAll(true);

			loader.LoadAll();
			s_currentFilenames = loader.GetLoadedFilenamesList();
			s_loadingErrorMask = loader.GetErrorCodeMask();

			loaderForBackwardsCompatibility.LoadAll(false);
			auto const& fileNames = loaderForBackwardsCompatibility.GetLoadedFilenamesList();

			for (auto const& name : fileNames)
			{
				s_currentFilenames.emplace(name);
			}
		}

		GetIEditor()->GetIUndoManager()->Resume();
	}
	else if ((flags& EReloadFlags::ReloadImplData) != 0)
	{
		ReloadImplData(flags);
	}

	if ((flags& EReloadFlags::ReloadScopes) != 0)
	{
		g_assetsManager.ClearScopes();

		CFileLoader loader;
		loader.LoadScopes();

		// CAudioControlsLoader is deprecated and only used for backwards compatibility. It will be removed before March 2019.
		CAudioControlsLoader loaderForBackwardsCompatibility;
		loaderForBackwardsCompatibility.LoadScopes();
	}

	if ((flags& EReloadFlags::SendSignals) != 0)
	{
		SignalLoaded();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsEditorPlugin::ReloadImplData(EReloadFlags const flags)
{
	if (g_pIImpl != nullptr)
	{
		if ((flags& EReloadFlags::BackupConnections) != 0)
		{
			g_assetsManager.BackupAndClearAllConnections();
		}

		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_COMMENT, "[Audio Controls Editor] Reloading audio implementation data");
		g_pIImpl->Reload();

		if ((flags& EReloadFlags::SetPlatforms) != 0)
		{
			g_pIImpl->SetPlatforms(g_platforms);
		}

		if ((flags& EReloadFlags::BackupConnections) != 0)
		{
			g_assetsManager.ReloadAllConnections();
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "[Audio Controls Editor] Data from middleware is empty.");
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsEditorPlugin::ExecuteTrigger(string const& sTriggerName)
{
	if (!sTriggerName.empty() && (s_pIAudioObject != nullptr))
	{
		StopTriggerExecution();
		CCamera const& camera = GetIEditor()->GetSystem()->GetViewCamera();
		Matrix34 const& cameraMatrix = camera.GetMatrix();
		s_pIAudioObject->SetTransformation(cameraMatrix);
		s_audioTriggerId = CryAudio::StringToId(sTriggerName.c_str());
		s_pIAudioObject->ExecuteTrigger(s_audioTriggerId);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsEditorPlugin::StopTriggerExecution()
{
	if (s_pIAudioObject && (s_audioTriggerId != CryAudio::InvalidControlId))
	{
		s_pIAudioObject->StopTrigger(s_audioTriggerId);
		s_audioTriggerId = CryAudio::InvalidControlId;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsEditorPlugin::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_AUDIO_IMPLEMENTATION_LOADED:
		g_implementationManager.LoadImplementation();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsEditorPlugin::InitPlatforms()
{
	g_platforms.clear();
	std::vector<dll_string> const& platforms = GetIEditor()->GetConfigurationManager()->GetPlatformNames();

	for (auto const& platform : platforms)
	{
		g_platforms.push_back(platform.c_str());
	}
}
} // namespace ACE

