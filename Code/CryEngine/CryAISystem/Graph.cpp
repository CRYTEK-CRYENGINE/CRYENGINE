// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/********************************************************************
   CryGame Source File.
   -------------------------------------------------------------------------
   File name:   Graph.cpp
   Version:     v1.00
   Description: Implementation of the CGraph class.

   -------------------------------------------------------------------------
   History:
   - ?
   - 4 May 2009  : Evgeny Adamenkov: Removed IRenderer

 *********************************************************************/

#include "StdAfx.h"

#include <limits>
#include <algorithm>

#include <CrySystem/ISystem.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySystem/ILog.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/IConsole.h>
#include <CryPhysics/IPhysics.h>
#include <CrySystem/File/CryFile.h>

#include "Graph.h"
#include "CAISystem.h"
#include "AILog.h"
#include <CryMath/Cry_Math.h>
#include "AIObject.h"
#include "VertexList.h"
#include "NavRegion.h"

#include "GraphLinkManager.h"
#include "GraphNodeManager.h"

#define BAI_TRI_FILE_VERSION 54

// identifier so links can be marked as impassable, then restored
//static const float RADIUS_FOR_BROKEN_LINKS = -121314.0f;

const float AStarSearchNode::fInvalidCost = 999999.0f;

Vec3 CObstacleRef::GetPos() const
{
	CCCPOINT(CObstacleRef_GetPos);

	if (m_refAnchor.IsValid())
	{
		return m_refAnchor.GetAIObject()->GetPos();
	}
	else if (m_pNode)
	{
		return m_pNode->GetPos();
	}
	else if (m_pSmartObject && m_pRule)
	{
		return m_pRule->pObjectHelper ? m_pSmartObject->GetHelperPos(m_pRule->pObjectHelper) : m_pSmartObject->GetPos();
	}
	else
	{
		assert(false);
		return Vec3(ZERO);
	}
}

float CObstacleRef::GetApproxRadius() const
{
	assert(false);
	return 0.0f;
}

//====================================================================
// ValidateNode
//====================================================================
inline bool CGraph::ValidateNode(unsigned nodeIndex, bool fullCheck) const
{
	const GraphNode* pNode = GetNodeManager().GetNode(nodeIndex);

	if (!nodeIndex)
		return false;
	if (!m_allNodes.DoesNodeExist(nodeIndex))
		return false;
	if (!fullCheck)
		return true;
	else
		return ValidateNodeFullCheck(pNode);
}

