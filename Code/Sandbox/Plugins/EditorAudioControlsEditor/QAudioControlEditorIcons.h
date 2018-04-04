// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QIcon>
#include "AudioAssets.h"

namespace ACE
{
inline QIcon GetItemTypeIcon(EItemType type)
{
	switch (type)
	{
	case EItemType::eItemType_Trigger:
		return QIcon(":Icons/Trigger.ico");
	case EItemType::eItemType_RTPC:
		return QIcon(":Icons/RTPC.ico");
	case EItemType::eItemType_Switch:
		return QIcon(":Icons/Switch.ico");
	case EItemType::eItemType_State:
		return QIcon(":Icons/Property.ico");
	case EItemType::eItemType_Environment:
		return QIcon(":Icons/Environment.ico");
	case EItemType::eItemType_Preload:
		return QIcon(":Icons/Bank.ico");
	case EItemType::eItemType_Folder:
		return QIcon(":Icons/Folder.ico");
	case EItemType::eItemType_Library:
		return QIcon("icons:Assets/GeomCache.ico");
	}
	return QIcon(":Icons/RTPC.ico");
}

}
