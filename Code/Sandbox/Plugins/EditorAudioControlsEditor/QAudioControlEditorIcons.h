// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QIcon>
#include "AudioControl.h"

namespace ACE
{
inline QIcon GetControlTypeIcon(EACEControlType type)
{
	switch (type)
	{
	case ACE::eACEControlType_Trigger:
		return QIcon(":Icons/Trigger.ico");
		break;
	case ACE::eACEControlType_RTPC:
		return QIcon(":Icons/RTPC.ico");
		break;
	case ACE::eACEControlType_Switch:
		return QIcon(":Icons/Switch.ico");
		break;
	case ACE::eACEControlType_State:
		return QIcon(":Icons/Property.ico");
		break;
	case ACE::eACEControlType_Environment:
		return QIcon(":Icons/Environment.ico");
		break;
	case ACE::eACEControlType_Preload:
		return QIcon(":Icons/Bank.ico");
		break;
	}
	return QIcon(":Icons/RTPC.ico");
}

inline QIcon GetFolderIcon()
{
	return QIcon(":Icons/Folder.ico");
}

inline QIcon GetSoundBankIcon()
{
	return QIcon(":Icons/Preload.ico");
}
}
