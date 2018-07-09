// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "NodeGraphViewGraphicsWidget.h"
#include "NodeGraphView.h"

#include "AbstractNodeGraphViewModelItem.h"

namespace CryGraphEditor {

CNodeGraphViewGraphicsWidget::CNodeGraphViewGraphicsWidget(CNodeGraphView& view)
	: m_view(view)
	, m_isSelected(false)
	, m_isHighlighted(false)
	, m_isDeactivated(false)
{

}

CNodeGraphViewGraphicsWidget::~CNodeGraphViewGraphicsWidget()
{

}

void CNodeGraphViewGraphicsWidget::DeleteLater()
{
	GetView().scene()->removeItem(this);
	deleteLater();
}

void CNodeGraphViewGraphicsWidget::OnItemInvalidated()
{
	if (CAbstractNodeGraphViewModelItem* pAbstractItem = GetAbstractItem())
	{
		if (pAbstractItem->GetAcceptsMoves())
		{
			const QPointF position = pAbstractItem->GetPosition();
			if (position != scenePos())
				setPos(position);
		}

		if (pAbstractItem->GetAcceptsDeactivation())
		{
			const bool isDeactivated = pAbstractItem->IsDeactivated();
			SetDeactivated(isDeactivated);
		}
	}
}

bool CNodeGraphViewGraphicsWidget::IsSameView(CNodeGraphViewGraphicsWidget* pGrahicsItem) const
{
	return &m_view == &pGrahicsItem->GetView();
}

bool CNodeGraphViewGraphicsWidget::IsView(QObject* pObject) const
{
	if (CNodeGraphView* pView = qobject_cast<CNodeGraphView*>(pObject))
	{
		return &m_view == pView;
	}

	return false;
}

void CNodeGraphViewGraphicsWidget::SetSelected(bool isSelected)
{
	CAbstractNodeGraphViewModelItem* pAbstractItem = GetAbstractItem();
	if (pAbstractItem && pAbstractItem->GetAcceptsSelection() && m_isSelected != isSelected)
	{
		m_isSelected = isSelected;
		SignalSelectionChanged(isSelected);

		update();
	}
}

void CNodeGraphViewGraphicsWidget::SetHighlighted(bool isHighlighted)
{
	CAbstractNodeGraphViewModelItem* pAbstractItem = GetAbstractItem();
	if (pAbstractItem && pAbstractItem->GetAcceptsHighlightning() && m_isHighlighted != isHighlighted)
	{
		m_isHighlighted = isHighlighted;
		SignalHighlightedChanged(isHighlighted);

		update();
	}
}

void CNodeGraphViewGraphicsWidget::SetDeactivated(bool isDeactivated)
{
	CAbstractNodeGraphViewModelItem* pAbstractItem = GetAbstractItem();
	if (pAbstractItem && pAbstractItem->GetAcceptsDeactivation() && m_isDeactivated != isDeactivated)
	{
		m_isDeactivated = isDeactivated;
		SignalDeactivatedChanged(isDeactivated);

		update();
	}
}

void CNodeGraphViewGraphicsWidget::OnItemPositionChanged()
{
	if (CAbstractNodeGraphViewModelItem* pAbstractItem = GetAbstractItem())
	{
		CRY_ASSERT(pAbstractItem->GetAcceptsMoves());
		const QPointF position = pAbstractItem->GetPosition();
		setPos(position);
	}
}

void CNodeGraphViewGraphicsWidget::OnItemDeactivatedChanged()
{
	if (CAbstractNodeGraphViewModelItem* pAbstractItem = GetAbstractItem())
	{
		SetDeactivated(pAbstractItem->IsDeactivated());
	}
}

}

