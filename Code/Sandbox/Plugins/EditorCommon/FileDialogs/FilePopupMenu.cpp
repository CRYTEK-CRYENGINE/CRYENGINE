// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FilePopupMenu.h"

#include <QWidget>
#include <QFileInfo>
#include <QApplication>
#include <QClipboard>

CFilePopupMenu::CFilePopupMenu(const QFileInfo& fileInfo, QWidget* pParent)
	: QMenu(pParent)
	, m_fileInfo(fileInfo)
{
	addAction(new SFilePopupMenuAction(tr("Copy Name To Clipboard"), this, [this] { QApplication::clipboard()->setText(m_fileInfo.fileName());
	                                   }));
	addAction(new SFilePopupMenuAction(tr("Copy Path To Clipboard"), this, [this] { QApplication::clipboard()->setText(m_fileInfo.absoluteFilePath());
	                                   }));
}

