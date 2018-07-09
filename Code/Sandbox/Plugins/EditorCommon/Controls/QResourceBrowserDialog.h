// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include "EditorDialog.h"

class QObjectTreeWidget;
class QLabel;

class EDITOR_COMMON_API QResourceBrowserDialog : public CEditorDialog
{
	Q_OBJECT
public:
	QResourceBrowserDialog();

	void           PreviewResource(const char* szIconPath);
	void           ResourceSelected(const char* szIconPath);
	const QString& GetSelectedResource() const { return m_SelectedResource; }

protected:
	virtual QSize sizeHint() const override { return QSize(480, 480); }

protected:
	QObjectTreeWidget* m_pTreeWidget;
	QString            m_SelectedResource;
	QLabel*            m_pPreview;
};