//====================================================================
// ValidateNodeFullCheck
//====================================================================
bool CGraph::ValidateNodeFullCheck(const GraphNode* pNode) const
{
	bool result = true;
	AIAssert(pNode);
	int nNonRoadLinks = 0;
	unsigned nTriLinks = 0;
	for (unsigned linkId = pNode->firstLinkIndex; linkId; linkId = GetLinkManager().GetNextLink(linkId))
	{
		unsigned nextNodeIndex = GetLinkManager().GetNextNode(linkId);
		const GraphNode* next = GetNodeManager().GetNode(nextNodeIndex);
		if (!CGraph::ValidateNode(nextNodeIndex, false))
			result = false;
		if (next->navType != IAISystem::NAV_ROAD)
			++nNonRoadLinks;
		if (next->navType == IAISystem::NAV_TRIANGULAR)
			++nTriLinks;
	}
	if (nNonRoadLinks > 50)
	{
		AIWarning("Too many non-road links (%d) from node %p type %d at (%5.2f, %5.2f, %5.2f)", nNonRoadLinks, pNode, pNode->navType,
		          pNode->GetPos().x, pNode->GetPos().y, pNode->GetPos().z);
	}
	if (pNode->navType == IAISystem::NAV_TRIANGULAR && nTriLinks != 3)
	{
		// NAV_TRIANGULAR is replaced by MNM
		assert(false);
		result = false;
	}
	return result;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//====================================================================
// CGraph
//====================================================================
CGraph::CGraph()
	: m_pGraphLinkManager(new CGraphLinkManager),
	m_pGraphNodeManager(new CGraphNodeManager),
	m_allNodes(*m_pGraphNodeManager),
	m_triangularBBox(AABB::RESET)
{

	m_safeFirstIndex = CreateNewNode(IAISystem::NAV_UNSET, Vec3(ZERO), 0);
	m_firstIndex = m_safeFirstIndex;
	m_pFirst = GetNodeManager().GetNode(m_safeFirstIndex);
	m_pSafeFirst = m_pFirst;
	m_pCurrent = m_pFirst;
	m_currentIndex = m_firstIndex;
	m_pCurrent->firstLinkIndex = 0;
}

//====================================================================
// ~CGraph
//====================================================================
CGraph::~CGraph()
{
	//Takes a long time and shouldn't be needed
	//Clear(IAISystem::NAVMASK_ALL);

	delete m_pGraphNodeManager;
	delete m_pGraphLinkManager;

	GraphNode::ClearFreeIDs();
}

//====================================================================
// Clear
//====================================================================
void CGraph::Clear(IAISystem::tNavCapMask navTypeMask)
{
	ClearMarks();
	DeleteGraph(navTypeMask);
	if (navTypeMask & (IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE))
	{
		EntranceMap::iterator next;
		for (EntranceMap::iterator it = m_mapEntrances.begin(); it != m_mapEntrances.end(); it = next)
		{
			next = it;
			++next;
			GraphNode* pNode = GetNodeManager().GetNode(it->second);
			if (pNode->navType & navTypeMask)
				m_mapEntrances.erase(it);
		}
		for (EntranceMap::iterator it = m_mapExits.begin(); it != m_mapExits.end(); it = next)
		{
			next = it;
			++next;
			GraphNode* pNode = GetNodeManager().GetNode(it->second);
			if (pNode->navType & navTypeMask)
				m_mapExits.erase(it);
		}
	}

	GetNodeManager().Clear(navTypeMask);

	if (m_pSafeFirst)
	{
		Disconnect(m_safeFirstIndex, false);
	}
	else
	{
		m_safeFirstIndex = CreateNewNode(IAISystem::NAV_UNSET, Vec3(ZERO), 0);
		m_pSafeFirst = GetNodeManager().GetNode(m_safeFirstIndex);
	}
	m_pFirst = m_pSafeFirst;
	m_firstIndex = m_safeFirstIndex;
	m_pCurrent = m_pFirst;
	m_currentIndex = m_firstIndex;
}

//====================================================================
// ConnectInCm
//====================================================================
void CGraph::ConnectInCm(unsigned oneNodeIndex, unsigned twoNodeIndex,
                         int16 radiusOneToTwoCm, int16 radiusTwoToOneCm,
                         unsigned* pLinkOneTwo, unsigned* pLinkTwoOne)
{
	GraphNode* one = GetNodeManager().GetNode(oneNodeIndex);
	GraphNode* two = GetNodeManager().GetNode(twoNodeIndex);

	if (pLinkOneTwo)
		*pLinkOneTwo = 0;
	if (pLinkTwoOne)
		*pLinkTwoOne = 0;

	if (one == two)
		return;

	if (!one || !two)
		return;

	if (!CGraph::ValidateNode(oneNodeIndex, false) || !CGraph::ValidateNode(twoNodeIndex, false))
	{
		AIError("CGraph::Connect Attempt to connect nodes that aren't created [Code bug]");
		return;
	}

	if ((one == m_pSafeFirst || two == m_pSafeFirst) && m_pSafeFirst->firstLinkIndex)
	{
		AIWarning("Second link being made to safe/first node");
		return;
	}

#ifdef CRYAISYSTEM_DEBUG
	extern std::vector<const GraphNode*> g_DebugGraphNodesToDraw;
	g_DebugGraphNodesToDraw.clear();
#endif //CRYAISYSTEM_DEBUG

	// handle case where they're already connected
	unsigned linkIndexOne = one->GetLinkTo(GetNodeManager(), GetLinkManager(), two);
	unsigned linkIndexTwo = two->GetLinkTo(GetNodeManager(), GetLinkManager(), one);

	if ((linkIndexOne == 0) != (linkIndexTwo == 0))
	{
		AIWarning("Trying to connect links but one is already connected, other isn't");
		return;
	}

	// Check that if both links have bidirectional data, then it is the same.
	assert(linkIndexOne == 0 || linkIndexTwo == 0 || (linkIndexOne & ~1) == (linkIndexTwo & ~1));

	// Create new bidirectional data if necessary.
	unsigned linkIndex = linkIndexOne;
	if (!linkIndex && linkIndexTwo)
		linkIndex = linkIndexTwo ^ 1;
	if (!linkIndex)
		linkIndex = m_pGraphLinkManager->CreateLink();

	Vec3 midPos = 0.5f * (one->GetPos() + two->GetPos());

	if (!linkIndexOne)
	{
		// [1/3/2007 MichaelS] Should be possible to push link on front, but I don't want to run the risk
		// of breaking code that relies on the link order being preserved, so we add it to the end.
		//one->links.push_back(linkIndex);
		if (!one->firstLinkIndex)
			one->firstLinkIndex = linkIndex;
		else
		{
			unsigned lastLink, nextLink;
			for (lastLink = one->firstLinkIndex; nextLink = m_pGraphLinkManager->GetNextLink(lastLink); lastLink = nextLink)
				;
			m_pGraphLinkManager->SetNextLink(lastLink, linkIndex);
		}

		linkIndexOne = linkIndex;
		GetLinkManager().SetNextNode(linkIndex, twoNodeIndex);
		GetLinkManager().SetRadiusInCm(linkIndex, two == m_pSafeFirst ? -100 : radiusOneToTwoCm);
		GetLinkManager().GetEdgeCenter(linkIndex) = midPos;
		two->AddRef();
	}
	else
	{
		if (radiusOneToTwoCm != 0)
			GetLinkManager().ModifyRadiusInCm(linkIndexOne, radiusOneToTwoCm);
	}
	if (!linkIndexTwo)
	{
		// [1/3/2007 MichaelS] Should be possible to push link on front, but I don't want to run the risk
		// of breaking code that relies on the link order being preserved, so we add it to the end.
		//one->links.push_back(linkIndex);
		if (!two->firstLinkIndex)
			two->firstLinkIndex = linkIndex ^ 1;
		else
		{
			unsigned lastLink, nextLink;
			for (lastLink = two->firstLinkIndex; nextLink = m_pGraphLinkManager->GetNextLink(lastLink); lastLink = nextLink)
				;
			m_pGraphLinkManager->SetNextLink(lastLink, linkIndex ^ 1);
		}

		linkIndexTwo = linkIndex ^ 1;
		GetLinkManager().SetNextNode(linkIndex ^ 1, oneNodeIndex);
		GetLinkManager().SetRadiusInCm(linkIndex ^ 1, one == m_pSafeFirst ? -100 : radiusTwoToOneCm);
		GetLinkManager().GetEdgeCenter(linkIndex ^ 1) = midPos;
		one->AddRef();
	}
	else
	{
		if (radiusTwoToOneCm != 0)
			GetLinkManager().ModifyRadiusInCm(linkIndexTwo, radiusTwoToOneCm);
	}

	if (pLinkOneTwo && linkIndexOne)
		*pLinkOneTwo = linkIndexOne;
	if (pLinkTwoOne && linkIndexTwo)
		*pLinkTwoOne = linkIndexTwo;

	if (m_pSafeFirst->firstLinkIndex)
		return;

	if (one->navType == IAISystem::NAV_TRIANGULAR)
	{
		ConnectInCm(m_safeFirstIndex, oneNodeIndex, 10000, -100);
		m_pFirst = one;
		m_firstIndex = oneNodeIndex;
	}
	else if (two->navType == IAISystem::NAV_TRIANGULAR)
	{
		ConnectInCm(m_safeFirstIndex, twoNodeIndex, 10000, -100);
		m_pFirst = two;
		m_firstIndex = twoNodeIndex;
	}
	// may have incurred a reallocation
	// [1/3/2007 MichaelS] Should no longer be necessary since we use indices instead of pointers, but leaving it here for now.
	if (pLinkOneTwo && linkIndexOne)
		*pLinkOneTwo = linkIndexOne;
	if (pLinkTwoOne && linkIndexTwo)
		*pLinkTwoOne = linkIndexTwo;
}

/// Connects (two-way) two nodes, optionally returning pointers to the new links
void CGraph::Connect(unsigned oneIndex, unsigned twoIndex, float radiusOneToTwo, float radiusTwoToOne,
                     unsigned* pLinkOneTwo, unsigned* pLinkTwoOne)
{
	int16 radiusOneToTwoInCm = NavGraphUtils::InCentimeters(radiusOneToTwo);
	int16 radiusTwoToOneInCm = NavGraphUtils::InCentimeters(radiusTwoToOne);

	ConnectInCm(oneIndex, twoIndex, radiusOneToTwoInCm, radiusTwoToOneInCm, pLinkOneTwo, pLinkTwoOne);
}

//====================================================================
// DeleteGraph
//====================================================================
void CGraph::DeleteGraph(IAISystem::tNavCapMask navTypeMask)
{
	std::vector<unsigned> nodesToDelete;
	CAllNodesContainer::Iterator it(m_allNodes, navTypeMask);
	while (unsigned nodeIndex = it.Increment())
	{
		GraphNode* pNode = GetNodeManager().GetNode(nodeIndex);
		AIAssert(pNode->navType & navTypeMask);
		nodesToDelete.push_back(nodeIndex);
	}

	for (unsigned i = 0; i < nodesToDelete.size(); ++i)
	{
		GraphNode* pNode = GetNodeManager().GetNode(nodesToDelete[i]);
		Disconnect(nodesToDelete[i]);
		if (pNode == m_pSafeFirst)
		{
			m_pSafeFirst = 0;
			m_safeFirstIndex = 0;
		}
	}

	m_allNodes.Compact();
}

//====================================================================
// Disconnect
//====================================================================
void CGraph::Disconnect(unsigned nodeIndex, unsigned linkId)
{
	GraphNode* pNode = m_pGraphNodeManager->GetNode(nodeIndex);

	if (!CGraph::ValidateNode(nodeIndex, false))
	{
		AIError("CGraph::Disconnect Attempt to disconnect link from node that isn't created [Code bug]");
		return;
	}
	AIAssert(linkId);
	unsigned otherNodeIndex = GetLinkManager().GetNextNode(linkId);
	GraphNode* pOtherNode = GetNodeManager().GetNode(otherNodeIndex);
	if (!CGraph::ValidateNode(otherNodeIndex, false))
	{
		AIError("CGraph::Disconnect Attempt to disconnect link from other node that isn't created [Code bug]");
		return;
	}

	pNode->RemoveLinkTo(GetLinkManager(), otherNodeIndex);
	pNode->Release();

	pOtherNode->RemoveLinkTo(GetLinkManager(), nodeIndex);
	pOtherNode->Release();

	GetLinkManager().DestroyLink(linkId);
}

//====================================================================
// Disconnect
//====================================================================
void CGraph::Disconnect(unsigned nodeIndex, bool bDelete)
{
	GraphNode* pDisconnected = m_pGraphNodeManager->GetNode(nodeIndex);

	if (!CGraph::ValidateNode(nodeIndex, false))
	{
		AIError("CGraph::Disconnect Attempt to disconnect node that isn't created [Code bug]");
		return;
	}

#ifdef CRYAISYSTEM_DEBUG
	extern std::vector<const GraphNode*> g_DebugGraphNodesToDraw;
	g_DebugGraphNodesToDraw.clear();
#endif //CRYAISYSTEM_DEBUG

	// if the node we are disconnecting is the current node, move the current
	// to one of his links, or the root if it has no links
	if (pDisconnected == m_pCurrent)
	{
		if (pDisconnected->firstLinkIndex)
		{
			m_currentIndex = GetLinkManager().GetNextNode(pDisconnected->firstLinkIndex);
			m_pCurrent = GetNodeManager().GetNode(m_currentIndex);
		}
		else
		{
			m_currentIndex = m_safeFirstIndex;
			m_pCurrent = m_pSafeFirst;
		}
	}

	// if its the root that is being disconnected, move it
	if (m_pFirst == pDisconnected)
	{
		if (pDisconnected->firstLinkIndex)
		{
			m_firstIndex = GetLinkManager().GetNextNode(pDisconnected->firstLinkIndex);
			m_pFirst = GetNodeManager().GetNode(m_firstIndex);
		}
		else
		{
			if (m_pFirst != m_pSafeFirst)
			{
				m_pFirst = m_pSafeFirst;
				m_firstIndex = m_safeFirstIndex;
			}
			else
			{
				m_pFirst = 0;
				m_firstIndex = 0;
			}
		}
	}

	// now disconnect this node from its links
	for (unsigned link = pDisconnected->firstLinkIndex, nextLink; link; link = nextLink)
	{
		nextLink = GetLinkManager().GetNextLink(link);

		unsigned nextNodeIndex = GetLinkManager().GetNextNode(link);
		GraphNode* pNextNode = GetNodeManager().GetNode(nextNodeIndex);
		pNextNode->RemoveLinkTo(GetLinkManager(), nodeIndex);
		pNextNode->Release();
		pDisconnected->Release();
		GetLinkManager().DestroyLink(link);
	}

	pDisconnected->firstLinkIndex = 0;

	if (pDisconnected->nRefCount != 1)
		AIWarning("Node reference count is not 1 after disconnecting");

	if (bDelete)
		DeleteNode(nodeIndex);

	if (!m_pSafeFirst)
		return;

	if (pDisconnected != m_pSafeFirst && !m_pSafeFirst->firstLinkIndex)
	{
		unsigned firstIndex = m_firstIndex;
		// we have disconnected the link to the dummy safe node - relink it to any outdoor node of the graph
		if (firstIndex == m_safeFirstIndex)
		{
			if (m_currentIndex == m_safeFirstIndex)
			{
				// try any entrance
				if (!m_mapEntrances.empty())
					firstIndex = (m_mapEntrances.begin()->second);
				else
					return; // m_pSafeFirst links will stay empty

			}
			else
			{
				firstIndex = m_currentIndex;
			}
		}

		if (firstIndex)
		{
			GraphNode* pFirst = GetNodeManager().GetNode(firstIndex);
			if (pFirst->navType == IAISystem::NAV_TRIANGULAR)
				ConnectInCm(m_safeFirstIndex, firstIndex, 10000, -100);
			else if (pFirst->navType == IAISystem::NAV_WAYPOINT_HUMAN)
			{
				GraphNode* pEntrance = GetEntrance(pFirst->GetWaypointNavData()->nBuildingID, Vec3(0, 0, 0));
				if (pEntrance)
				{
					for (unsigned link = pEntrance->firstLinkIndex; link; link = GetLinkManager().GetNextLink(link))
					{
						unsigned nextNodeIndex = GetLinkManager().GetNextNode(link);
						GraphNode* pNextNode = GetNodeManager().GetNode(nextNodeIndex);
						if (pNextNode->navType == IAISystem::NAV_WAYPOINT_HUMAN)
						{
							ConnectInCm(m_safeFirstIndex, nextNodeIndex, 10000, -100);
							break;
						}
					}
				}
			}
		}
	}
}

bool operator<(const Vec3r& v1, const Vec3r& v2)
{
	if (v1.x < v2.x)
		return true;
	else if (v1.x > v2.y)
		return false;
	if (v1.y < v2.y)
		return true;
	else if (v1.y > v2.y)
		return false;
	if (v1.z < v2.z)
		return true;
	else if (v1.z > v2.z)
		return false;
	return false;
}

//====================================================================
// PointInTriangle
// Check whether a position is within a node's triangle
//====================================================================
bool PointInTriangle(const Vec3& pos, GraphNode* pNode)
{
	// NAV_TRIANGULAR is replaced by MNM
	assert(false);
	return false;
}

//#define VALIDATEINMARK

//====================================================================
// MarkNode
// uses mark for internal graph operation without disturbing the pathfinder
//====================================================================
void CGraph::MarkNode(unsigned nodeIndex) const
{
#ifdef VALIDATEINMARK
	if (!CGraph::ValidateNode(nodeIndex))
	{
		AIError("CGraph::MarkNode Unable to validate/mark node %p [Code bug]", pNode);
		return;
	}
#endif
	GetNodeManager().GetNode(nodeIndex)->mark = 1;
	m_markedNodes.push_back(nodeIndex);
}

//====================================================================
// ClearMarks
// clears the marked nodes
//====================================================================
void CGraph::ClearMarks() const
{
	while (!m_markedNodes.empty())
	{
#ifdef VALIDATEINMARK
		if (!CGraph::ValidateNode(m_markedNodes.back()))
			AIError("CGraph::ClearMarks Unable to validate/clear mark node %p [Code bug]", m_markedNodes.back());
		else
#endif
		GetNodeManager().GetNode(m_markedNodes.back())->mark = 0;
		m_markedNodes.pop_back();
	}
}

//====================================================================
// ReadFromFile
// Reads the AI graph from a specified file
//====================================================================
bool CGraph::ReadFromFile(const char* szName)
{
	CCryFile file;
	;
	if (file.Open(szName, "rb"))
		return ReadNodes(file);
	else
		return false;
}

//====================================================================
// ReadNodes
// reads all the nodes in a map
//====================================================================
bool CGraph::ReadNodes(CCryFile& file)
{
	NodeDescBuffer nodeDescs;

	//AIAssert(_heapchk()==_HEAPOK);
	int iNumber;
	Vec3 mins, maxs;

	AILogLoading("Verifying BAI file version");
	file.ReadType(&iNumber);
	if (iNumber != BAI_TRI_FILE_VERSION)
	{
		AIError("CGraph::ReadNodes Wrong triangulation BAI file version - found %d expected %d: Regenerate triangulation in the editor [Design bug]", iNumber, BAI_TRI_FILE_VERSION);
		return false;
	}

	AILogLoading("Reading BBOX");
	file.ReadType(&mins);
	file.ReadType(&maxs);

	SetBBox(mins, maxs);

	AILogLoading("Reading node descriptors");

	file.ReadType(&iNumber);

	if (iNumber > 0)
	{
		nodeDescs.resize(iNumber);
		file.ReadType(&nodeDescs[0], iNumber);
	}

	AILogLoading("Verifying graph nodes");
	bool bFoundFirstNode = false;

	for (NodeDescBuffer::const_iterator ni = nodeDescs.begin(), end = nodeDescs.end(); ni != end; ++ni)
	{
		if (ni->ID == 1)
		{
			bFoundFirstNode = true;
			break;
		}
	}

	if (bFoundFirstNode)
	{
		// m_pSafeFirstIndex overlaps an ID we've read in. We need to disconnect m_pSafeFirst
		// before we create a node with the same ID to avoid accidentally reusing the ID.
		if (m_pSafeFirst)
			Disconnect(m_safeFirstIndex);
		m_pSafeFirst = NULL;
		m_safeFirstIndex = 0;
	}
	else
	{
		AIError("CGraph::ReadNodes First node not found - navigation loading failed [code bug]");
		gEnv->pLog->UpdateLoadingScreen(" ");
		return false;
	}

	AILogLoading("Creating graph nodes");

	typedef std::map<unsigned, unsigned> ReadNodesMap;
	ReadNodesMap mapReadNodes;

	I3DEngine* pEngine = gEnv->p3DEngine;
	NodeDescBuffer::iterator ni;
	int index = 0;

	if (/*!gEnv->IsEditor() && */ iNumber)
	{
		//int nodeCounts[IAISystem::NAV_TYPE_COUNT] = {0};

		NodeDescBuffer::iterator end = nodeDescs.end();
		//for (ni=nodeDescs.begin();ni != end;++ni,++index)
		//{
		//	NodeDescriptor desc = (*ni);
		//	IAISystem::ENavigationType type = (IAISystem::ENavigationType) desc.navType;

		//	int typeIndex = TypeIndexFromType(type);

		//	// in editor waypoint nodes get added by the editor
		//	// MichaelS - removed check to improve stats consistency.
		//	if (typeIndex < 0 ||
		//		(/*type != IAISystem::NAV_TRIANGULAR || */type != IAISystem::NAV_UNSET && gEnv->IsEditor()))
		//		continue;

		//	++nodeCounts[typeIndex];
		//}

		for (ni = nodeDescs.begin(); ni != end; ++ni, ++index)
		{
			NodeDescriptor desc = (*ni);
			IAISystem::ENavigationType type = (IAISystem::ENavigationType) desc.navType;

			unsigned int nodeIndex = 0;
			GraphNode* pNode = 0;
			nodeIndex = CreateNewNode(type, desc.pos, desc.ID);
			pNode = GetNodeManager().GetNode(nodeIndex);

			switch (type)
			{
			case IAISystem::NAV_UNSET:
				break;
			case IAISystem::NAV_TRIANGULAR:
				{
					// NAV_TRIANGULAR is replaced by MNM
					assert(false);
				}
				break;
			case IAISystem::NAV_WAYPOINT_3DSURFACE:
			case IAISystem::NAV_WAYPOINT_HUMAN:
				{
					pNode->GetWaypointNavData()->type = (EWaypointNodeType) desc.type;

					// building id isn't preserved
					pNode->GetWaypointNavData()->nBuildingID = -1;
					pNode->GetWaypointNavData()->pArea = 0;

					if (desc.type == WNT_ENTRYEXIT)
						m_mapEntrances.insert(EntranceMap::iterator::value_type(pNode->GetWaypointNavData()->nBuildingID, nodeIndex));

					if (desc.type == WNT_ENTRYEXIT || desc.type == WNT_EXITONLY)
						m_mapExits.insert(EntranceMap::iterator::value_type(pNode->GetWaypointNavData()->nBuildingID, nodeIndex));

					// NOTE Oct 15, 2009: <pvl> removable nodes have been unused for
					// a very long time now, removing support altogether
					assert(!desc.bRemovable);

					pNode->GetWaypointNavData()->dir = desc.dir;
					pNode->GetWaypointNavData()->up = desc.up;
				}
				break;
			case IAISystem::NAV_FLIGHT:
				AIAssert(!"flight nav should be loaded separately");
				pNode->GetFlightNavData()->nSpanIdx = desc.index;
				break;
			case IAISystem::NAV_VOLUME:
				AIAssert(!"volume nav should be loaded separately");
				pNode->GetVolumeNavData()->nVolimeIdx = desc.index;
				break;
			case IAISystem::NAV_ROAD:
				AIAssert(!"road nav should be loaded separately");
				pNode->GetRoadNavData()->fRoadWidth = desc.dir.x;
				/*pNode->GetRoadNavData()->fRoadOffset = desc.dir.y;*/
				break;
			case IAISystem::NAV_SMARTOBJECT:
				AIAssert(!"smart object nav should be loaded separately");
				break;
			default:
				AIError("CGraph::ReadNodes Unhandled nav type %d", pNode->navType);
			}
			mapReadNodes.insert(ReadNodesMap::value_type(desc.ID, nodeIndex));

		}

	}

	AILogLoading("Reading links");

	LinkDescBuffer linkDescs;

	file.ReadType(&iNumber);
	if (iNumber > 0)
	{
		linkDescs.resize(iNumber);
		file.ReadType(&linkDescs[0], iNumber);
	}

	ReadNodesMap::iterator ei, link;

	ei = mapReadNodes.find(1);
	AIAssert(ei != mapReadNodes.end());
	m_safeFirstIndex = (ei->second);
	m_pSafeFirst = GetNodeManager().GetNode(m_safeFirstIndex);
	m_pFirst = m_pSafeFirst;
	m_firstIndex = m_safeFirstIndex;
	m_pCurrent = m_pSafeFirst;
	m_currentIndex = m_safeFirstIndex;

	float fStartTime = gEnv->pTimer->GetAsyncCurTime();
	AILogLoading("Reconnecting links");

	if (!linkDescs.empty())
	{
		LinkDescBuffer::iterator iend = linkDescs.end();
		for (LinkDescBuffer::iterator ldbi = linkDescs.begin(); ldbi != iend; ++ldbi)
		{
			LinkDescriptor& ldb = (*ldbi);

			ei = mapReadNodes.find((unsigned)ldb.nSourceNode);
			if (ei == mapReadNodes.end())
			{
#if defined __GNUC__
				AIWarning("NodeId %lld not found in node map", (long long)ldb.nSourceNode);
#else
				AIWarning("NodeId %I64d not found in node map", (int64)ldb.nSourceNode);
#endif
			}
			else
			{
				unsigned nodeIndex = ei->second;
				GraphNode* pNode = GetNodeManager().GetNode(nodeIndex);
				link = mapReadNodes.find((unsigned int)ldb.nTargetNode);
				if (link == mapReadNodes.end() || ei == mapReadNodes.end())
				{
					AIError("CGraph::ReadNodes Read a link to a node which could not be found [Code bug]");
				}
				else
				{
					if (pNode != m_pSafeFirst && link->second != m_safeFirstIndex)
					{
						unsigned linkOneTwo = 0;
						unsigned linkTwoOne = 0;
						// incoming link gets set when the other way is read
						Connect(nodeIndex, link->second, ldb.fMaxPassRadius, 0.0f, &linkOneTwo, &linkTwoOne);

						if (linkOneTwo)
						{
							unsigned nextNodeIndex = link->second;
							GraphNode* pNextNode = GetNodeManager().GetNode(nextNodeIndex);
							AIAssert(GetLinkManager().GetNextNode(linkOneTwo) == nextNodeIndex);

							if (GetLinkManager().GetNextNode(linkOneTwo) == link->second)
							{
								GetLinkManager().SetRadius(linkOneTwo, ldb.fMaxPassRadius);
								GetLinkManager().SetStartIndex(linkOneTwo, ldb.nStartIndex);
								GetLinkManager().SetEndIndex(linkOneTwo, ldb.nEndIndex);
								GetLinkManager().GetEdgeCenter(linkOneTwo) = ldb.vEdgeCenter; // TODO: Don't read shared value twice
								GetLinkManager().SetExposure(linkOneTwo, ldb.fExposure);      // TODO: Don't read shared value twice
								GetLinkManager().SetMaxWaterDepth(linkOneTwo, ldb.fMaxWaterDepth);
								GetLinkManager().SetMinWaterDepth(linkOneTwo, ldb.fMinWaterDepth);
								GetLinkManager().SetSimple(linkOneTwo, ldb.bSimplePassabilityCheck);
							}
							if (linkTwoOne)
								GetLinkManager().RestoreLink(linkTwoOne);
						}
					}
				}
			}
		}
	}

	AILogLoading("Finished Reconnecting links in %6.3f sec",
	             gEnv->pTimer->GetAsyncCurTime() - fStartTime);

	mapReadNodes.clear();
	//AIAssert(_heapchk()==_HEAPOK);

	return true;
}

//====================================================================
// SetBBox
// defines bounding rectangle of this graph
//====================================================================
void CGraph::SetBBox(const Vec3& min, const Vec3& max)
{
	m_triangularBBox.min = min;
	m_triangularBBox.max = max;
}

//====================================================================
// InsideOfBBox
//====================================================================
bool CGraph::InsideOfBBox(const Vec3& pos) const
{
	return pos.x > m_triangularBBox.min.x && pos.x < m_triangularBBox.max.x && pos.y > m_triangularBBox.min.y && pos.y < m_triangularBBox.max.y;
}

unsigned CGraph::CreateNewNode(IAISystem::tNavCapMask type, const Vec3& pos, unsigned ID)
{
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Navigation, 0, "Graph Node (%s)", StringFromTypeIndex(TypeIndexFromType(type)));
	//GraphNode *pNode = new GraphNode(type, pos, ID);
	//GraphNode *pNode = NodesPool.AddGraphNode(type, pos, ID);
	unsigned nodeIndex = m_pGraphNodeManager->CreateNode(type, pos, ID);
	GraphNode* pNode = m_pGraphNodeManager->GetNode(nodeIndex);
	pNode->AddRef();
	m_allNodes.AddNode(nodeIndex);

	return nodeIndex;
}

