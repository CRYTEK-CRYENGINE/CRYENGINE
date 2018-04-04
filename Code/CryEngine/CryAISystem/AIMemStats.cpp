// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include <CryMemory/CrySizer.h>
#include "CAISystem.h"
#include "Puppet.h"
#include "AIVehicle.h"
#include "AIPlayer.h"
#include "GoalPipe.h"
#include "GoalOp.h"
#include "AStarSolver.h"
#include "WorldOctree.h"
#include "ObjectContainer.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"

void CAISystem::GetMemoryStatistics(ICrySizer* pSizer)
{
#ifndef _LIB // Only when compiling as dynamic library
	{
		//SIZER_COMPONENT_NAME(pSizer,"Strings");
		//pSizer->AddObject( (this+1),string::_usedMemory(0) );
	}
	{
		SIZER_COMPONENT_NAME(pSizer, "STL Allocator Waste");
		CryModuleMemoryInfo meminfo;
		ZeroStruct(meminfo);
		CryGetMemoryInfoForModule(&meminfo);
		pSizer->AddObject((this + 2), (size_t)meminfo.STL_wasted);
	}
#endif

	//sizeof(bool) + sizeof(void*)*3 + sizeof(MapType::value_type)
	size_t size = 0;

	size = sizeof(*this);

	//  size+= (m_vWaitingToBeUpdated.size()+m_vAlreadyUpdated.size())*sizeof(CAIObject*);
	size += m_disabledAIActorsSet.size() * sizeof(CAIActor*);
	size += m_enabledAIActorsSet.size() * sizeof(CAIActor*);
	pSizer->AddObject(this, size);

	{
		//    char str[255];
		//    cry_sprintf(str,"%d AIObjects",m_Objects.size());
		SIZER_SUBCOMPONENT_NAME(pSizer, "AIObjects");      // string is used to identify component, it should not be dynamic

		// (MATT) TODO These and dummies etc should be included via the object container {2009/03/25}

		/*
		   AIObjects::iterator curObj = m_Objects.begin();

		   for(;curObj!=m_Objects.end();curObj++)
		   {
		   CAIObject *pObj = (CAIObject *) curObj->second;

		   size+=strlen(pObj->GetName());

		   if(CPuppet* pPuppet = pObj->CastToCPuppet())
		   {
		    size += pPuppet->MemStats();
		   }
		   else if(CAIPlayer* pPlayer = pObj->CastToCAIPlayer())
		   {
		    size += sizeof *pPlayer;
		   }
		   else if(CPuppet* pVehicle = pObj->CastToCAIVehicle())
		   {
		    size += sizeof *pVehicle;
		   }
		   else
		   {
		    size += sizeof *pObj;
		   }
		   }
		   pSizer->AddObject( &m_Objects, size );
		 */
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "NavGraph");

		if (m_pGraph)
		{
			pSizer->AddObject(m_pGraph, sizeof(*m_pGraph));
			m_pGraph->GetMemoryStatistics(pSizer);
		}

		if (m_pNavigation)
		{
			m_pNavigation->GetMemoryStatistics(pSizer);
		}
	}

	size = 0;

	{
		char str[255];
		cry_sprintf(str, "%" PRISIZE_T " GoalPipes", m_PipeManager.m_mapGoals.size());
		SIZER_SUBCOMPONENT_NAME(pSizer, "Goals");
		GoalMap::iterator gItr = m_PipeManager.m_mapGoals.begin();
		for (; gItr != m_PipeManager.m_mapGoals.end(); ++gItr)
		{
			size += (gItr->first).capacity();
			size += (gItr->second)->MemStats() + sizeof(*(gItr->second));
		}
		pSizer->AddObject(&m_PipeManager.m_mapGoals, size);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "ObjectContainer");
		pSizer->AddObject(gAIEnv.pObjectContainer, sizeof(*gAIEnv.pObjectContainer));
	}

	size = 0;
	FormationDescriptorMap::iterator fItr = m_mapFormationDescriptors.begin();
	for (; fItr != m_mapFormationDescriptors.end(); ++fItr)
	{
		size += (fItr->first).capacity();
		size += sizeof((fItr->second));
		size += (fItr->second).m_sName.capacity();
		size += (fItr->second).m_Nodes.size() * sizeof(FormationNode);
	}
	pSizer->AddObject(&m_mapFormationDescriptors, size);

	size = m_mapGroups.size() * (sizeof(unsigned short) + sizeof(CAIObject*));
	pSizer->AddObject(&m_mapGroups, size);

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "MNM Navigation System");
		if (gAIEnv.pNavigationSystem)
		{
			size = sizeof(NavigationSystem);
			pSizer->AddObject(gAIEnv.pNavigationSystem, size);
			gAIEnv.pNavigationSystem->GetMemoryStatistics(pSizer);
		}
	}
}

