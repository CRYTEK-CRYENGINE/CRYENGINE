// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "CommentBoxNode.h"

#include "HyperGraph/NodePainter/HyperNodePainter_CommentBox.h"
#include "HyperGraph/FlowGraphVariables.h"

#define DEFAULT_WIDTH         300
#define DEFAULT_HEIGHT        100
#define DEFAULT_COLOR         (Vec3(255, 255, 255))
#define DEFAULT_DRAW_PRIORITY 16

CCommentBoxNode::CCommentBoxNode()
{
	SetClass(GetClassType());
	m_pPainter = new CHyperNodePainter_CommentBox();
	m_name = "This is a comment box";
	m_resizeBorderRect.Width = DEFAULT_WIDTH * CHyperNodePainter_CommentBox::AccessStaticVar_ZoomFactor();
	m_resizeBorderRect.Height = DEFAULT_HEIGHT * CHyperNodePainter_CommentBox::AccessStaticVar_ZoomFactor();

	{
		CVariableFlowNodeEnum<int>* pEnumVar = new CVariableFlowNodeEnum<int>;
		pEnumVar->AddEnumItem("Small", 1);
		pEnumVar->AddEnumItem("Medium", 2);
		pEnumVar->AddEnumItem("Big", 3);

		CHyperNodePort port;
		port.bInput = true;
		port.pVar = pEnumVar;
		port.pVar->SetDescription("Title text size");
		port.pVar->SetName("TextSize");
		port.pVar->Set(1);    // default text size
		AddPort(port);
	}

	{
		CHyperNodePort port;
		port.bInput = true;
		port.pVar = new CVariableFlowNode<Vec3>;
		port.pVar->SetName("Color");
		port.pVar->SetDataType(IVariable::DT_COLOR);

		ColorF colorF(DEFAULT_COLOR / 255.f);
		Vec3 vec = colorF.toVec3();
		port.pVar->Set(vec);
		port.pVar->SetDescription("Node color");
		AddPort(port);
	}

	{
		CHyperNodePort port;
		port.bInput = true;
		port.pVar = new CVariableFlowNode<bool>;
		port.pVar->SetName("DisplayFilled");
		port.pVar->Set(true);
		port.pVar->SetDescription("Draw the body of the box or only the borders");
		AddPort(port);
	}

	{
		CHyperNodePort port;
		port.bInput = true;
		port.pVar = new CVariableFlowNode<bool>;
		port.pVar->SetName("DisplayBox");
		port.pVar->Set(true);
		port.pVar->SetDescription("Draw the box");
		AddPort(port);
	}

	m_drawPriority = DEFAULT_DRAW_PRIORITY;

	{
		CHyperNodePort port;
		port.bInput = true;
		port.pVar = new CVariableFlowNode<int>;
		port.pVar->SetName("SortPriority");
		port.pVar->Set(int(m_drawPriority));
		port.pVar->SetDescription("Drawing priority");
		port.pVar->SetLimits(0, MAX_DRAW_PRIORITY, 1);
		AddPort(port);
	}
}

CCommentBoxNode::~CCommentBoxNode()
{
	SAFE_DELETE(m_pPainter);
}

CHyperNode* CCommentBoxNode::Clone()
{
	CCommentBoxNode* pNode = new CCommentBoxNode();
	pNode->CopyFrom(*this);
	pNode->m_resizeBorderRect = m_resizeBorderRect;
	return pNode;
}

// parameter is relative to the node pos
void CCommentBoxNode::SetResizeBorderRect(const Gdiplus::RectF& newRelBordersRect)
{
	Gdiplus::RectF newRect = newRelBordersRect;

	newRect.Width = floor(((float)newRect.Width / GRID_SIZE) + 0.5f) * GRID_SIZE;
	newRect.Height = floor(((float)newRect.Height / GRID_SIZE) + 0.5f) * GRID_SIZE;

	float incYSize = newRect.Height - m_resizeBorderRect.Height;
	if ((incYSize + m_rect.Height) <= 0)
		return;
	m_rect.Height += incYSize;

	m_resizeBorderRect = newRect;
}

