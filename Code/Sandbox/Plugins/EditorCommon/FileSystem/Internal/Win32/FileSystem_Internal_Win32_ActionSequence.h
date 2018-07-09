// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>

namespace FileSystem
{
namespace Internal
{
namespace Win32
{

/// \brief internal win32 specific sequence of action to pathes
struct SActionSequence
{
	enum ActionType { Created, Removed, Modified, RenamedFrom, RenamedTo };

	struct SAction
	{
		ActionType type;
		QString    fullPath;
	};

	unsigned long    key;                  ///< unique key associated with the base path
	QString          baseFullAbsolutePath; ///< monitored base path (all pathes are relative to this)
	QVector<SAction> sequence;
};

} // namespace Win32
} // namespace Internal
} // namespace FileSystem

// allow to use with signal & slots
Q_DECLARE_METATYPE(FileSystem::Internal::Win32::SActionSequence)

