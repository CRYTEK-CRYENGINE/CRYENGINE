// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"

#include "AllNodesContainer.h"
#include "Graph.h"

bool CAllNodesContainer::SOneNodeCollector::operator()(const SGraphNodeRecord& record, float distSq)
{
	bool ret = false;
	GraphNode* pNode = nodeManager.GetNode(record.nodeIndex);
	if (pNode->navType & navTypeMask && PointInTriangle(m_Pos, pNode))
	{
		node.first = distSq;
		node.second = record.nodeIndex;
		ret = true;
	}

	return ret;
}

void CAllNodesContainer::Compact()
{
	m_hashSpace.Compact();
}
