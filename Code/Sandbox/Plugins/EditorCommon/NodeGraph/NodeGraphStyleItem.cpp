// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "NodeGraphStyleItem.h"

#include "NodeGraphViewStyle.h"

namespace CryGraphEditor {

CNodeGraphViewStyleItem::CNodeGraphViewStyleItem(const char* szStyleId)
	: m_styleId(szStyleId)
	, m_styleIdHash(CCrc32::Compute(szStyleId))
	, m_pViewStyle(nullptr)
{
	CRY_ASSERT(szStyleId);
	setObjectName(szStyleId);
}

void CNodeGraphViewStyleItem::SetParent(CNodeGraphViewStyle* pViewStyle)
{
	m_pViewStyle = pViewStyle;
	setParent(pViewStyle);
}

}