//====================================================================
// GetNode
//====================================================================
GraphNode* CGraph::GetNode(unsigned index)
{
	return GetNodeManager().GetNode(index);
}

const GraphNode* CGraph::GetNode(unsigned index) const
{
	return GetNodeManager().GetNode(index);
}

//====================================================================
// MoveNode
//====================================================================
void CGraph::MoveNode(unsigned nodeIndex, const Vec3& newPos)
{
	GraphNode* pNode = GetNodeManager().GetNode(nodeIndex);
	if (pNode->GetPos().IsEquivalent(newPos))
		return;
	m_allNodes.RemoveNode(nodeIndex);
	pNode->SetPos(newPos);
	m_allNodes.AddNode(nodeIndex);
}

struct SNodeFinder
{
	SNodeFinder(unsigned nodeIndex) : nodeIndex(nodeIndex) {}
	bool operator()(const EntranceMap::value_type& val) const { return val.second == nodeIndex; }
	unsigned nodeIndex;
};

//====================================================================
// DeleteNode
//====================================================================
void CGraph::DeleteNode(unsigned nodeIndex)
{
	GraphNode* pNode = GetNodeManager().GetNode(nodeIndex);

	if (!CGraph::ValidateNode(nodeIndex, false))
	{
		AIError("CGraph::DeleteNode Attempting to delete node that doesn't exist %p [Code bug]", pNode);
		return;
	}

	if (pNode->firstLinkIndex)
	{
		AIWarning("Deleting node %p but it is still connected - disconnecting", pNode);
		Disconnect(nodeIndex, false);
	}

	if (pNode->Release())
	{
		m_allNodes.RemoveNode(nodeIndex);

		VectorConstNodeIndices::iterator it;
		it = std::remove(m_taggedNodes.begin(), m_taggedNodes.end(), nodeIndex);
		m_taggedNodes.erase(it, m_taggedNodes.end());
		it = std::remove(m_markedNodes.begin(), m_markedNodes.end(), nodeIndex);
		m_markedNodes.erase(it, m_markedNodes.end());

		EntranceMap::iterator entranceIt;
		entranceIt = std::find_if(m_mapEntrances.begin(), m_mapEntrances.end(), SNodeFinder(nodeIndex));
		if (entranceIt != m_mapEntrances.end())
			m_mapEntrances.erase(entranceIt);
		entranceIt = std::find_if(m_mapExits.begin(), m_mapExits.end(), SNodeFinder(nodeIndex));
		if (entranceIt != m_mapExits.end())
			m_mapExits.erase(entranceIt);

		m_pGraphNodeManager->DestroyNode(nodeIndex);
	}
}

