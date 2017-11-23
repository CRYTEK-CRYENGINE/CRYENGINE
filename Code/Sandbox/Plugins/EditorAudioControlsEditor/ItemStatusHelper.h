// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <EditorStyleHelper.h>

namespace ACE
{
enum class EItemStatus
{
	Placeholder = 0,
	NoConnection,
	NoControl,
	Localized,
};

inline QColor GetItemStatusColor(EItemStatus const status)
{
	switch (status)
	{
	case EItemStatus::Placeholder:
		return GetStyleHelper()->errorColor(); // red
	case EItemStatus::NoConnection:
		return QColor(255, 150, 50); // orange
	case EItemStatus::NoControl:
		return QColor(50, 150, 255); // blue
	case EItemStatus::Localized:
		return QColor(36, 180, 245); // cyan
	}
	return GetStyleHelper()->errorColor(); // red
}
} // namespace ACE
