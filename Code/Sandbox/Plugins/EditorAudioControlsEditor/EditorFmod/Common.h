// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Item.h"
#include <CryIcon.h>
#include <CryAudioImplFmod/GlobalData.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
using ConnectionsByContext = std::map<CryAudio::ContextId, CryAudio::Impl::Fmod::SPoolSizes>;
extern ConnectionsByContext g_connections;

extern string g_language;

static CryIcon s_errorIcon;
static CryIcon s_bankIcon;
static CryIcon s_editorFolderIcon;
static CryIcon s_eventIcon;
static CryIcon s_keyIcon;
static CryIcon s_folderIcon;
static CryIcon s_mixerGroupIcon;
static CryIcon s_ParameterIcon;
static CryIcon s_returnIcon;
static CryIcon s_snapshotIcon;
static CryIcon s_vcaIcon;

static QString const s_emptyTypeName("");
static QString const s_bankTypeName("Bank");
static QString const s_editorFolderTypeName("Editor Folder");
static QString const s_eventTypeName("Event");
static QString const s_keyTypeName("Key");
static QString const s_folderTypeName("Folder");
static QString const s_mixerGroupTypeName("Mixer Group");
static QString const s_parameterTypeName("Parameter");
static QString const s_returnTypeName("Return");
static QString const s_snapshotTypeName("Snapshot");
static QString const s_vcaTypeName("VCA");

//////////////////////////////////////////////////////////////////////////
inline void InitIcons()
{
	s_errorIcon = CryIcon("icons:Dialogs/dialog-error.ico");
	s_bankIcon = CryIcon("icons:audio/impl/fmod/bank.ico");
	s_editorFolderIcon = CryIcon("icons:General/Folder.ico");
	s_eventIcon = CryIcon("icons:audio/impl/fmod/event.ico");
	s_keyIcon = CryIcon("icons:audio/impl/fmod/key.ico");
	s_folderIcon = CryIcon("icons:audio/impl/fmod/folder_closed.ico");
	s_mixerGroupIcon = CryIcon("icons:audio/impl/fmod/group.ico");
	s_ParameterIcon = CryIcon("icons:audio/impl/fmod/tag.ico");
	s_returnIcon = CryIcon("icons:audio/impl/fmod/return.ico");
	s_snapshotIcon = CryIcon("icons:audio/impl/fmod/snapshot.ico");
	s_vcaIcon = CryIcon("icons:audio/impl/fmod/vca.ico");
}

//////////////////////////////////////////////////////////////////////////
inline CryIcon const& GetTypeIcon(EItemType const type)
{
	switch (type)
	{
	case EItemType::Bank:
		{
			return s_bankIcon;
		}
	case EItemType::EditorFolder:
		{
			return s_editorFolderIcon;
		}
	case EItemType::Event:
		{
			return s_eventIcon;
		}
	case EItemType::Folder:
		{
			return s_folderIcon;
		}
	case EItemType::MixerGroup:
		{
			return s_mixerGroupIcon;
		}
	case EItemType::Parameter:
		{
			return s_ParameterIcon;
		}
	case EItemType::Return:
		{
			return s_returnIcon;
		}
	case EItemType::Key:
		{
			return s_keyIcon;
		}
	case EItemType::Snapshot:
		{
			return s_snapshotIcon;
		}
	case EItemType::VCA:
		{
			return s_vcaIcon;
		}
	default:
		{
			return s_errorIcon;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
inline QString const& TypeToString(EItemType const type)
{
	switch (type)
	{
	case EItemType::Bank:
		{
			return s_bankTypeName;
		}
	case EItemType::EditorFolder:
		{
			return s_editorFolderTypeName;
		}
	case EItemType::Event:
		{
			return s_eventTypeName;
		}
	case EItemType::Folder:
		{
			return s_folderTypeName;
		}
	case EItemType::MixerGroup:
		{
			return s_mixerGroupTypeName;
		}
	case EItemType::Parameter:
		{
			return s_parameterTypeName;
		}
	case EItemType::Return:
		{
			return s_returnTypeName;
		}
	case EItemType::Key:
		{
			return s_keyTypeName;
		}
	case EItemType::Snapshot:
		{
			return s_snapshotTypeName;
		}
	case EItemType::VCA:
		{
			return s_vcaTypeName;
		}
	default:
		{
			return s_emptyTypeName;
		}
	}
}
} // namespace Fmod
} // namespace Impl
} // namespace ACE
