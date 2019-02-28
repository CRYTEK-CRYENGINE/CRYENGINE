// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <NodeGraph/NodeGraphView.h>
#include "NodeGraph/NodeWidget.h"

class CAssetNodeBase;
class QPopupWidget;
class CDictionaryWidget;

namespace CryGraphEditor
{

class CNodeGraphViewStyle;
class CNodeGraphViewBackground;
class CNodeGraphView;

}

class CGraphView : public CryGraphEditor::CNodeGraphView
{
public:
	CGraphView(CryGraphEditor::CNodeGraphViewModel* pViewModel);
protected:
	virtual bool PopulateNodeContextMenu(CryGraphEditor::CAbstractNodeItem& node, QMenu& menu) override;
	virtual void ShowGraphContextMenu(QPointF screenPos) override;
	void         OnContextMenuEntryClicked(CAbstractDictionaryEntry& entry);
private:
	std::unique_ptr<QPopupWidget> m_pSearchPopup;
	CDictionaryWidget*            m_pSearchPopupContent;
};

class CAssetWidget : public CryGraphEditor::CNodeWidget
{
public:
	CAssetWidget(CAssetNodeBase& item, CryGraphEditor::CNodeGraphView& view);
protected:
	virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* pEvent) override;
};

