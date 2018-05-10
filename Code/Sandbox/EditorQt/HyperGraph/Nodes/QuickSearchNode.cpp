// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QuickSearchNode.h"

#include "HyperGraph/NodePainter/HyperNodePainter_QuickSearch.h"


static CHyperNodePainter_QuickSearch painter;

CQuickSearchNode::CQuickSearchNode()
	: m_iSearchResultCount(1)
	, m_iIndex(1)
{
	SetClass(GetClassType());
	m_pPainter = &painter;
	m_name = "Game:Start";
}

CHyperNode* CQuickSearchNode::Clone()
{
	CQuickSearchNode* pNode = new CQuickSearchNode();
	pNode->CopyFrom(*this);
	return pNode;
}

