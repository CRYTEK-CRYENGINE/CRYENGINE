// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GraphViewWidget.h"

#include "PropertiesWidget.h"
#include "GraphViewModel.h"

#include <QtUtil.h>

namespace CrySchematycEditor {

CGraphViewWidget::CGraphViewWidget(CMainWindow& editor)
	: CryGraphEditor::CNodeGraphView()
	, m_editor(editor)
{

}

CGraphViewWidget::~CGraphViewWidget()
{

}

QWidget* CGraphViewWidget::CreatePropertiesWidget(CryGraphEditor::GraphItemSet& selectedItems)
{
	CPropertiesWidget* pPropertiesWidget = new CPropertiesWidget(selectedItems, &m_editor);

	if (CNodeGraphViewModel* pModel = static_cast<CNodeGraphViewModel*>(GetModel()))
	{
		QObject::connect(pPropertiesWidget, &CPropertiesWidget::SignalPropertyChanged, pModel, &CNodeGraphViewModel::Refresh);
	}
	return pPropertiesWidget;
}

}

