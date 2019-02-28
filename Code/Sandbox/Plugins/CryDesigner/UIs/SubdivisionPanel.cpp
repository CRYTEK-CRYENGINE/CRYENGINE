// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SubdivisionPanel.h"
#include "DesignerPanel.h"
#include "DesignerEditor.h"
#include "Tools/Modify/SubdivisionTool.h"
#include "Util/Undo.h"
#include "Core/Model.h"
#include "Core/EdgesSharpnessManager.h"
#include "Util/ElementSet.h"

#include <QGridLayout>
#include <QSlider>
#include <QTreeWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>

namespace Designer
{
SubdivisionPanel::SubdivisionPanel(SubdivisionTool* pSubdivisionTool) : m_pSubdivisionTool(pSubdivisionTool)
{
	QGridLayout* pGridLayout = new QGridLayout;

	m_pSubdivisionLevelSlider = new QSlider(Qt::Horizontal);
	m_pSubdivisionLevelSlider->setMinimum(0);
	m_pSubdivisionLevelSlider->setMaximum(4);
	m_pSubdivisionLevelSlider->setPageStep(1);
	m_pSubdivisionLevelSlider->setValue(pSubdivisionTool->GetModel()->GetSubdivisionLevel());

	m_pSemiSharpCreaseList = new QTreeWidget;
	m_pSemiSharpCreaseList->setFixedHeight(100);
	m_pSemiSharpCreaseList->setColumnCount(2);
	m_pSemiSharpCreaseList->setSelectionMode(QAbstractItemView::MultiSelection);
	QStringList headerLabels;
	headerLabels.append("Name");
	headerLabels.append("Sharpness");
	m_pSemiSharpCreaseList->setHeaderLabels(headerLabels);
	RefreshEdgeGroupList();

	QObject::connect(m_pSemiSharpCreaseList, &QTreeWidget::itemDoubleClicked, this, [=](QTreeWidgetItem* item, int column){ OnItemDoubleClicked(item, column); });
	QObject::connect(m_pSemiSharpCreaseList, &QTreeWidget::itemChanged, this, [=](QTreeWidgetItem* item, int column){ OnItemChanged(item, column); });
	QObject::connect(m_pSemiSharpCreaseList, &QTreeWidget::currentItemChanged, this, [=](QTreeWidgetItem* current, QTreeWidgetItem* previous){ OnCurrentItemChanged(current, previous); });

	QPushButton* pApplyButton = new QPushButton("Apply");
	QPushButton* pAddButton = new QPushButton("Add");
	QPushButton* pDeleteButton = new QPushButton("Delete");
	QPushButton* pDeleteUnusedButton = new QPushButton("Delete Unused");
	QPushButton* pClearButton = new QPushButton("Clear");
	QCheckBox* pSmoothingSurfaceButton = new QCheckBox("Smoothing Surface");
	QLabel* pLabel = new QLabel("Semi-sharp Creases");
	pLabel->setAlignment(Qt::AlignHCenter);

	pGridLayout->addWidget(m_pSubdivisionLevelSlider, 0, 0, 1, 6);
	pGridLayout->addWidget(pSmoothingSurfaceButton, 1, 0, 1, 3);
	pGridLayout->addWidget(pApplyButton, 1, 3, 1, 3);
	pGridLayout->addWidget(CreateHorizontalLine(), 2, 0, 1, 6);
	pGridLayout->addWidget(pLabel, 3, 0, 1, 6);
	pGridLayout->addWidget(m_pSemiSharpCreaseList, 4, 0, 2, 6);
	pGridLayout->addWidget(pAddButton, 6, 0, 1, 3);
	pGridLayout->addWidget(pDeleteButton, 6, 3, 1, 3);
	pGridLayout->addWidget(pDeleteUnusedButton, 7, 0, 1, 3);
	pGridLayout->addWidget(pClearButton, 7, 3, 1, 3);
	
	QObject::connect(m_pSubdivisionLevelSlider, &QAbstractSlider::valueChanged, this, [ = ]
		{
			CUndo undo("Designer Tool : Subdivision Level");
			CUndo::Record(new UndoSubdivision(m_pSubdivisionTool->GetMainContext()));
			SubdivisionTool::Subdivide(m_pSubdivisionLevelSlider->value(), true);
		});
	QObject::connect(pApplyButton, &QPushButton::clicked, this, [ = ] { OnApply(); });
	QObject::connect(pClearButton, &QPushButton::clicked, this, [ = ] { OnClear(); });
	QObject::connect(pDeleteButton, &QPushButton::clicked, this, [ = ] { OnDelete(); });
	QObject::connect(pDeleteUnusedButton, &QPushButton::clicked, this, [ = ] { OnDeleteUnused(); });
	QObject::connect(pAddButton, &QPushButton::clicked, this, [ = ] { AddSemiSharpGroup(); });
	QObject::connect(pSmoothingSurfaceButton, &QCheckBox::stateChanged, this, [ = ](int state)
		{
			if (state == Qt::Checked)
				m_pSubdivisionTool->GetModel()->EnableSmoothingSurfaceForSubdividedMesh(true);
			else if (state == Qt::Unchecked)
				m_pSubdivisionTool->GetModel()->EnableSmoothingSurfaceForSubdividedMesh(false);
			m_pSubdivisionTool->ApplyPostProcess(ePostProcess_Mesh | ePostProcess_SyncPrefab);
		});

	pSmoothingSurfaceButton->setChecked(m_pSubdivisionTool->GetModel()->IsSmoothingSurfaceForSubdividedMeshEnabled());

	setLayout(pGridLayout);
}

void SubdivisionPanel::Update()
{
	MainContext mc(m_pSubdivisionTool->GetMainContext());
	if (!mc.pModel)
		return;
	m_pSubdivisionLevelSlider->setValue(mc.pModel->GetSubdivisionLevel());
}

void SubdivisionPanel::AddSemiSharpGroup()
{
	m_pSubdivisionTool->AddNewEdgeTag();
	RefreshEdgeGroupList();
}

void SubdivisionPanel::RefreshEdgeGroupList()
{
	while (m_pSemiSharpCreaseList->topLevelItemCount() > 0)
		delete m_pSemiSharpCreaseList->takeTopLevelItem(0);

	EdgeSharpnessManager* pEdgeSharpnessMgr = m_pSubdivisionTool->GetModel()->GetEdgeSharpnessMgr();
	for (int i = 0, iCount(pEdgeSharpnessMgr->GetCount()); i < iCount; ++i)
	{
		const EdgeSharpness& sharpness = pEdgeSharpnessMgr->Get(i);
		QStringList strList;
		strList.append(sharpness.name.c_str());
		strList.append(QString("%1").arg(sharpness.sharpness));
		QTreeWidgetItem* pTreeWidget = new QTreeWidgetItem(strList);
		pTreeWidget->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled);
		m_pSemiSharpCreaseList->addTopLevelItem(pTreeWidget);
	}
}

