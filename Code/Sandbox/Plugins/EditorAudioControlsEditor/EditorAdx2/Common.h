// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Item.h"
#include <CryIcon.h>
#include <CryAudioImplAdx2/GlobalData.h>

namespace ACE
{
namespace Impl
{
namespace Adx2
{
using ConnectionsByContext = std::map<CryAudio::ContextId, CryAudio::Impl::Adx2::SPoolSizes>;
extern ConnectionsByContext g_connections;

static CryIcon s_errorIcon;
static CryIcon s_aisacControlIcon;
static CryIcon s_binaryIcon;
static CryIcon s_busIcon;
static CryIcon s_categoryIcon;
static CryIcon s_categoryGroupIcon;
static CryIcon s_cueIcon;
static CryIcon s_cueSheetIcon;
static CryIcon s_dspBusSettingIcon;
static CryIcon s_folderGreyIcon;
static CryIcon s_folderRedIcon;
static CryIcon s_folderYellowIcon;
static CryIcon s_gameVariableIcon;
static CryIcon s_selectorIcon;
static CryIcon s_selectorLabelIcon;
static CryIcon s_snapshotIcon;
static CryIcon s_workUnitIcon;

static QString const s_emptyTypeName("");
static QString const s_aiscacControlTypeName("AISAC Control");
static QString const s_binaryTypeName("Cue Sheet Binary");
static QString const s_busTypeName("Bus");
static QString const s_categoryTypeName("Category");
static QString const s_categoryGroupTypeName("Category Group");
static QString const s_cueTypeName("Cue");
static QString const s_cueSheetTypeName("Cue Sheet");
static QString const s_dspBusSettingTypeName("DSP Bus Setting");
static QString const s_folderTypeName("Folder");
static QString const s_gameVariableTypeName("Game Variable");
static QString const s_selectorTypeName("Selector");
static QString const s_selectorLabelTypeName("Selector Label");
static QString const s_snapshotTypeName("Snapshot");
static QString const s_workUnitTypeName("Work Unit");

//////////////////////////////////////////////////////////////////////////
inline void InitIcons()
{
	s_errorIcon = CryIcon("icons:Dialogs/dialog-error.ico");
	s_aisacControlIcon = CryIcon("icons:audio/impl/adx2/aisaccontrol.ico");
	s_binaryIcon = CryIcon("icons:audio/impl/adx2/binary.ico");
	s_busIcon = CryIcon("icons:audio/impl/adx2/bus.ico");
	s_categoryIcon = CryIcon("icons:audio/impl/adx2/category.ico");
	s_categoryGroupIcon = CryIcon("icons:audio/impl/adx2/categorygroup.ico");
	s_cueIcon = CryIcon("icons:audio/impl/adx2/cue.ico");
	s_cueSheetIcon = CryIcon("icons:audio/impl/adx2/cuesheet.ico");
	s_dspBusSettingIcon = CryIcon("icons:audio/impl/adx2/dspbussetting.ico");
	s_folderGreyIcon = CryIcon("icons:audio/impl/adx2/folder_grey.ico");
	s_folderRedIcon = CryIcon("icons:audio/impl/adx2/folder_red.ico");
	s_folderYellowIcon = CryIcon("icons:audio/impl/adx2/folder_yellow.ico");
	s_gameVariableIcon = CryIcon("icons:audio/impl/adx2/gamevariable.ico");
	s_selectorIcon = CryIcon("icons:audio/impl/adx2/selector.ico");
	s_selectorLabelIcon = CryIcon("icons:audio/impl/adx2/selectorlabel.ico");
	s_snapshotIcon = CryIcon("icons:audio/impl/adx2/snapshot.ico");
	s_workUnitIcon = CryIcon("icons:audio/impl/adx2/workunit.ico");
}

//////////////////////////////////////////////////////////////////////////
inline CryIcon const& GetTypeIcon(EItemType const type)
{
	switch (type)
	{
	case EItemType::AisacControl:
		{
			return s_aisacControlIcon;
		}
	case EItemType::Binary:
		{
			return s_binaryIcon;
		}
	case EItemType::Bus:
		{
			return s_busIcon;
		}
	case EItemType::Category:
		{
			return s_categoryIcon;
		}
	case EItemType::CategoryGroup:
		{
			return s_categoryGroupIcon;
		}
	case EItemType::Cue:
		{
			return s_cueIcon;
		}
	case EItemType::CueSheet:
		{
			return s_cueSheetIcon;
		}
	case EItemType::DspBusSetting:
		{
			return s_dspBusSettingIcon;
		}
	case EItemType::FolderCue:
		{
			return s_folderGreyIcon;
		}
	case EItemType::FolderGlobal:
		{
			return s_folderRedIcon;
		}
	case EItemType::FolderCueSheet:
		{
			return s_folderYellowIcon;
		}
	case EItemType::GameVariable:
		{
			return s_gameVariableIcon;
		}
	case EItemType::Selector:
		{
			return s_selectorIcon;
		}
	case EItemType::SelectorLabel:
		{
			return s_selectorLabelIcon;
		}
	case EItemType::Snapshot:
		{
			return s_snapshotIcon;
		}
	case EItemType::WorkUnit:
		{
			return s_workUnitIcon;
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
	case EItemType::AisacControl:
		{
			return s_aiscacControlTypeName;
		}
	case EItemType::Binary:
		{
			return s_binaryTypeName;
		}
	case EItemType::Bus:
		{
			return s_busTypeName;
		}
	case EItemType::Category:
		{
			return s_categoryTypeName;
		}
	case EItemType::CategoryGroup:
		{
			return s_categoryGroupTypeName;
		}
	case EItemType::Cue:
		{
			return s_cueTypeName;
		}
	case EItemType::CueSheet:
		{
			return s_cueSheetTypeName;
		}
	case EItemType::DspBusSetting:
		{
			return s_dspBusSettingTypeName;
		}
	case EItemType::FolderCue:    // Intentional fall-through.
	case EItemType::FolderGlobal: // Intentional fall-through.
	case EItemType::FolderCueSheet:
		{
			return s_folderTypeName;
		}
	case EItemType::GameVariable:
		{
			return s_gameVariableTypeName;
		}
	case EItemType::Selector:
		{
			return s_selectorTypeName;
		}
	case EItemType::SelectorLabel:
		{
			return s_selectorLabelTypeName;
		}
	case EItemType::Snapshot:
		{
			return s_snapshotTypeName;
		}
	case EItemType::WorkUnit:
		{
			return s_workUnitTypeName;
		}
	default:
		{
			return s_emptyTypeName;
		}
	}
}
} // namespace Adx2
} // namespace Impl
} // namespace ACE
