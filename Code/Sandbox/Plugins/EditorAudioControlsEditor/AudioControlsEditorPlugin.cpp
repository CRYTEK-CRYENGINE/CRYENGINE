// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControlsEditorPlugin.h"

#include "AssetsManager.h"
#include "ContextManager.h"
#include "MainWindow.h"
#include "AudioControlsLoader.h"
#include "FileWriter.h"
#include "FileLoader.h"
#include "ImplManager.h"
#include "ListenerManager.h"
#include "NameValidator.h"
#include "AssetIcons.h"
#include "Common/IImpl.h"

#include <CryMath/Cry_Camera.h>
#include <CryCore/Platform/platform_impl.inl>
#include <IUndoManager.h>
#include <QtViewPane.h>
#include <ConfigurationManager.h>

REGISTER_PLUGIN(ACE::CAudioControlsEditorPlugin);

namespace ACE
{
FileNames CAudioControlsEditorPlugin::s_currentFilenames;
CryAudio::ControlId CAudioControlsEditorPlugin::s_audioTriggerId = CryAudio::InvalidControlId;
CCrySignal<void()> CAudioControlsEditorPlugin::SignalOnBeforeLoad;
CCrySignal<void()> CAudioControlsEditorPlugin::SignalOnAfterLoad;
CCrySignal<void()> CAudioControlsEditorPlugin::SignalOnBeforeSave;
CCrySignal<void()> CAudioControlsEditorPlugin::SignalOnAfterSave;

REGISTER_VIEWPANE_FACTORY(CMainWindow, g_szEditorName, "Tools", true)

//////////////////////////////////////////////////////////////////////////
CAudioControlsEditorPlugin::CAudioControlsEditorPlugin()
{
	InitAssetIcons();

	g_nameValidator.Initialize(s_regexInvalidFileName);
	g_assetsManager.Initialize();
	g_contextManager.Initialize();
	g_implManager.LoadImpl();
	g_listenerManager.Initialize();

	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CAudioControlsEditorPlugin");

	CryAudio::ESystemEvents const events = CryAudio::ESystemEvents::ContextActivated | CryAudio::ESystemEvents::ContextDeactivated;
	gEnv->pAudioSystem->AddRequestListener(&CAudioControlsEditorPlugin::OnAudioCallback, nullptr, events);
}

//////////////////////////////////////////////////////////////////////////
CAudioControlsEditorPlugin::~CAudioControlsEditorPlugin()
{
	g_implManager.Release();
	StopTriggerExecution();
	gEnv->pAudioSystem->RemoveRequestListener(nullptr, this);
	GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsEditorPlugin::SaveData()
{
	SignalOnBeforeSave();

	if (g_pIImpl != nullptr)
	{
		CFileWriter writer(s_currentFilenames);
		writer.WriteAll();
	}

	SignalOnAfterSave();
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsEditorPlugin::ReloadData(EReloadFlags const flags)
{
	if ((flags& EReloadFlags::SendSignals) != 0)
	{
		SignalOnBeforeLoad();
	}

	if ((flags& EReloadFlags::ReloadSystemControls) != 0)
	{
		GetIEditor()->GetIUndoManager()->Suspend();

		g_assetsManager.UpdateConfigFolderPath();
		g_assetsManager.Clear();
		g_contextManager.Clear();

		if (g_pIImpl != nullptr)
		{
			if ((flags& EReloadFlags::ReloadImplData) != 0)
			{
				ReloadImplData(flags);
			}

			CFileLoader loader;
			loader.CreateInternalControls();

#if defined (USE_BACKWARDS_COMPATIBILITY)
			// CAudioControlsLoader is deprecated and only used for backwards compatibility.  It will be removed with CE 5.7.
			CAudioControlsLoader loaderForBackwardsCompatibility;
			loaderForBackwardsCompatibility.LoadAll(true);
#endif  //  USE_BACKWARDS_COMPATIBILITY

			loader.Load();
			s_currentFilenames = loader.GetLoadedFilenamesList();

#if defined (USE_BACKWARDS_COMPATIBILITY)
			loaderForBackwardsCompatibility.LoadAll(false);
			auto const& fileNames = loaderForBackwardsCompatibility.GetLoadedFilenamesList();
#endif  //  USE_BACKWARDS_COMPATIBILITY

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

	if ((flags& EReloadFlags::SendSignals) != 0)
	{
		SignalOnAfterLoad();
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
		g_pIImpl->Reload(g_implInfo);

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
void CAudioControlsEditorPlugin::ExecuteTrigger(string const& triggerName)
{
	if (!triggerName.empty())
	{
		StopTriggerExecution();
		s_audioTriggerId = CryAudio::StringToId(triggerName.c_str());
		gEnv->pAudioSystem->ExecutePreviewTrigger(s_audioTriggerId);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsEditorPlugin::ExecuteTriggerEx(string const& triggerName, XmlNodeRef const& node)
{
	if (node.isValid())
	{
		StopTriggerExecution();
		s_audioTriggerId = CryAudio::StringToId(triggerName.c_str());
		gEnv->pAudioSystem->ExecutePreviewTriggerEx(node);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsEditorPlugin::StopTriggerExecution()
{
	if (s_audioTriggerId != CryAudio::InvalidControlId)
	{
		gEnv->pAudioSystem->StopPreviewTrigger();
		s_audioTriggerId = CryAudio::InvalidControlId;
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsEditorPlugin::OnAudioCallback(CryAudio::SRequestInfo const* const pRequestInfo)
{
	switch (pRequestInfo->systemEvent)
	{
	case CryAudio::ESystemEvents::ContextActivated:
		{
			auto const contextId = static_cast<CryAudio::ContextId>(reinterpret_cast<uintptr_t>(pRequestInfo->pUserData));
			g_contextManager.SetContextActive(contextId);

			break;
		}
	case CryAudio::ESystemEvents::ContextDeactivated:
		{
			auto const contextId = static_cast<CryAudio::ContextId>(reinterpret_cast<uintptr_t>(pRequestInfo->pUserData));
			g_contextManager.SetContextInactive(contextId);

			break;
		}
	default:
		{
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsEditorPlugin::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_AUDIO_IMPLEMENTATION_LOADED:
		{
			g_implManager.LoadImpl();
			break;
		}
	default:
		{
			break;
		}
	}
}
} // namespace ACE
