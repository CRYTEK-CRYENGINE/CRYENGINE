// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

enum EItemType
{
	eItemType_Folder = 0,
	eItemType_AudioControl,
	eItemType_Root,
	eItemType_Invalid,
};

#include <qnamespace.h>

enum EDataRole
{
	eDataRole_Type = Qt::UserRole + 1,
	eDataRole_Id,
	eDataRole_Modified,
};
