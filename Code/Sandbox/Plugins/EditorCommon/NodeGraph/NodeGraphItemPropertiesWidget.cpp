// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NodeGraphItemPropertiesWidget.h"

#include "AbstractNodeGraphViewModelItem.h"
#include "NodeGraphUndo.h"

#include <QAdvancedPropertyTree.h>

namespace CryGraphEditor {

CNodeGraphItemPropertiesWidget::CNodeGraphItemPropertiesWidget(GraphItemSet& items)
{
	for (CAbstractNodeGraphViewModelItem* pAbstractItem : items)
	{
		m_structs.push_back(Serialization::SStruct(*pAbstractItem));
	}
	SetupPropertyTree();
}

CNodeGraphItemPropertiesWidget::CNodeGraphItemPropertiesWidget(CAbstractNodeGraphViewModelItem& item)
{
	m_structs.push_back(Serialization::SStruct(item));
	SetupPropertyTree();
}

CNodeGraphItemPropertiesWidget::~CNodeGraphItemPropertiesWidget()
{

}

void CNodeGraphItemPropertiesWidget::showEvent(QShowEvent* pEvent)
{
	QScrollableBox::showEvent(pEvent);

	if (m_pPropertyTree)
		m_pPropertyTree->setSizeToContent(true);
}

void CNodeGraphItemPropertiesWidget::SetupPropertyTree()
{
	m_pPropertyTree = new QAdvancedPropertyTree("Node Graph Items");
	m_pPropertyTree->setExpandLevels(4);
	m_pPropertyTree->setValueColumnWidth(0.6f);
	m_pPropertyTree->setAutoRevert(false);
	m_pPropertyTree->setAggregateMouseEvents(false);
	m_pPropertyTree->setFullRowContainers(true);
	m_pPropertyTree->setSizeToContent(true);

	m_pPropertyTree->attach(m_structs);

	addWidget(m_pPropertyTree);
}

}