struct SVolumeHideSpotFinder
{
	SVolumeHideSpotFinder(ListObstacles& obs) : obs(obs) {}

	void operator()(const SVolumeHideSpot& hs, float)
	{
		obs.push_back(ObstacleData(hs.pos, hs.dir));
	}

private:
	ListObstacles& obs;
};

//===================================================================
// FindNodesWithinRange
//===================================================================
void CGraph::FindNodesWithinRange(MapConstNodesDistance& result, float curDistT, float maxDist,
                                  const GraphNode* pStartNode, float passRadius, const class CAIObject* pRequester) const
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_AI);

	bool checkWaterDepth = false;
	int16 minWaterDepthInCm = 0;
	int16 maxWaterDepthInCm = 0;

	if (pRequester && pRequester->CastToCAIActor())
	{
		const CAIActor* pActor = pRequester->CastToCAIActor();
		if (pActor)
		{
			minWaterDepthInCm = NavGraphUtils::InCentimeters(pActor->m_movementAbility.pathfindingProperties.minWaterDepth);
			maxWaterDepthInCm = NavGraphUtils::InCentimeters(pActor->m_movementAbility.pathfindingProperties.maxWaterDepth);
			checkWaterDepth = true;
		}
	}

	const CGraphLinkManager& linkManager = GetLinkManager();
	const CGraphNodeManager& nodeManager = GetNodeManager();

	typedef std::multimap<float, const GraphNode*, std::less<float>> OpenListMap;

	static OpenListMap openList;
	openList.clear();

	pStartNode->mark = 1;
	openList.insert(std::make_pair(0.0f, pStartNode));
	//	openList[0.0f] = pStartNode;

	result[pStartNode] = 0.0f;

	while (!openList.empty())
	{
		OpenListMap::iterator front = openList.begin();
		const GraphNode* pNode = front->second;
		float curDist = front->first;
		openList.erase(front);
		pNode->mark = 0;

		for (unsigned link = pNode->firstLinkIndex; link; link = linkManager.GetNextLink(link))
		{
			float fCostMultiplier = 1.0f;
			unsigned int nextNodeIndex = linkManager.GetNextNode(link);
			const GraphNode* pNext = nodeManager.GetNode(nextNodeIndex);

			if (!(pNext->navType & (IAISystem::NAV_SMARTOBJECT | IAISystem::NAV_WAYPOINT_HUMAN | IAISystem::NAV_WAYPOINT_3DSURFACE | IAISystem::NAV_TRIANGULAR | IAISystem::NAV_VOLUME)))
				continue;

			if (pRequester && pNode->navType == IAISystem::NAV_SMARTOBJECT && pNext->navType == IAISystem::NAV_SMARTOBJECT)
			{
				const GraphNode* nodes[2] = { pNode, pNext };
				float resFactor = gAIEnv.pSmartObjectManager->GetSmartObjectLinkCostFactor(nodes, pRequester, &fCostMultiplier);
				if (resFactor < 0.0f)
					continue;
			}
			else if (linkManager.GetRadius(link) < passRadius)
			{
				continue;
			}
			else if (checkWaterDepth)
			{
				int16 wd = linkManager.GetMaxWaterDepthInCm(link);
				if (wd > maxWaterDepthInCm)
					continue;

				if (wd < minWaterDepthInCm)
					continue;
			}

			float linkLen = 0.01f + fCostMultiplier * Distance::Point_Point(pNode->GetPos(), pNext->GetPos()); // bias to prevent loops

			float totalDist = curDist + linkLen;
			if (totalDist <= maxDist)
			{
				// If we've already processed pNext and had a lower range value before then don't continue
				MapConstNodesDistance::iterator it = result.find(pNext);
				if (it != result.end())
				{
					if (totalDist >= it->second)
						continue;
					// Smaller distance, update value.
					it->second = totalDist;
				}
				else
				{
					result[pNext] = totalDist;
				}

				// Update open list.
				OpenListMap::iterator found = openList.end();
				if (pNext->mark == 1)
				{
					for (OpenListMap::iterator it2 = openList.begin(), end = openList.end(); it2 != end; ++it2)
					{
						if (it2->second == pNext)
						{
							found = it2;
							break;
						}
					}
				}

				if (found != openList.end())
				{
					// Replace value in open list.
					if (totalDist < found->first)
					{
						openList.erase(found);
						openList.insert(std::make_pair(totalDist, pNext));
					}
				}
				else
				{
					// Add to the open list
					pNext->mark = 1;
					openList.insert(std::make_pair(totalDist, pNext));
				}
			}
		}
	}
	openList.clear();
}

