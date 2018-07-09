// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BlackBoxNode.h"

#include "HyperGraph/NodePainter/HyperNodePainter_BlackBox.h"
#include "HyperGraph/FlowGraphNode.h"
#include "HyperGraph/FlowGraph.h"
#include "HyperGraph/FlowGraphManager.h"

static CHyperNodePainter_BlackBox bbPainter;

CBlackBoxNode::CBlackBoxNode()
{
	SetClass(GetClassType());
	m_pPainter = &bbPainter;
	m_name = "This is a black box";
	m_bCollapsed = false;
}

void CBlackBoxNode::Done()
{
	GetNodesSafe();
	for (int i = 0; i < m_nodes.size(); ++i)
		m_nodes[i]->SetBlackBox(NULL);
}

CHyperNode* CBlackBoxNode::Clone()
{
	CBlackBoxNode* pNode = new CBlackBoxNode();
	pNode->CopyFrom(*this);
	return pNode;
}

void CBlackBoxNode::Serialize(XmlNodeRef& node, bool bLoading, CObjectArchive* ar)
{
	__super::Serialize(node, bLoading, ar);

	CHyperGraph* pMyEditorFG = (CHyperGraph*)GetGraph();

	if (bLoading)
	{
		m_nodes.clear();
		m_nodesIDs.clear();
		m_ports.clear();

		if (pMyEditorFG)
		{
			if (XmlNodeRef children = node->findChild("BlackBoxChildren"))
			{
				for (int i = 0; i < children->getNumAttributes(); ++i)
				{
					const char* key = NULL;
					const char* value = NULL;
					children->getAttributeByIndex(i, &key, &value);
					HyperNodeID nodeId = (HyperNodeID)(atoi(value));

					if (IHyperNode* pNode = pMyEditorFG->FindNode(nodeId))
						AddNode((CHyperNode*)pNode);
				}
			}
		}

		Invalidate(true, true);
	}
	else
	{
		XmlNodeRef children = XmlHelpers::CreateXmlNode("BlackBoxChildren");
		for (int i = 0; i < m_nodesIDs.size(); ++i)
		{
			children->setAttr("NodeId", m_nodesIDs[i]);
		}
		node->addChild(children);
	}
}

void CBlackBoxNode::SetPos(Gdiplus::PointF pos)
{
	float dX, dY;
	Gdiplus::PointF p = GetPos();
	dX = pos.X - p.X;
	dY = pos.Y - p.Y;
	for (int i = 0; i < m_nodes.size(); ++i)
	{
		p = m_nodes[i]->GetPos();
		p.X += dX;
		p.Y += dY;
		m_nodes[i]->SetPos(p);
	}
	m_rect = Gdiplus::RectF(pos.X, pos.Y, m_rect.Width, m_rect.Height);
	Invalidate(false, true);
}

int CBlackBoxNode::GetObjectAt(Gdiplus::Graphics* pGr, Gdiplus::PointF point)
{
	int obj = __super::GetObjectAt(pGr, point);

	if (obj > 0)
	{
		Gdiplus::PointF pos = GetPos();
		Gdiplus::RectF rect = m_normalBounds;
		rect.X = pos.X;
		rect.Y = pos.Y;
		if (!rect.Contains(point))
			return -1;
	}

	return obj;
}

void CBlackBoxNode::AddNode(CHyperNode* pNode)
{
	if (pNode && !stl::find(m_nodes, pNode))
	{
		if (pNode->GetGraph() == GetGraph())
		{
			m_nodes.push_back(pNode);
			pNode->SetBlackBox(this);
			m_nodesIDs.push_back(pNode->GetId());
			Invalidate(true, true);
		}
	}
}

void CBlackBoxNode::RemoveNode(CHyperNode* pNode)
{
	if (pNode)
	{
		std::vector<CHyperNode*>::iterator it = m_nodes.begin();
		std::vector<CHyperNode*>::iterator end = m_nodes.end();
		for (; it != end; ++it)
		{
			if (*it == pNode)
			{
				m_nodes.erase(it);
				break;
			}
		}

		std::vector<HyperNodeID>::iterator itId = m_nodesIDs.begin();
		std::vector<HyperNodeID>::iterator endId = m_nodesIDs.end();
		for (; itId != endId; ++itId)
		{
			if (*itId == pNode->GetId())
			{
				m_nodesIDs.erase(itId);
				break;
			}
		}
	}
}

bool CBlackBoxNode::IncludesNode(CHyperNode* pNode)
{
	if (stl::find(m_nodes, pNode))
		return true;
	return false;
}

bool CBlackBoxNode::IncludesNode(HyperNodeID nodeId)
{
	for (int i = 0; i < m_nodes.size(); ++i)
	{
		if (m_nodes[i]->GetId() == nodeId)
			return true;
	}
	return false;
}

Gdiplus::PointF CBlackBoxNode::GetPointForPort(CHyperNodePort* port)
{
	std::map<CHyperNodePort*, Gdiplus::PointF>::iterator it = m_ports.begin();
	std::map<CHyperNodePort*, Gdiplus::PointF>::iterator end = m_ports.end();
	for (; it != end; ++it)
	{
		if (it->first == port)
		{
			return GetPos() + it->second;
		}
	}
	return GetPos();
}

CHyperNodePort* CBlackBoxNode::GetPortAtPoint(Gdiplus::PointF point)
{
	std::map<CHyperNodePort*, Gdiplus::PointF>::iterator it = m_ports.begin();
	std::map<CHyperNodePort*, Gdiplus::PointF>::iterator end = m_ports.end();
	Gdiplus::PointF pos = GetPos();
	for (; it != end; ++it)
	{
		int dX = (pos.X + it->second.X) - point.X;
		int dY = (pos.Y + it->second.Y) - point.Y;
		if (abs(dX) < 10.0f && abs(dY) < 10.0f)
			return it->first;
	}
	return NULL;
}

CHyperNode* CBlackBoxNode::GetNode(CHyperNodePort* port)
{
	std::vector<CHyperNode*>::iterator it = m_nodes.begin();
	std::vector<CHyperNode*>::iterator end = m_nodes.end();
	for (; it != end; ++it)
	{
		CHyperNode::Ports* ports = NULL;
		if (port->bInput)
			ports = (*it)->GetInputs();
		else
			ports = (*it)->GetOutputs();
		for (int i = 0; i < ports->size(); ++i)
		{
			CHyperNodePort* pP = &((*ports)[i]);
			if (pP == port)
				return (*it);
		}
	}
	return NULL;
}

bool CBlackBoxNode::PortActive(const CHyperNodePort* port)
{
	std::map<CHyperNodePort*, Gdiplus::PointF>::iterator it = m_ports.begin();
	std::map<CHyperNodePort*, Gdiplus::PointF>::iterator end = m_ports.end();
	for (; it != end; ++it)
	{
		if (it->first == port)
			return true;
	}
	return false;
}

std::vector<CHyperNode*>* CBlackBoxNode::GetNodesSafe()
{
	CHyperGraph* pMyEditorFG = (CHyperGraph*)GetGraph();
	m_nodes.clear();

	if (!pMyEditorFG)
		return &m_nodes;

	for (int i = 0; i < m_nodesIDs.size(); ++i)
	{
		if (IHyperNode* pNode = pMyEditorFG->FindNode(m_nodesIDs[i]))
			m_nodes.push_back((CHyperNode*)pNode);
	}

	return &m_nodes;
}

