// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControlsEditorPlugin.h"
#include "QtViewPane.h"
#include "AudioControlsEditorWindow.h"
#include "IResourceSelectorHost.h"
#include "AudioControlsLoader.h"
#include "AudioControlsWriter.h"
#include "AudioControl.h"
#include <IAudioSystemEditor.h>
#include <CryAudio/IAudioSystem.h>
#include <CryMath/Cry_Camera.h>

#include <CrySystem/File/CryFile.h>
#include <CryString/CryPath.h>
#include "ImplementationManager.h"

using namespace ACE;
using namespace PathUtil;

CATLControlsModel CAudioControlsEditorPlugin::ms_ATLModel;
QATLTreeModel CAudioControlsEditorPlugin::ms_layoutModel;
std::set<string> CAudioControlsEditorPlugin::ms_currentFilenames;
IAudioProxy* CAudioControlsEditorPlugin::ms_pIAudioProxy;
AudioControlId CAudioControlsEditorPlugin::ms_nAudioTriggerID;
CImplementationManager CAudioControlsEditorPlugin::ms_implementationManager;
uint CAudioControlsEditorPlugin::ms_loadingErrorMask;

REGISTER_VIEWPANE_FACTORY(CAudioControlsEditorWindow, "Audio Controls Editor", "Tools", true)

CAudioControlsEditorPlugin::CAudioControlsEditorPlugin(IEditor* editor)
{
	RegisterPlugin();
	RegisterModuleResourceSelectors(GetIEditor()->GetResourceSelectorHost());

	ms_pIAudioProxy = gEnv->pAudioSystem->GetFreeAudioProxy();
	if (ms_pIAudioProxy)
	{
		ms_pIAudioProxy->Initialize("Audio trigger preview");
		ms_pIAudioProxy->SetOcclusionType(eAudioOcclusionType_Ignore);
	}

	ms_implementationManager.LoadImplementation();
	ReloadModels();
	ms_layoutModel.Initialize(&ms_ATLModel);
	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this);
}

void CAudioControlsEditorPlugin::Release()
{
	UnregisterPlugin();
	ms_implementationManager.Release();
	if (ms_pIAudioProxy)
	{
		StopTriggerExecution();
		ms_pIAudioProxy->Release();
	}
	delete this;
}

void CAudioControlsEditorPlugin::SaveModels()
{
	ACE::IAudioSystemEditor* pImpl = ms_implementationManager.GetImplementation();
	if (pImpl)
	{
		CAudioControlsWriter writer(&ms_ATLModel, &ms_layoutModel, pImpl, ms_currentFilenames);
	}
	ms_loadingErrorMask = static_cast<uint>(EErrorCode::eErrorCode_NoError);
}

void CAudioControlsEditorPlugin::ReloadModels()
{
	GetIEditor()->SuspendUndo();
	ms_ATLModel.SetSuppressMessages(true);

	ACE::IAudioSystemEditor* pImpl = ms_implementationManager.GetImplementation();
	if (pImpl)
	{
		ms_layoutModel.clear();
		ms_ATLModel.Clear();
		pImpl->Reload();
		CAudioControlsLoader loader(&ms_ATLModel, &ms_layoutModel, pImpl);
		loader.LoadAll();
		ms_currentFilenames = loader.GetLoadedFilenamesList();
		ms_loadingErrorMask = loader.GetErrorCodeMask();
	}

	ms_ATLModel.SetSuppressMessages(false);
	GetIEditor()->ResumeUndo();
}

void CAudioControlsEditorPlugin::ReloadScopes()
{
	ACE::IAudioSystemEditor* pImpl = ms_implementationManager.GetImplementation();
	if (pImpl)
	{
		ms_ATLModel.ClearScopes();
		CAudioControlsLoader loader(&ms_ATLModel, &ms_layoutModel, pImpl);
		loader.LoadScopes();
	}
}

CATLControlsModel* CAudioControlsEditorPlugin::GetATLModel()
{
	return &ms_ATLModel;
}

ACE::IAudioSystemEditor* CAudioControlsEditorPlugin::GetAudioSystemEditorImpl()
{
	return ms_implementationManager.GetImplementation();
}

QATLTreeModel* CAudioControlsEditorPlugin::GetControlsTree()
{
	return &ms_layoutModel;
}

void CAudioControlsEditorPlugin::ExecuteTrigger(const string& sTriggerName)
{
	if (!sTriggerName.empty() && ms_pIAudioProxy)
	{
		StopTriggerExecution();
		gEnv->pAudioSystem->GetAudioTriggerId(sTriggerName.c_str(), ms_nAudioTriggerID);

		if (ms_nAudioTriggerID != INVALID_AUDIO_CONTROL_ID)
		{
			const CCamera& camera = GetIEditor()->GetSystem()->GetViewCamera();

			SAudioRequest request;
			request.flags = eAudioRequestFlags_PriorityNormal;

			const Matrix34& cameraMatrix = camera.GetMatrix();

			SAudioListenerRequestData<eAudioListenerRequestType_SetTransformation> requestData(cameraMatrix);
			request.pData = &requestData;
			gEnv->pAudioSystem->PushRequest(request);

			ms_pIAudioProxy->SetTransformation(cameraMatrix);
			ms_pIAudioProxy->ExecuteTrigger(ms_nAudioTriggerID);
		}
	}
}

void CAudioControlsEditorPlugin::StopTriggerExecution()
{
	if (ms_pIAudioProxy && ms_nAudioTriggerID != INVALID_AUDIO_CONTROL_ID)
	{
		ms_pIAudioProxy->StopTrigger(ms_nAudioTriggerID);
		ms_nAudioTriggerID = INVALID_AUDIO_CONTROL_ID;
	}
}

void CAudioControlsEditorPlugin::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_AUDIO_IMPLEMENTATION_LOADED:
		GetIEditor()->SuspendUndo();
		ms_implementationManager.LoadImplementation();
		GetIEditor()->ResumeUndo();
		break;
	}
}

CImplementationManager* CAudioControlsEditorPlugin::GetImplementationManger()
{
	return &ms_implementationManager;
}