void SubdivisionPanel::OnItemDoubleClicked(QTreeWidgetItem* item, int column)
{
	if (item == NULL)
		return;

	m_ItemNameBeforeEdit = item->text(column);
}

void SubdivisionPanel::OnItemChanged(QTreeWidgetItem* item, int column)
{
	if (item == NULL)
		return;

	EdgeSharpnessManager* pEdgeMgr = m_pSubdivisionTool->GetModel()->GetEdgeSharpnessMgr();

	if (column == 0)
	{
		string name = pEdgeMgr->GenerateValidName(item->text(column).toStdString().c_str());
		pEdgeMgr->Rename(m_ItemNameBeforeEdit.toStdString().c_str(), name);
	}
	else if (column == 1)
	{
		float sharpness = item->text(1).toFloat();
		pEdgeMgr->SetSharpness(item->text(0).toStdString().c_str(), sharpness);
		m_pSubdivisionTool->ApplyPostProcess(ePostProcess_Mesh | ePostProcess_SyncPrefab);
	}
}

void SubdivisionPanel::OnCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
	EdgeSharpnessManager* pEdgeMgr = m_pSubdivisionTool->GetModel()->GetEdgeSharpnessMgr();
	if (current)
		m_pSubdivisionTool->HighlightEdgeGroup(current->text(0).toStdString().c_str());
	else
		m_pSubdivisionTool->HighlightEdgeGroup(NULL);
}

void SubdivisionPanel::OnApply()
{
	CUndo undo("Subdivision : Apply");
	// reset the slider to reflect that subdivision has been applied
	m_pSubdivisionTool->GetModel()->RecordUndo("Subdivision : Apply", m_pSubdivisionTool->GetBaseObject());
	m_pSubdivisionTool->ApplySubdividedMesh();
	m_pSubdivisionLevelSlider->setValue(0);
}

void SubdivisionPanel::OnClear()
{
	EdgeSharpnessManager* pEdgeMgr = m_pSubdivisionTool->GetModel()->GetEdgeSharpnessMgr();
	pEdgeMgr->Clear();
	m_pSubdivisionTool->ApplyPostProcess(ePostProcess_Mesh | ePostProcess_SyncPrefab);
	RefreshEdgeGroupList();
}

void SubdivisionPanel::OnDelete()
{
	QTreeWidgetItem* pCurrentItem = m_pSemiSharpCreaseList->currentItem();
	if (pCurrentItem == NULL)
		return;
	m_pSubdivisionTool->DeleteEdgeTag(pCurrentItem->text(0).toStdString().c_str());
	RefreshEdgeGroupList();
}

void SubdivisionPanel::OnDeleteUnused()
{
	m_pSubdivisionTool->DeleteAllUnused();
	RefreshEdgeGroupList();
}
}