size_t GraphNode::MemStats()
{
	size_t size = 0;

	switch (navType)
	{
	case IAISystem::NAV_UNSET:
		size += sizeof(GraphNode_Unset);
		break;
	case IAISystem::NAV_TRIANGULAR:
		size += sizeof(GraphNode_Triangular);
		size += (GetTriangularNavData()->vertices.capacity() ? GetTriangularNavData()->vertices.capacity() * sizeof(int) + 2 * sizeof(int) : 0);
		break;
	case IAISystem::NAV_WAYPOINT_HUMAN:
		size += sizeof(GraphNode_WaypointHuman);
		break;
	case IAISystem::NAV_WAYPOINT_3DSURFACE:
		size += sizeof(GraphNode_Waypoint3DSurface);
		break;
	case IAISystem::NAV_FLIGHT:
		size += sizeof(GraphNode_Flight);
		break;
	case IAISystem::NAV_VOLUME:
		size += sizeof(GraphNode_Volume);
		break;
	case IAISystem::NAV_ROAD:
		size += sizeof(GraphNode_Road);
		break;
	case IAISystem::NAV_SMARTOBJECT:
		size += sizeof(GraphNode_SmartObject);
		break;
	case IAISystem::NAV_FREE_2D:
		size += sizeof(GraphNode_Free2D);
		break;
	default:
		break;
	}

	//size += links.capacity()*sizeof(unsigned);

	return size;
}

//====================================================================
// MemStats
//====================================================================
size_t CGraph::MemStats()
{
	size_t size = sizeof *this;
	size += sizeof(GraphNode*) * m_taggedNodes.capacity();
	size += sizeof(GraphNode*) * m_markedNodes.capacity();

	size += m_allNodes.MemStats();
	//size += NodesPool.MemStats();
	size += mBadGraphData.capacity() * sizeof(SBadGraphData);
	size += m_mapEntrances.size() * sizeof(bool) + sizeof(void*) * 3 + sizeof(EntranceMap::value_type);
	size += m_mapExits.size() * sizeof(bool) + sizeof(void*) * 3 + sizeof(EntranceMap::value_type);

	return size;
}

void CGraph::GetMemoryStatistics(ICrySizer* pSizer)
{
	pSizer->AddContainer(m_taggedNodes);
	pSizer->AddContainer(m_markedNodes);

	pSizer->AddContainer(mBadGraphData);

	pSizer->AddContainer(m_mapEntrances);
	pSizer->AddContainer(m_mapExits);

	pSizer->AddObject(&m_allNodes, m_allNodes.MemStats());

	pSizer->AddObject(m_pGraphLinkManager, sizeof(*m_pGraphLinkManager));
	m_pGraphLinkManager->GetMemoryStatistics(pSizer);

	pSizer->AddObject(m_pGraphNodeManager, sizeof(*m_pGraphNodeManager));
	m_pGraphNodeManager->GetMemoryStatistics(pSizer);
}

//====================================================================
// NodeMemStats
//====================================================================
size_t CGraph::NodeMemStats(unsigned navTypeMask)
{
	size_t nodesSize = 0;
	size_t linksSize = 0;

	CAllNodesContainer::Iterator it(m_allNodes, navTypeMask);
	while (unsigned nodeIndex = it.Increment())
	{
		GraphNode* pNode = GetNodeManager().GetNode(nodeIndex);

		if (navTypeMask & pNode->navType)
		{
			nodesSize += pNode->MemStats();
			/*
			      nodesSize += sizeof(GraphNode);
			      linksSize += pNode->links.capacity() * sizeof(GraphLink);
			      for (unsigned i = 0 ; i < pNode->links.size() ; ++i)
			      {
			        if (pNode->links[i].GetCachedPassabilityResult())
			          linksSize += sizeof(GraphLink::SCachedPassabilityResult);
			      }
			      switch (pNode->navType)
			      {
			        case IAISystem::NAV_TRIANGULAR:
			          nodesSize += sizeof(STriangularNavData);
			          nodesSize += pNode->GetTriangularNavData()->vertices.capacity() * sizeof(int);
			          break;
			        case IAISystem::NAV_WAYPOINT_3DSURFACE:
			        case IAISystem::NAV_WAYPOINT_HUMAN:
			          nodesSize += sizeof(SWaypointNavData);
			          break;
			        case IAISystem::NAV_SMARTOBJECT: nodesSize += sizeof(SSmartObjectNavData);
			          break;
			        default:
			          break;
			      }
			 */
		}
	}
	return nodesSize + linksSize;
}

