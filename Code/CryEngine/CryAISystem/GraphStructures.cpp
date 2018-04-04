// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "GraphStructures.h"

std::vector<unsigned> GraphNode::freeIDs;
#ifdef DEBUG_GRAPHNODE_IDS
std::vector<unsigned> GraphNode::usedIds;
#endif
unsigned GraphNode::maxID = 0;

GraphNode::GraphNode(IAISystem::ENavigationType type, const Vec3& inpos, unsigned int _ID)
	: navType(type), pos(inpos)
{
	firstLinkIndex = 0;
	mark = 0;
	nRefCount = 0;
	if (_ID == 0)
	{
		if (freeIDs.empty())
		{
			ID = maxID + 1;
		}
		else
		{
			ID = freeIDs.back();
			freeIDs.pop_back();
		}
	}
	else
	{
		ID = _ID;
		stl::find_and_erase(freeIDs, ID);
	}
	if (ID > maxID)
		maxID = ID;

#ifdef DEBUG_GRAPHNODE_IDS
	assert(!stl::find(usedIds, ID));
	usedIds.push_back(ID);
#endif
}

GraphNode::~GraphNode()
{
	freeIDs.push_back(ID);

#ifdef DEBUG_GRAPHNODE_IDS
	const bool bUsedIdRemoved = stl::find_and_erase(usedIds, ID);
	assert(bUsedIdRemoved);
#endif
}

//====================================================================
// Serialize
//====================================================================
void ObstacleData::Serialize(TSerialize ser)
{
	ser.Value("vPos", vPos);
	ser.Value("vDir", vDir);
	ser.Value("fApproxRadius", fApproxRadius);
	ser.Value("flags", flags);
	if (ser.IsReading())
	{
		navNodes.clear();
	}
}

//===================================================================
// SetNavNodes
//===================================================================
void ObstacleData::SetNavNodes(const std::vector<const GraphNode*>& nodes)
{
	navNodes = nodes;
}

//===================================================================
// AddNavNode
//===================================================================
void ObstacleData::AddNavNode(const GraphNode* pNode)
{
	navNodes.push_back(pNode);
}

//===================================================================
// GetNavNodes
//===================================================================
const std::vector<const GraphNode*>& ObstacleData::GetNavNodes() const
{
	return navNodes;
}
