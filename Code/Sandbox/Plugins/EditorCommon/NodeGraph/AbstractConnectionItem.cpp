// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AbstractConnectionItem.h"

#include "NodeGraphViewStyle.h"

#include <QColor>

namespace CryGraphEditor {

CAbstractConnectionItem::CAbstractConnectionItem(CNodeGraphViewModel& viewModel)
	: CAbstractNodeGraphViewModelItem(viewModel)
	, m_isDeactivated(false)
{
	SetAcceptsSelection(true);
	SetAcceptsHighlightning(true);
	SetAcceptsDeactivation(true);
}

CAbstractConnectionItem::~CAbstractConnectionItem()
{

}

void CAbstractConnectionItem::SetDeactivated(bool isDeactivated)
{
	if (GetAcceptsDeactivation() && m_isDeactivated != isDeactivated)
	{
		m_isDeactivated = isDeactivated;
		SignalDeactivatedChanged();
	}
}

}

