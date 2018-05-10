// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AbstractGroupItem.h"

namespace CryGraphEditor {

CAbstractGroupItem::CAbstractGroupItem(CNodeGraphViewModel& viewModel)
	: CAbstractNodeGraphViewModelItem(viewModel)
{
	SetAcceptsSelection(true);
	SetAcceptsHighlightning(true);
}

CAbstractGroupItem::~CAbstractGroupItem()
{

}

void CAbstractGroupItem::SetPosition(QPointF position)
{
	if (GetAcceptsMoves() && m_position != position)
	{
		m_position = position;
		SignalPositionChanged();
	}
}

}