//====================================================================
// GetNodesInRange
//====================================================================
MapConstNodesDistance& CGraph::GetNodesInRange(MapConstNodesDistance& result, const Vec3& startPos,
                                               float maxDist, IAISystem::tNavCapMask navCapMask, float passRadius,
                                               unsigned startNodeIndex, const class CAIObject* pRequester)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_AI);
	result.clear();

	GraphNode* pStart = GetNodeManager().GetNode(startNodeIndex);

	const CAllNodesContainer& allNodes = GetAllNodes();
	const CAllNodesContainer::Iterator it(allNodes, IAISystem::NAV_TRIANGULAR | IAISystem::NAV_VOLUME);
	if (!it.GetNode())
		return result; // no navigation

	unsigned nodeIndex = 0;
	if (pStart && !allNodes.DoesNodeExist(startNodeIndex))
	{
		startNodeIndex = 0;
		pStart = 0;
	}

	if (pStart)
	{
		if ((pStart->navType & navCapMask) && startPos.IsEquivalent(pStart->GetPos(), 0.01f))
		{
			nodeIndex = startNodeIndex;
		}
	}

	if (!nodeIndex)
	{
		return result;
	}

	// don't add the distance to the current node since that
	float curDist = 0.0f; //Distance::Point_Point(startPos, pNode->GetPos());
	FindNodesWithinRange(result, curDist, maxDist, GetNodeManager().GetNode(nodeIndex), passRadius, pRequester);
	return result;
}

