// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <NodeGraph/NodeGraphView.h>

namespace CrySchematycEditor
{
class CMainWindow;

class CGraphViewWidget : public CryGraphEditor::CNodeGraphView
{
public:
	CGraphViewWidget(CMainWindow& editor);
	~CGraphViewWidget();

	// CryGraphEditor::CNodeGraphView
	virtual QWidget* CreatePropertiesWidget(CryGraphEditor::GraphItemSet& selectedItems) override;
	// ~CryGraphEditor::CNodeGraphView

private:
	CMainWindow& m_editor;

};

}

