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

enum class ERoles
{
	Id = Qt::UserRole + 1,
	Name,
	InternalPointer,
	IsDefaultControl,
	IsPlaceholder,
	SortPriority,
};

enum class EItemStatus
{
	Placeholder,
	NoConnection,
	NoControl,
	Localized,
	NotificationHeader,
};

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
}

//////////////////////////////////////////////////////////////////////////
inline CryIcon const& GetItemNotificationIcon(EItemStatus const status)
{
	switch (status)
	{
	case EItemStatus::Placeholder:
		return s_placeholderIcon;
		break;
	case EItemStatus::NoConnection:
		return s_noConnectionIcon;
		break;
	case EItemStatus::NoControl:
		return s_noControlIcon;
		break;
	case EItemStatus::Localized:
		return s_localizedIcon;
		break;
	case EItemStatus::NotificationHeader:
		return s_notificationHeaderIcon;
		break;
	default:
		return s_errorIcon;
		break;
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

static CItemModelAttribute s_notificationAttribute("Notification", eAttributeType_String, CItemModelAttribute::Visible, false);
static CItemModelAttribute s_placeholderAttribute("Valid Connection", eAttributeType_Boolean, CItemModelAttribute::AlwaysHidden, true, Qt::Unchecked);
static CItemModelAttribute s_connectedAttribute("Connected", eAttributeType_Boolean, CItemModelAttribute::AlwaysHidden, true, Qt::Unchecked);
static CItemModelAttribute s_localizedAttribute("Localized", eAttributeType_Boolean, CItemModelAttribute::AlwaysHidden, true, Qt::Checked);
static CItemModelAttribute s_noControlAttribute("Empty", eAttributeType_Boolean, CItemModelAttribute::AlwaysHidden, true, Qt::Checked);
static CItemModelAttribute s_pakStatus("Pak ", eAttributeType_String, CItemModelAttribute::StartHidden, false);
static CItemModelAttribute s_inPakAttribute("In pak", eAttributeType_Boolean, CItemModelAttribute::AlwaysHidden, true, Qt::Checked);
static CItemModelAttribute s_onDiskAttribute("On disk", eAttributeType_Boolean, CItemModelAttribute::AlwaysHidden, true, Qt::Checked);
static CItemModelAttribute s_pathAttribute("Path", eAttributeType_String, CItemModelAttribute::Visible, false);
} // namespace ModelUtils
} // namespace ACE
