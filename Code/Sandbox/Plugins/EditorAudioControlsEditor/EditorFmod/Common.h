// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Item.h"
#include <CryIcon.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
static CryIcon s_errorIcon;
static CryIcon s_bankIcon;
static CryIcon s_editorFolderIcon;
static CryIcon s_eventIcon;
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
		return s_bankIcon;
		break;
	case EItemType::EditorFolder:
		return s_editorFolderIcon;
		break;
	case EItemType::Event:
		return s_eventIcon;
		break;
	case EItemType::Folder:
		return s_folderIcon;
		break;
	case EItemType::MixerGroup:
		return s_mixerGroupIcon;
		break;
	case EItemType::Parameter:
		return s_ParameterIcon;
		break;
	case EItemType::Return:
		return s_returnIcon;
		break;
	case EItemType::Snapshot:
		return s_snapshotIcon;
		break;
	case EItemType::VCA:
		return s_vcaIcon;
		break;
	default:
		return s_errorIcon;
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
inline QString const& TypeToString(EItemType const type)
{
	switch (type)
	{
	case EItemType::Bank:
		return s_bankTypeName;
		break;
	case EItemType::EditorFolder:
		return s_editorFolderTypeName;
		break;
	case EItemType::Event:
		return s_eventTypeName;
		break;
	case EItemType::Folder:
		return s_folderTypeName;
		break;
	case EItemType::MixerGroup:
		return s_mixerGroupTypeName;
		break;
	case EItemType::Parameter:
		return s_parameterTypeName;
		break;
	case EItemType::Return:
		return s_returnTypeName;
		break;
	case EItemType::Snapshot:
		return s_snapshotTypeName;
		break;
	case EItemType::VCA:
		return s_vcaTypeName;
		break;
	default:
		return s_emptyTypeName;
		break;
	}
}
} // namespace Fmod
} // namespace Impl
} // namespace ACE
