// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SmoothingGroupPanel.h"
#include "DesignerPanel.h"
#include "Tools/Surface/SmoothingGroupTool.h"

#include <QGridLayout>
#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QHeaderView>

namespace Designer
{
SmoothingGroupPanel::SmoothingGroupPanel(SmoothingGroupTool* pSmoothingGroupTool) :
	m_pSmoothingGroupTool(pSmoothingGroupTool)
{
	QGridLayout* pGridLayout = new QGridLayout;

	m_pSmoothingGroupList = new QTreeWidget;
	m_pSmoothingGroupList->setFixedHeight(100);
	m_pSmoothingGroupList->setColumnCount(2);
	m_pSmoothingGroupList->setSelectionMode(QAbstractItemView::MultiSelection);
	QStringList headerLabels;
	headerLabels.append("Name");
	headerLabels.append("Polygon Count");
	m_pSmoothingGroupList->setHeaderLabels(headerLabels);

	QObject::connect(m_pSmoothingGroupList, &QTreeWidget::itemDoubleClicked, this, [=](QTreeWidgetItem* item, int column){ OnItemDoubleClicked(item, column); });
	QObject::connect(m_pSmoothingGroupList, &QTreeWidget::itemChanged, this, [=](QTreeWidgetItem* item, int column){ OnItemChanged(item, column); });

	QPushButton* pAddSmoothingGroupButton = new QPushButton("Add SG");
	QPushButton* pDeleteSmoothingGroup = new QPushButton("Delete SG");
	QPushButton* pAddPolygonsButton = new QPushButton("Add Polygons");
	QPushButton* pSelectButton = new QPushButton("Select Polygons");
	QPushButton* pAutoSmoothButton = new QPushButton("Auto Smooth");
	m_pThresholdAngleLineEdit = new QLineEdit(QString("%1").arg(m_pSmoothingGroupTool->GetThresholdAngle()));

	pGridLayout->addWidget(m_pSmoothingGroupList, 0, 0, 2, 6);

	pGridLayout->addWidget(pAddSmoothingGroupButton, 2, 0, 1, 3);
	pGridLayout->addWidget(pDeleteSmoothingGroup, 2, 3, 1, 3);
	pGridLayout->addWidget(pAddPolygonsButton, 3, 0, 1, 3);
	pGridLayout->addWidget(pSelectButton, 3, 3, 1, 3);
	pGridLayout->addWidget(pAutoSmoothButton, 4, 0, 1, 3);
	pGridLayout->addWidget(new QLabel("Threshold Angle"), 5, 0, 1, 2);
	pGridLayout->addWidget(m_pThresholdAngleLineEdit, 5, 2, 1, 4);

	QObject::connect(pAddPolygonsButton, &QPushButton::clicked, this, [ = ] { OnAddPolygons();
	                 });
	QObject::connect(pDeleteSmoothingGroup, &QPushButton::clicked, this, [ = ] { OnDeleteSmoothingGroup();
	                 });
	QObject::connect(pAddSmoothingGroupButton, &QPushButton::clicked, this, [ = ] { OnAddSmoothingGroup();
	                 });
	QObject::connect(pSelectButton, &QPushButton::clicked, this, [ = ] { OnSelectPolygons();
	                 });
	QObject::connect(pAutoSmoothButton, &QPushButton::clicked, this, [ = ] { OnAutoSmooth();
	                 });
	QObject::connect(m_pThresholdAngleLineEdit, &QLineEdit::editingFinished, this, [ = ] { OnEditFinishingThresholdAngle();
	                 });

	setLayout(pGridLayout);
	UpdateSmoothingGroupList();
}

void SmoothingGroupPanel::OnItemDoubleClicked(QTreeWidgetItem* item, int column)
{
	if (item == NULL)
		return;

	m_ItemNameBeforeEdit = item->text(column);
}

void SmoothingGroupPanel::OnItemChanged(QTreeWidgetItem* item, int column)
{
	if (item == NULL)
		return;

	if (column == 0)
	{
		QString itemname = item->text(column);
		if (!m_pSmoothingGroupTool->RenameSmoothingGroup(m_ItemNameBeforeEdit.toStdString().c_str(), itemname.toStdString().c_str()))
			item->setText(column, m_ItemNameBeforeEdit);
	}
	else
	{
		item->setText(column, m_ItemNameBeforeEdit);
	}
}

void SmoothingGroupPanel::OnAddPolygons()
{
	QTreeWidgetItem* pCurrentItem = m_pSmoothingGroupList->currentItem();
	if (pCurrentItem == NULL)
		return;

	m_pSmoothingGroupTool->AddPolygonsToSmoothingGroup(pCurrentItem->text(0).toStdString().c_str());
	UpdateSmoothingGroupList();
}

void SmoothingGroupPanel::OnAddSmoothingGroup()
{
	m_pSmoothingGroupTool->AddSmoothingGroup();
	UpdateSmoothingGroupList();
}

void SmoothingGroupPanel::OnSelectPolygons()
{
	QTreeWidgetItem* pCurrentItem = m_pSmoothingGroupList->currentItem();
	if (pCurrentItem == NULL)
		return;

	m_pSmoothingGroupTool->SelectPolygons(pCurrentItem->text(0).toStdString().c_str());
}

void SmoothingGroupPanel::OnDeleteSmoothingGroup()
{
	QTreeWidgetItem* pCurrentItem = m_pSmoothingGroupList->currentItem();
	if (pCurrentItem == NULL)
		return;

	m_pSmoothingGroupTool->DeleteSmoothingGroup(pCurrentItem->text(0).toStdString().c_str());
	UpdateSmoothingGroupList();
}

void SmoothingGroupPanel::OnAutoSmooth()
{
	m_pSmoothingGroupTool->ApplyAutoSmooth();
	UpdateSmoothingGroupList();
}

void SmoothingGroupPanel::OnEditFinishingThresholdAngle()
{
	m_pSmoothingGroupTool->SetThresholdAngle(m_pThresholdAngleLineEdit->text().toFloat());
}

void SmoothingGroupPanel::UpdateSmoothingGroupList()
{
	m_pSmoothingGroupList->clear();

	std::vector<std::pair<string, SmoothingGroupPtr>> smoothingGroupList = m_pSmoothingGroupTool->GetSmoothingGroupList();
	for (auto ii = smoothingGroupList.begin(); ii != smoothingGroupList.end(); ++ii)
	{
		QStringList strList;
		strList.append((*ii).first.c_str());
		strList.append(QString("%1").arg((*ii).second->GetPolygonCount()));
		QTreeWidgetItem* pTreeWidget = new QTreeWidgetItem(strList);
		pTreeWidget->setFlags(Qt::ItemIsEnabled | Qt::ItemIsEditable);
		m_pSmoothingGroupList->addTopLevelItem(pTreeWidget);
	}
}
}

