// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "HyperGraph/HyperGraphNode.h"
class CFlowNode;


class CBlackBoxNode : public CHyperNode
{
public:
	static const char* GetClassType()
	{
		return "_blackbox";
	}
	CBlackBoxNode();

	// CHyperNode
	void                      Init() override {}
	void                      Done() override;
	CHyperNode*               Clone() override;
	virtual void              Serialize(XmlNodeRef& node, bool bLoading, CObjectArchive* ar) override;

	virtual bool              IsEditorSpecialNode() const override { return true; }
	virtual bool              IsFlowNode() const      override    { return false; }
	virtual const char*       GetDescription() const   override   { return "Encapsulates other nodes for organization purposes."; }
	virtual const char*       GetInfoAsString() const   override  { return "Class: BlackBox"; }
	// ~CHyperNode

	virtual void              SetPos(Gdiplus::PointF pos);

	virtual int               GetObjectAt(Gdiplus::Graphics* pGr, Gdiplus::PointF point);

	void                      AddNode(CHyperNode* pNode);
	void                      RemoveNode(CHyperNode* pNode);
	bool                      IncludesNode(CHyperNode* pNode);
	bool                      IncludesNode(HyperNodeID nodeId);
	void                      Clear()    { m_nodes.clear(); }
	std::vector<CHyperNode*>* GetNodes() { return &m_nodes; }
	std::vector<CHyperNode*>* GetNodesSafe();
	Gdiplus::PointF           GetPointForPort(CHyperNodePort* port);
	CHyperNodePort*           GetPortAtPoint(Gdiplus::PointF);
	CHyperNode*               GetNode(CHyperNodePort* port);
	ILINE void                SetPort(CHyperNodePort* port, Gdiplus::PointF pos) { m_ports[port] = pos; }

	void SetNormalBounds(Gdiplus::RectF rect) { m_normalBounds = rect; }

	void Minimize()
	{
		m_bCollapsed = !m_bCollapsed;
		for (int i = 0; i < m_nodes.size(); ++i)
			m_nodes[i]->Invalidate(true, false);
		if (m_bCollapsed)
			m_iBrackets = -1;
		Invalidate(true, true);
	}
	bool                    IsMinimized() { return m_bCollapsed; }

	bool                    PortActive(const CHyperNodePort* port);

	virtual CHyperNodePort* GetPortAtPoint(Gdiplus::Graphics* pGr, Gdiplus::PointF point)
	{
		//go through sub-nodes to retrieve port .. ?
		return NULL;
	}

private:

	std::vector<CHyperNode*>                   m_nodes;
	std::vector<HyperNodeID>                   m_nodesIDs;
	std::map<CHyperNodePort*, Gdiplus::PointF> m_ports;
	bool           m_bCollapsed;
	int            m_iBrackets;
	Gdiplus::RectF m_normalBounds;
};

