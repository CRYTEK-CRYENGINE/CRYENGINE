// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "UICommon.h"
#include "Core/Polygon.h"

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;

namespace Designer
{
class SmoothingGroupTool;
class SmoothingGroupPanel : public QWidget, public IBasePanel
{
public:
	SmoothingGroupPanel(SmoothingGroupTool* pSmoothingGroupTool);

	void Update() override
	{
		UpdateSmoothingGroupList();
	}

	QWidget* GetWidget() override
	{
		return this;
	}

private:
	void OnItemDoubleClicked(QTreeWidgetItem* item, int column);
	void OnItemChanged(QTreeWidgetItem* item, int column);

	void OnAddPolygons();
	void OnAddSmoothingGroup();
	void OnSelectPolygons();
	void OnDeleteSmoothingGroup();
	void OnAutoSmooth();
	void OnEditFinishingThresholdAngle();

	void UpdateSmoothingGroupList();

	SmoothingGroupTool* m_pSmoothingGroupTool;
	QTreeWidget*        m_pSmoothingGroupList;
	QString             m_ItemNameBeforeEdit;
	QLineEdit*          m_pThresholdAngleLineEdit;
};
}