//====================================================================
// MemStats
//====================================================================
size_t CGoalPipe::MemStats()
{
	size_t size = sizeof(*this);

	VectorOGoals::iterator itr = m_qGoalPipe.begin();

	for (; itr != m_qGoalPipe.end(); ++itr)
	{
		size += sizeof(QGoal);
		size += itr->sPipeName.capacity();
		size += sizeof(*(itr->pGoalOp));
		if (!itr->params.str.empty())
			size += itr->params.str.capacity();

		/*
		   QGoal *curGoal = itr;
		   size += sizeof *curGoal;
		   size += curGoal->name.capacity();
		   size += sizeof (*curGoal->pGoalOp);
		   size += strlen(curGoal->params.szString);
		 */
	}
	size += m_sName.capacity();
	return size;
}

size_t CPuppet::MemStats()
{
	size_t size = sizeof(*this);

	if (m_pCurrentGoalPipe)
		size += m_pCurrentGoalPipe->MemStats();

	/*
	   GoalMap::iterator itr=m_mapAttacks.begin();
	   for(; itr!=m_mapAttacks.end();itr++ )
	   {
	   size += (itr->first).capacity();
	   size += sizeof(CGoalPipe *);
	   }
	   itr=m_mapRetreats.begin();
	   for(; itr!=m_mapRetreats.end();itr++ )
	   {
	   size += (itr->first).capacity();
	   size += sizeof(CGoalPipe *);
	   }
	   itr=m_mapWanders.begin();
	   for(; itr!=m_mapWanders.end();itr++ )
	   {
	   size += (itr->first).capacity();
	   size += sizeof(CGoalPipe *);
	   }
	   itr=m_mapIdles.begin();
	   for(; itr!=m_mapIdles.end();itr++ )
	   {
	   size += (itr->first).capacity();
	   size += sizeof(CGoalPipe *);
	   }
	 */
	//	if(m_mapVisibleAgents.size() < 1000)
	//	size += (sizeof(CAIObject*)+sizeof(VisionSD))*m_mapVisibleAgents.size();
	//	if(m_mapMemory.size()<1000)
	//	size += (sizeof(CAIObject*)+sizeof(MemoryRecord))*m_mapMemory.size();
	if (m_mapDevaluedPoints.size() < 1000)
		size += (sizeof(CAIObject*) + sizeof(float)) * m_mapDevaluedPoints.size();
	//	if(m_mapPotentialTargets.size()<1000)
	//	size += (sizeof(CAIObject*)+sizeof(float))*m_mapPotentialTargets.size();
	//	if(m_mapSoundEvents.size()<1000)
	//	size += (sizeof(CAIObject*)+sizeof(SoundSD))*m_mapSoundEvents.size();

	return size;
}
//
//----------------------------------------------------------------------------------

//====================================================================
// MemStats
//====================================================================
size_t CAStarSolver::MemStats()
{
	size_t size = sizeof(*this);

	size += (sizeof(GraphNode*) + 4) * m_pathNodes.size();
	size += m_AStarNodeManager.MemStats();

	return size;
}

//====================================================================
// MemStats
//====================================================================
size_t CWorldOctree::MemStats()
{
	size_t size = sizeof(*this);

	size += m_cells.capacity() * sizeof(COctreeCell);
	size += m_triangles.capacity() * sizeof(CTriangle);

	for (unsigned i = 0; i < m_cells.size(); ++i)
	{
		const COctreeCell& cell = m_cells[i];
		size += sizeof(cell.m_aabb);
		size += sizeof(cell.m_childCellIndices);
		size += cell.m_triangleIndices.capacity() * sizeof(int);
	}

	return size;
}
