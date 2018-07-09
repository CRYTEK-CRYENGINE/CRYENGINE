// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "Controls/QObjectTreeWidget.h"

class CObjectClassDesc;

class QToolButton;
class QPreviewWidget;
class QStandardItem;

class CObjectCreateTreeWidget : public QObjectTreeWidget
{
public:
	enum Role
	{
		eRole_Path = QObjectTreeView::Roles::Id + 1,
		eRole_Material
	};

public:
	explicit CObjectCreateTreeWidget(CObjectClassDesc* pClassDesc, const char* szRegExp = "[/\\\\.]", QWidget* pParent = nullptr);

private:
	void UpdatePreviewWidget();

private:
	QToolButton*      m_pShowPreviewButton;
	QPreviewWidget*   m_pPreviewWidget;
	CObjectClassDesc* m_pClassDesc;
};

