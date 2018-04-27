// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AbstractNodeItem.h"
#include "AbstractPinItem.h"

#include "NodeGraphViewStyle.h"

namespace CryGraphEditor {

CAbstractNodeItem::CAbstractNodeItem(CNodeGraphViewModel& viewModel)
	: CAbstractNodeGraphViewModelItem(viewModel)
	, m_isDeactivated(false)
	, m_acceptsRenaming(false)
	, m_hasErrors(false)
	, m_hasWarnings(false)
{
	SetAccpetsMoves(true);
	SetAcceptsSelection(true);
	SetAcceptsHighlightning(true);
	SetAcceptsDeactivation(true);
}

CAbstractNodeItem::~CAbstractNodeItem()
{

}

void CAbstractNodeItem::SetPosition(QPointF position)
{
	if (GetAcceptsMoves() && m_position != position)
	{
		m_position = position;
		SignalPositionChanged();
	}
}

void CAbstractNodeItem::SetDeactivated(bool isDeactivated)
{
	if (GetAcceptsDeactivation() && m_isDeactivated != isDeactivated)
	{
		m_isDeactivated = isDeactivated;
		SignalDeactivatedChanged(isDeactivated);
	}
}

void CAbstractNodeItem::SetWarnings(bool hasWarnings)
{
	m_hasWarnings = hasWarnings;
}

void CAbstractNodeItem::SetErrors(bool hasErrors)
{
	m_hasErrors = hasErrors;
}

void CAbstractNodeItem::SetName(const QString& name)
{
	if (m_acceptsRenaming && m_name != name)
	{
		m_name = name;
		SignalNameChanged();
	}
}

CAbstractPinItem* CAbstractNodeItem::GetPinItemById(QVariant id) const
{
	for (CAbstractPinItem* pPinItem : GetPinItems())
	{
		if (pPinItem->HasId(id))
			return pPinItem;
	}

	return nullptr;
}

CAbstractPinItem* CAbstractNodeItem::GetPinItemByIndex(uint32 index) const
{
	const PinItemArray& pins = GetPinItems();
	if (index < pins.size())
	{
		return pins[index];
	}

	return nullptr;
}

uint32 CAbstractNodeItem::GetPinItemIndex(const CAbstractPinItem& pin) const
{
	uint32 index = 0;
	for (CAbstractPinItem* pPinItem : GetPinItems())
	{
		if (pPinItem == &pin)
			return index;
		++index;
	}

	return 0xffffffff;
}

}

