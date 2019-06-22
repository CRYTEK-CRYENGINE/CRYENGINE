// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SharedData.h"
#include <ProxyModels/ItemModelAttribute.h>
#include <CryIcon.h>

namespace ACE
{
namespace ModelUtils
{
static CryIcon s_errorIcon;

static CryIcon s_localizedIcon;
static CryIcon s_noConnectionIcon;
static CryIcon s_noControlIcon;
static CryIcon s_notificationHeaderIcon;
static CryIcon s_placeholderIcon;

static CryIcon s_pakFileFolderIcon;
static CryIcon s_pakFileIcon;
static CryIcon s_pakFolderIcon;

static CryIcon s_contextActiveIcon;

enum class ERoles : CryAudio::EnumFlagsType
{
	Id = Qt::UserRole + 1,
	InternalPointer,
	IsDefaultControl,
	IsPlaceholder,
	SortPriority, };

enum class EItemStatus : CryAudio::EnumFlagsType
{
	Placeholder,
	NoConnection,
	NoControl,
	Localized,
	NotificationHeader, };

//////////////////////////////////////////////////////////////////////////
inline void InitIcons()
{
	s_errorIcon = CryIcon("icons:Dialogs/dialog-error.ico");

	s_localizedIcon = CryIcon("icons:audio/notifications/localized.ico");
	s_noConnectionIcon = CryIcon("icons:audio/notifications/no_connection.ico");
	s_noControlIcon = CryIcon("icons:audio/notifications/no_control.ico");
	s_notificationHeaderIcon = CryIcon("icons:General/Scripting.ico");
	s_placeholderIcon = CryIcon("icons:audio/notifications/placeholder.ico");

	s_pakFileFolderIcon = CryIcon("icons:General/Pakfile_Folder.ico");
	s_pakFileIcon = CryIcon("icons:General/Pakfile.ico");
	s_pakFolderIcon = CryIcon("icons:General/Folder.ico");

	s_contextActiveIcon = CryIcon("icons:General/colour_green.ico");
}

//////////////////////////////////////////////////////////////////////////
inline CryIcon const& GetItemNotificationIcon(EItemStatus const status)
{
	switch (status)
	{
	case EItemStatus::Placeholder:
		{
			return s_placeholderIcon;
		}
	case EItemStatus::NoConnection:
		{
			return s_noConnectionIcon;
		}
	case EItemStatus::NoControl:
		{
			return s_noControlIcon;
		}
	case EItemStatus::Localized:
		{
			return s_localizedIcon;
		}
	case EItemStatus::NotificationHeader:
		{
			return s_notificationHeaderIcon;
		}
	default:
		{
			return s_errorIcon;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
inline CryIcon const& GetPakStatusIcon(EPakStatus const pakStatus)
{
	if ((pakStatus == (EPakStatus::InPak | EPakStatus::OnDisk)))
	{
		return s_pakFileFolderIcon;
	}
	else if ((pakStatus& EPakStatus::InPak) != 0)
	{
		return s_pakFileIcon;
	}
	else if ((pakStatus& EPakStatus::OnDisk) != 0)
	{
		return s_pakFolderIcon;
	}

	return s_errorIcon;
}

static char const* const s_szSystemMimeType = "AudioSystemItems";
static char const* const s_szImplMimeType = "AudioImplItems";

static CItemModelAttribute s_notificationAttribute("Notification", &Attributes::s_stringAttributeType, CItemModelAttribute::Visible, false);
static CItemModelAttribute s_placeholderAttribute("Valid Connection", &Attributes::s_booleanAttributeType, CItemModelAttribute::AlwaysHidden, true, Qt::Unchecked, Qt::CheckStateRole);
static CItemModelAttribute s_connectedAttribute("Connected", &Attributes::s_booleanAttributeType, CItemModelAttribute::AlwaysHidden, true, Qt::Unchecked, Qt::CheckStateRole);
static CItemModelAttribute s_localizedAttribute("Localized", &Attributes::s_booleanAttributeType, CItemModelAttribute::AlwaysHidden, true, Qt::Checked, Qt::CheckStateRole);
static CItemModelAttribute s_noControlAttribute("Empty", &Attributes::s_booleanAttributeType, CItemModelAttribute::AlwaysHidden, true, Qt::Checked, Qt::CheckStateRole);
static CItemModelAttribute s_pakStatus("Pak ", &Attributes::s_stringAttributeType, CItemModelAttribute::StartHidden, false);
static CItemModelAttribute s_inPakAttribute("In pak", &Attributes::s_booleanAttributeType, CItemModelAttribute::AlwaysHidden, true, Qt::Checked, Qt::CheckStateRole);
static CItemModelAttribute s_onDiskAttribute("On disk", &Attributes::s_booleanAttributeType, CItemModelAttribute::AlwaysHidden, true, Qt::Checked, Qt::CheckStateRole);
static CItemModelAttribute s_pathAttribute("Path", &Attributes::s_stringAttributeType, CItemModelAttribute::Visible, false);
} // namespace ModelUtils
} // namespace ACE