//====================================================================
// Reset
//====================================================================
void CGraph::Reset()
{
	ClearMarks();
	RestoreAllNavigation();
}

void CGraph::ResetIDs()
{
	GraphNode::ResetIDs(GetNodeManager(), m_allNodes, m_pSafeFirst);
}

//====================================================================
// GetEntrance
//====================================================================
GraphNode* CGraph::GetEntrance(int nBuildingID, const Vec3& pos)
{
	GraphNode* pEntrance = 0;
	float mindist = 1000000;
	EntranceMap::iterator ei = m_mapEntrances.find(nBuildingID);
	if (ei != m_mapEntrances.end())
	{
		pEntrance = GetNodeManager().GetNode(ei->second);
		if (m_mapEntrances.count(nBuildingID) > 1)
		{
			mindist = (pEntrance->GetPos() - pos).GetLengthSquared();
			for (; ei != m_mapEntrances.end(); ++ei)
			{
				if (ei->first != nBuildingID)
					break;
				float curr_dist = (GetNodeManager().GetNode(ei->second)->GetPos() - pos).GetLengthSquared();
				if (curr_dist <= mindist)
				{
					pEntrance = GetNodeManager().GetNode(ei->second);
					mindist = curr_dist;
				}
			}
		}
	}

	ei = m_mapExits.find(nBuildingID);
	if (ei != m_mapExits.end())
	{
		pEntrance = GetNodeManager().GetNode(ei->second);
		if (m_mapExits.count(nBuildingID) > 1)
		{
			mindist = (pEntrance->GetPos() - pos).GetLengthSquared();
			for (; ei != m_mapExits.end(); ++ei)
			{
				if (ei->first != nBuildingID)
					break;
				float curr_dist = (GetNodeManager().GetNode(ei->second)->GetPos() - pos).GetLengthSquared();
				if (curr_dist <= mindist)
				{
					pEntrance = GetNodeManager().GetNode(ei->second);
					mindist = curr_dist;
				}
			}
		}
	}

	return pEntrance;
}

