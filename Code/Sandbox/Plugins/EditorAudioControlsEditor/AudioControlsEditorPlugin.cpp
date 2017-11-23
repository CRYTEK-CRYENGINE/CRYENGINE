// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControlsEditorPlugin.h"

#include "SystemAssets.h"
#include "AudioControlsEditorWindow.h"
#include "AudioControlsLoader.h"
#include "AudioControlsWriter.h"
#include "ImplementationManager.h"

#include <IEditorImpl.h>
#include <CryAudio/IAudioSystem.h>
#include <CryAudio/IObject.h>
#include <CryMath/Cry_Camera.h>
#include <CrySystem/File/CryFile.h>
#include <CryString/CryPath.h>
#include <CryCore/Platform/platform_impl.inl>
#include <IUndoManager.h>
#include <QtViewPane.h>
#include <IResourceSelectorHost.h>

REGISTER_PLUGIN(ACE::CAudioControlsEditorPlugin);

namespace ACE
{
CSystemAssetsManager CAudioControlsEditorPlugin::s_assetsManager;
std::set<string> CAudioControlsEditorPlugin::s_currentFilenames;
CryAudio::IObject* CAudioControlsEditorPlugin::s_pIAudioObject = nullptr;
CryAudio::ControlId CAudioControlsEditorPlugin::s_audioTriggerId = CryAudio::InvalidControlId;
CImplementationManager CAudioControlsEditorPlugin::s_implementationManager;
EErrorCode CAudioControlsEditorPlugin::s_loadingErrorMask;
CCrySignal<void()> CAudioControlsEditorPlugin::signalAboutToLoad;
CCrySignal<void()> CAudioControlsEditorPlugin::signalLoaded;
CCrySignal<void()> CAudioControlsEditorPlugin::signalAboutToSave;
CCrySignal<void()> CAudioControlsEditorPlugin::signalSaved;

REGISTER_VIEWPANE_FACTORY(CAudioControlsEditorWindow, "Audio Controls Editor", "Tools", true)

//////////////////////////////////////////////////////////////////////////
CAudioControlsEditorPlugin::CAudioControlsEditorPlugin()
{
	CryAudio::SCreateObjectData const objectData("Audio trigger preview", CryAudio::EOcclusionType::Ignore);
	s_pIAudioObject = gEnv->pAudioSystem->CreateObject(objectData);

	s_assetsManager.Initialize();
	s_implementationManager.LoadImplementation();

	signalAboutToLoad();
	ReloadModels(false);
	signalLoaded();

	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CAudioControlsEditorPlugin");
}

//////////////////////////////////////////////////////////////////////////
CAudioControlsEditorPlugin::~CAudioControlsEditorPlugin()
{
	s_implementationManager.Release();

	if (s_pIAudioObject != nullptr)
	{
		StopTriggerExecution();
		gEnv->pAudioSystem->ReleaseObject(s_pIAudioObject);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsEditorPlugin::SaveModels()
{
	signalAboutToSave();
	IEditorImpl* pEditorImpl = s_implementationManager.GetImplementation();

	if (pEditorImpl != nullptr)
	{
		CAudioControlsWriter writer(&s_assetsManager, pEditorImpl, s_currentFilenames);
	}

	s_loadingErrorMask = EErrorCode::NoError;
	signalSaved();
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsEditorPlugin::ReloadModels(bool const reloadImplementation)
{
	// Do not call signalAboutToLoad and signalLoaded in here!
	GetIEditor()->GetIUndoManager()->Suspend();

	IEditorImpl* const pEditorImpl = s_implementationManager.GetImplementation();

	if (pEditorImpl != nullptr)
	{
		s_assetsManager.Clear();

		if (reloadImplementation)
		{
			pEditorImpl->Reload();
		}

		CAudioControlsLoader loader(&s_assetsManager);
		loader.LoadAll();

		s_currentFilenames = loader.GetLoadedFilenamesList();
		s_loadingErrorMask = loader.GetErrorCodeMask();
	}

	GetIEditor()->GetIUndoManager()->Resume();
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsEditorPlugin::ReloadScopes()
{
	s_assetsManager.ClearScopes();
	CAudioControlsLoader loader(&s_assetsManager);
	loader.LoadScopes();
}

//////////////////////////////////////////////////////////////////////////
CSystemAssetsManager* CAudioControlsEditorPlugin::GetAssetsManager()
{
	return &s_assetsManager;
}

//////////////////////////////////////////////////////////////////////////
IEditorImpl* CAudioControlsEditorPlugin::GetImplEditor()
{
	return s_implementationManager.GetImplementation();
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
		s_implementationManager.LoadImplementation();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
CImplementationManager* CAudioControlsEditorPlugin::GetImplementationManger()
{
	return &s_implementationManager;
}
} // namespace ACE
