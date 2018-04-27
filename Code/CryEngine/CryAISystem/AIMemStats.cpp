// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryMemory/CrySizer.h>
#include "CAISystem.h"
#include "Puppet.h"
#include "AIVehicle.h"
#include "AIPlayer.h"
#include "GoalPipe.h"
#include "GoalOp.h"
#include "WorldOctree.h"
#include "ObjectContainer.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "Formation/FormationManager.h"

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

	if (gAIEnv.pFormationManager)
	{
		gAIEnv.pFormationManager->GetMemoryStatistics(pSizer);
	}

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
