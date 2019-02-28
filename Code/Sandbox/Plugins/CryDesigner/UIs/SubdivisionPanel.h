// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "UICommon.h"

class QSlider;
class QTreeWidget;
class QTreeWidgetItem;

namespace Designer
{
class SubdivisionTool;
class SubdivisionPanel : public QWidget, public IBasePanel
{
public:
	SubdivisionPanel(SubdivisionTool* pTool);

	void     Update() override;
	QWidget* GetWidget() override { return this; }

protected:
	void OnApply();
	void OnDelete();
	void OnDeleteUnused();
	void OnClear();
	void OnItemDoubleClicked(QTreeWidgetItem* item, int column);
	void OnItemChanged(QTreeWidgetItem* item, int column);
	void OnCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);

	void AddSemiSharpGroup();
	void RefreshEdgeGroupList();

	QSlider*         m_pSubdivisionLevelSlider;
	QTreeWidget*     m_pSemiSharpCreaseList;
	SubdivisionTool* m_pSubdivisionTool;
	QString          m_ItemNameBeforeEdit;
};
}