// parameter is absolute coordinates
void CCommentBoxNode::SetBorderRect(const Gdiplus::RectF& newAbsBordersRect)
{
	Gdiplus::PointF pos;
	newAbsBordersRect.GetLocation(&pos);
	SetPos(pos);
	m_rect.Height = m_resizeBorderRect.Height = newAbsBordersRect.Height;
	m_rect.Width = m_resizeBorderRect.Width = newAbsBordersRect.Width;
	OnPossibleSizeChange();
}

void CCommentBoxNode::Serialize(XmlNodeRef& node, bool bLoading, CObjectArchive* ar)
{
	__super::Serialize(node, bLoading, ar);

	if (bLoading)
	{
		XmlNodeRef resizeBorderNode = node->findChild("ResizeBorder");
		resizeBorderNode->getAttr("X", m_resizeBorderRect.X);
		resizeBorderNode->getAttr("Y", m_resizeBorderRect.Y);
		resizeBorderNode->getAttr("Width", m_resizeBorderRect.Width);
		resizeBorderNode->getAttr("Height", m_resizeBorderRect.Height);
		XmlNodeRef nodeSize = node->findChild("NodeSize");
		if (nodeSize)
		{
			nodeSize->getAttr("Width", m_rect.Width);
			nodeSize->getAttr("Height", m_rect.Height);
		}
		CHyperNodePort* pPort = FindPort("SortPriority", true);
		if (pPort)
			pPort->pVar->Get(m_drawPriority);

		OnPossibleSizeChange();
	}
	else
	{
		XmlNodeRef resizeBorderNode = node->newChild("ResizeBorder");
		resizeBorderNode->setAttr("X", m_resizeBorderRect.X);
		resizeBorderNode->setAttr("Y", m_resizeBorderRect.Y);
		resizeBorderNode->setAttr("Width", m_resizeBorderRect.Width);
		resizeBorderNode->setAttr("Height", m_resizeBorderRect.Height);
		XmlNodeRef nodeSize = node->newChild("NodeSize");
		nodeSize->setAttr("Width", m_rect.Width);
		nodeSize->setAttr("Height", m_rect.Height);
	}
}

void CCommentBoxNode::OnZoomChange(float zoom)
{
	const float MIN_ZOOM_VALUE = 0.00001f;
	zoom = max(zoom, MIN_ZOOM_VALUE);
	float zoomFactor = max(1.f, 1.f / zoom);
	CHyperNodePainter_CommentBox::AccessStaticVar_ZoomFactor() = zoomFactor;
	OnPossibleSizeChange(); // the size and position of the node can be changed in the first draw after a zoom change (because the title could have been resized), so we need another redraw.
}

void CCommentBoxNode::OnInputsChanged()
{
	CHyperNodePort* pPort = FindPort("SortPriority", true);
	if (pPort)
		pPort->pVar->Get(m_drawPriority);
	OnPossibleSizeChange();  // the title font size could be modified
}

void CCommentBoxNode::OnPossibleSizeChange()
{
	Invalidate(true, false);
	if (!m_dispList.Empty())
	{
		Gdiplus::RectF bounds = m_dispList.GetBounds();
		m_rect.X += bounds.X;
		m_rect.Y += bounds.Y;
		m_rect.Width = bounds.Width;
		m_rect.Height = bounds.Height;
		Invalidate(true, false);   // need a double call because the size and/or position could change after the first one.
	}
}

void CCommentBoxNode::SetPos(Gdiplus::PointF pos)
{
	pos.X = floor(((float)pos.X / GRID_SIZE) + 0.5f) * GRID_SIZE;
	pos.Y = floor(((float)pos.Y / GRID_SIZE) + 0.5f) * GRID_SIZE;

	m_rect = Gdiplus::RectF(pos.X, pos.Y, m_rect.Width, m_rect.Height);
	Invalidate(false, true);
}

