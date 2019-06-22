// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include <QScrollableBox.h>
#include <QtViewPane.h>

class QPropertyTreeLegacy;

class EDITOR_COMMON_API CViewportHelperWnd : public CDockableWidgetT<QScrollableBox>
{
public:
	CViewportHelperWnd(QWidget* pParent, CViewport* pViewport);

	void        Serialize(Serialization::IArchive& ar);
	QSize       sizeHint() const override { return QSize(250, 855); }
	const char* GetPaneTitle() const      { return "Helper Settings"; }

private:
	void showEvent(QShowEvent* e) override;
	void OnReset();

	CViewport*     m_pViewport;
	QPropertyTreeLegacy* m_propertyTree;
};