//====================================================================
// GetEntrances
//====================================================================
bool CGraph::GetEntrances(int nBuildingID, const Vec3& pos, std::vector<unsigned>& nodes)
{
	EntranceMap::iterator it1 = m_mapEntrances.lower_bound(nBuildingID);
	EntranceMap::iterator it2 = m_mapEntrances.upper_bound(nBuildingID);
	for (EntranceMap::iterator it = it1; it != it2; ++it)
		nodes.push_back(it->second);
	it1 = m_mapExits.lower_bound(nBuildingID);
	it2 = m_mapExits.upper_bound(nBuildingID);
	for (EntranceMap::iterator it = it1; it != it2; ++it)
		nodes.push_back(it->second);
	return !nodes.empty();
}

//====================================================================
// RestoreAllNavigation
// for all removable nodes restore links passibility etc
//====================================================================
void CGraph::RestoreAllNavigation()
{
	CAllNodesContainer::Iterator it(m_allNodes, IAISystem::NAVMASK_ALL);
	while (GraphNode* node = GetNodeManager().GetNode(it.Increment()))
	{
		for (unsigned link = node->firstLinkIndex; link; link = GetLinkManager().GetNextLink(link))
		{
			GetLinkManager().RestoreLink(link);
		}
	}
}

//====================================================================
// CheckForEmpty
//====================================================================
bool CGraph::CheckForEmpty(IAISystem::tNavCapMask navTypeMask) const
{
	unsigned count = 0;
	CAllNodesContainer::Iterator it(m_allNodes, navTypeMask);
	while (const GraphNode* node = GetNodeManager().GetNode(it.Increment()))
	{
		++count;
		AILogEvent("Unexpected Node %p, type = %d, pos = (%5.2f %5.2f %5.2f)", node, node->navType,
		           node->GetPos().x, node->GetPos().y, node->GetPos().z);
	}
	if (count)
		AIWarning("Detected %d unexpected nodes whilst checking types %u", count, (unsigned)navTypeMask);
	return (count == 0);
}

