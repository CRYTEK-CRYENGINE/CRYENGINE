// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QMenu>
#include <QAction>
#include <QFileInfo>

#include "EditorCommonAPI.h"

class QFileInfo;
class QWidget;

class EDITOR_COMMON_API CFilePopupMenu : public QMenu
{
	Q_OBJECT

public:
	struct SFilePopupMenuAction : public QAction
	{
		template<typename Func>
		SFilePopupMenuAction(const QString& text, QWidget* pParent, Func slot)
			: QAction(text, pParent)
		{
			connect(this, &SFilePopupMenuAction::triggered, slot);
		}
	};

public:
	explicit CFilePopupMenu(const QFileInfo& fileInfo, QWidget* pParent);

	const QFileInfo& fileInfo() const { return m_fileInfo; }

protected:
	QFileInfo m_fileInfo;
};

