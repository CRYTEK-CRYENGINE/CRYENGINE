// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <IPlugin.h>
#include <IEditor.h>

namespace ACE
{
enum class EReloadFlags : CryAudio::EnumFlagsType
{
	None = 0,
	ReloadSystemControls = BIT(0),
	ReloadImplData = BIT(1),
	SendSignals = BIT(2),
	BackupConnections = BIT(3), };
CRY_CREATE_ENUM_FLAG_OPERATORS(EReloadFlags);

class CAudioControlsEditorPlugin final : public IPlugin, public ISystemEventListener
{
public:

	CAudioControlsEditorPlugin(CAudioControlsEditorPlugin const&) = delete;
	CAudioControlsEditorPlugin(CAudioControlsEditorPlugin&&) = delete;
	CAudioControlsEditorPlugin& operator=(CAudioControlsEditorPlugin const&) = delete;
	CAudioControlsEditorPlugin& operator=(CAudioControlsEditorPlugin&&) = delete;

	CAudioControlsEditorPlugin();
	virtual ~CAudioControlsEditorPlugin() override;

	// IPlugin
	virtual int32       GetPluginVersion() override     { return 1; }
	virtual char const* GetPluginName() override        { return g_szEditorName; }
	virtual char const* GetPluginDescription() override { return "The Audio Controls Editor enables browsing and configuring audio events exposed from the audio middleware"; }
	// ~IPlugin

	static void SaveData();
	static void ReloadData(EReloadFlags const flags);
	static void ExecuteTrigger(string const& triggerName);
	static void ExecuteTriggerEx(string const& triggerName, XmlNodeRef const& node);
	static void StopTriggerExecution();
	static void OnAudioCallback(CryAudio::SRequestInfo const* const pRequestInfo);

	static CCrySignal<void()> SignalOnBeforeLoad;
	static CCrySignal<void()> SignalOnAfterLoad;
	static CCrySignal<void()> SignalOnBeforeSave;
	static CCrySignal<void()> SignalOnAfterSave;

private:

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

	static void ReloadImplData(EReloadFlags const flags);

	static FileNames           s_currentFilenames;
	static CryAudio::ControlId s_audioTriggerId;
};
} // namespace ACE