//====================================================================
// Validate
//====================================================================
bool CGraph::Validate(const char* msg, bool checkPassable) const
{
#ifdef _DEBUG
	return true;
#endif

#ifdef CRYAISYSTEM_VERBOSITY
	if (!AIGetWarningErrorsEnabled())
	{
		AILogEvent("CGraph::Validate Skipping: %s", msg);
		return true;
	}

	AILogProgress("CGraph::Validate Starting: %s", msg);

	// Danny todo tweak this so that if !checkPassable then we only clear
	// errors that are not to do with the passable checks...
	if (checkPassable)
		mBadGraphData.clear();

	if (!m_pFirst)
		return false;

	if (!m_pSafeFirst->firstLinkIndex)
		return true;

	// the first safe node is different - expect only one link
	if (GetLinkManager().GetNextLink(m_pSafeFirst->firstLinkIndex) != 0)
	{
		AIWarning("CGraph::Validate Expect only 1 link to the first safe node");
		return false;
	}

	int badOutdoorNodes = 0;
	int badIndoorNodes = 0;
	int badLinks = 0;
	int badNumOutsideLinks = 0;
	int badLinkPassability = 0;

	static int maxBadImassabilityWarnings = 10;

	// nodes containing vertices with identical indices
	int indexDegenerates = 0;
	// nodes containing vertices that have different indices, but essentially
	// the same position.
	int posDegenerates = 0;

	CAllNodesContainer::Iterator it(m_allNodes, IAISystem::NAVMASK_ALL);
	while (unsigned nodeIndex = it.Increment())
	{
		const GraphNode* node = GetNodeManager().GetNode(nodeIndex);
		if (!CGraph::ValidateNode(nodeIndex, true))
			AIWarning("CGraph::Validate Node validation failed: %p", node);

		if (node->navType == IAISystem::NAV_WAYPOINT_HUMAN)
		{
			if (node->GetWaypointNavData()->nBuildingID == -1)
			{
				AIWarning("Node at (%5.2f, %5.2f, %5.2f) is not in a human waypoint nav modifier [design bug]",
				          node->GetPos().x, node->GetPos().y, node->GetPos().z);
				++badIndoorNodes;
			}
		}
		else if (node->navType == IAISystem::NAV_WAYPOINT_3DSURFACE)
		{
			if (node->GetWaypointNavData()->nBuildingID == -1)
			{
				AIWarning("Node at (%5.2f, %5.2f, %5.2f) is not in a 3d surface nav modifier [design bug]",
				          node->GetPos().x, node->GetPos().y, node->GetPos().z);
				++badIndoorNodes;
			}
		}
		else if (node->navType == IAISystem::NAV_TRIANGULAR)
		{
			// NAV_TRIANGULAR is replaced by MNM
			assert(false);
		}
	}

	if (badOutdoorNodes + badIndoorNodes + badNumOutsideLinks + indexDegenerates + posDegenerates + badLinkPassability > 0)
	{
		AIError("CGraph::Validate AI graph contains invalid: %d outdoor, %d indoor, %d outdoor links, %d degenerate index, %d degenerate pos, %d passable: %s: Regenerate triangulation in editor [Design bug]",
		        badOutdoorNodes, badIndoorNodes, badNumOutsideLinks, indexDegenerates, posDegenerates, badLinkPassability, msg);
	}

	AILogProgress("CGraph::Validate Finished graph validation: %s", msg);
	return (0 == badOutdoorNodes + badIndoorNodes);
#else
	return true;
#endif
}

//===================================================================
// ResetIDs
//===================================================================
void GraphNode::ResetIDs(CGraphNodeManager& nodeManager, class CAllNodesContainer& allNodes, GraphNode* pNodeForID1)
{
	AIAssert(pNodeForID1);

	CAllNodesContainer::Iterator itAll(allNodes, 0xffffffff);

	freeIDs.clear();
#ifdef DEBUG_GRAPHNODE_IDS
	usedIds.clear();
#endif

	maxID = 1;
	while (GraphNode* pCurrent = nodeManager.GetNode(itAll.Increment()))
	{
		if (pCurrent == pNodeForID1)
			pCurrent->ID = 1;
		else
			pCurrent->ID = ++maxID;

#ifdef DEBUG_GRAPHNODE_IDS
		usedIds.push_back(pCurrent->ID);
#endif
	}
}
