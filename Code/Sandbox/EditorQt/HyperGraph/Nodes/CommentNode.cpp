// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "CommentNode.h"

#include "HyperGraph/NodePainter/HyperNodePainter_Comment.h"

static CHyperNodePainter_Comment comment_painter;

CCommentNode::CCommentNode()
{
	SetClass(GetClassType());
	m_pPainter = &comment_painter;
	m_name = "This is a comment";
}

CHyperNode* CCommentNode::Clone()
{
	CCommentNode* pNode = new CCommentNode();
	pNode->CopyFrom(*this);
	return pNode;
}

