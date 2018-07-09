// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AbstractCommentItem.h"

namespace CryGraphEditor {

CAbstractCommentItem::CAbstractCommentItem(CNodeGraphViewModel& viewModel)
	: CAbstractNodeGraphViewModelItem(viewModel)
{
}

CAbstractCommentItem::~CAbstractCommentItem()
{
	SetAccpetsMoves(true);
	SetAcceptsSelection(true);
	SetAcceptsHighlightning(true);
}

void CAbstractCommentItem::SetPosition(QPointF position)
{
	if (GetAcceptsMoves() && m_position != position)
	{
		m_position = position;
		SignalPositionChanged();
	}
}

}

