// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Item.h"
#include <CryIcon.h>

namespace ACE
{
namespace Impl
{
namespace Wwise
{
static CryIcon s_errorIcon;
static CryIcon s_auxBusIcon;
static CryIcon s_eventIcon;
static CryIcon s_parameterIcon;
static CryIcon s_physicalFolderIcon;
static CryIcon s_soundBankIcon;
static CryIcon s_stateGroupIcon;
static CryIcon s_stateIcon;
static CryIcon s_switchGroupIcon;
static CryIcon s_switchIcon;
static CryIcon s_virtualFolderIcon;
static CryIcon s_workUnitIcon;

static QString const s_emptyTypeName("");
static QString const s_auxBusTypeName("Auxiliary Bus");
static QString const s_eventTypeName("Event");
static QString const s_parameterTypeName("Game Parameter");
static QString const s_physicalFolderTypeName("Physical Folder");
static QString const s_soundBankTypeName("SoundBank");
static QString const s_stateGroupTypeName("State Group");
static QString const s_stateTypeName("State");
static QString const s_switchGroupTypeName("Switch Group");
static QString const s_switchTypeName("Switch");
static QString const s_virtualFolderTypeName("Virtual Folder");
static QString const s_workUnitTypeName("Work Unit");

//////////////////////////////////////////////////////////////////////////
inline void InitIcons()
{
	s_errorIcon = CryIcon("icons:Dialogs/dialog-error.ico");
	s_auxBusIcon = CryIcon("icons:audio/impl/wwise/auxbus.ico");
	s_eventIcon = CryIcon("icons:audio/impl/wwise/event.ico");
	s_parameterIcon = CryIcon("icons:audio/impl/wwise/gameparameter.ico");
	s_physicalFolderIcon = CryIcon("icons:audio/impl/wwise/physicalfolder.ico");
	s_soundBankIcon = CryIcon("icons:audio/impl/wwise/soundbank.ico");
	s_stateGroupIcon = CryIcon("icons:audio/impl/wwise/stategroup.ico");
	s_stateIcon = CryIcon("icons:audio/impl/wwise/state.ico");
	s_switchGroupIcon = CryIcon("icons:audio/impl/wwise/switchgroup.ico");
	s_switchIcon = CryIcon("icons:audio/impl/wwise/switch.ico");
	s_virtualFolderIcon = CryIcon("icons:audio/impl/wwise/virtualfolder.ico");
	s_workUnitIcon = CryIcon("icons:audio/impl/wwise/workunit.ico");
}

//////////////////////////////////////////////////////////////////////////
inline CryIcon const& GetTypeIcon(EItemType const type)
{
	switch (type)
	{
	case EItemType::AuxBus:
		return s_auxBusIcon;
		break;
	case EItemType::Event:
		return s_eventIcon;
		break;
	case EItemType::Parameter:
		return s_parameterIcon;
		break;
	case EItemType::PhysicalFolder:
		return s_physicalFolderIcon;
		break;
	case EItemType::SoundBank:
		return s_soundBankIcon;
		break;
	case EItemType::StateGroup:
		return s_stateGroupIcon;
		break;
	case EItemType::State:
		return s_stateIcon;
		break;
	case EItemType::SwitchGroup:
		return s_switchGroupIcon;
		break;
	case EItemType::Switch:
		return s_switchIcon;
		break;
	case EItemType::VirtualFolder:
		return s_virtualFolderIcon;
		break;
	case EItemType::WorkUnit:
		return s_workUnitIcon;
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
	case EItemType::AuxBus:
		return s_auxBusTypeName;
		break;
	case EItemType::Event:
		return s_eventTypeName;
		break;
	case EItemType::Parameter:
		return s_parameterTypeName;
		break;
	case EItemType::PhysicalFolder:
		return s_physicalFolderTypeName;
		break;
	case EItemType::SoundBank:
		return s_soundBankTypeName;
		break;
	case EItemType::StateGroup:
		return s_stateGroupTypeName;
		break;
	case EItemType::State:
		return s_stateTypeName;
		break;
	case EItemType::SwitchGroup:
		return s_switchGroupTypeName;
		break;
	case EItemType::Switch:
		return s_switchTypeName;
		break;
	case EItemType::VirtualFolder:
		return s_virtualFolderTypeName;
		break;
	case EItemType::WorkUnit:
		return s_workUnitTypeName;
		break;
	default:
		return s_emptyTypeName;
		break;
	}
}
} // namespace Wwise
} // namespace Impl
} // namespace ACE
