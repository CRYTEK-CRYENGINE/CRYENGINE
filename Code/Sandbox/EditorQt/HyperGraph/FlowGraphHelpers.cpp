// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FlowGraphHelpers.h"


#include <CryAISystem/IAIAction.h>

#include "FlowGraphManager.h"
#include "FlowGraph.h"
#include "Controls/HyperGraphEditorWnd.h"
#include "Controls/FlowGraphSearchCtrl.h"
#include "Controls/DynamicPopupMenu.h"

#include "Objects/EntityObject.h"

#include <CryEntitySystem/IEntityBasicTypes.h>

#include "Util/BoostPythonHelpers.h"

namespace {
void GetRealName(CHyperFlowGraph* pFG, CString& outName)
{
	CEntityObject* pEntity = pFG->GetEntity();
	IAIAction* pAIAction = pFG->GetAIAction();
	outName = pFG->GetGroupName();
	if (!outName.IsEmpty()) outName += ":";
	if (pEntity)
	{
		outName += pEntity->GetName();
	}
	else if (pAIAction)
	{
		outName += pAIAction->GetName();
		outName += " <AI>";
	}
	else
	{
		outName += pFG->GetName();
	}
}

struct CmpByName
{
	bool operator()(CHyperFlowGraph* lhs, CHyperFlowGraph* rhs) const
	{
		CString lName;
		CString rName;
		GetRealName(lhs, lName);
		GetRealName(rhs, rName);
		return lName.CompareNoCase(rName) < 0;
	}
};
}

//////////////////////////////////////////////////////////////////////////
namespace FlowGraphHelpers
{
void GetHumanName(CHyperFlowGraph* pFlowGraph, CString& outName)
{
	GetRealName(pFlowGraph, outName);
}

void FindGraphsForEntity(CEntityObject* pEntity, std::vector<CHyperFlowGraph*>& outFlowGraphs, CHyperFlowGraph*& outEntityFG)
{
	if (pEntity && pEntity->GetEntityId() != INVALID_ENTITYID)
	{
		typedef std::vector<TFlowNodeId>                                  TNodeIdVec;
		typedef std::map<CHyperFlowGraph*, TNodeIdVec, std::less<CHyperFlowGraph*>> TFGMap;
		TFGMap fgMap;
		CHyperFlowGraph* pEntityGraph = nullptr;

		IEntitySystem* pEntSys = gEnv->pEntitySystem;
		EntityId myId = pEntity->GetEntityId();
		CFlowGraphManager* pFGMgr = GetIEditorImpl()->GetFlowGraphManager();
		size_t numFG = pFGMgr->GetFlowGraphCount();
		for (size_t i = 0; i < numFG; ++i)
		{
			CHyperFlowGraph* pFG = pFGMgr->GetFlowGraph(i);

			// AI Action may not reference actual entities
			if (pFG->GetAIAction() != 0)
				continue;

			IFlowGraphPtr pGameFG = pFG->GetIFlowGraph();
			if (pGameFG == 0)
			{
				CryLogAlways("FlowGraphHelpers::FindGraphsForEntity: No Game FG for FlowGraph 0x%p", pFG);
			}
			else
			{
				if (pGameFG->GetGraphEntity(0) == myId ||
				    pGameFG->GetGraphEntity(1) == myId)
				{
					pEntityGraph = pFG;
					fgMap[pFG].push_back(InvalidFlowNodeId);
					//					CryLogAlways("entity as graph entity: %p\n",pFG);
				}
				IFlowNodeIteratorPtr nodeIter(pGameFG->CreateNodeIterator());
				TFlowNodeId nodeId;
				while (IFlowNodeData* pNodeData = nodeIter->Next(nodeId))
				{
					EntityId id = pGameFG->GetEntityId(nodeId);
					if (myId == id && nodeId != InvalidFlowNodeId)
					{
						//					CryLogAlways("  node entity for id %d: %p\n",nodeId, pEntSys->GetEntity(id));
						fgMap[pFG].push_back(nodeId);
					}
				}
			}
		}

		//		CryLogAlways("found %d unique graphs",fgMap.size());

		typedef std::vector<CHyperFlowGraph*> TFGVec;
		TFGVec fgSortedVec;
		fgSortedVec.reserve(fgMap.size());

		// if there's an entity graph, put it in front
		if (pEntityGraph != NULL)
		{
			fgSortedVec.push_back(pEntityGraph);
		}

		// fill in the rest
		for (TFGMap::const_iterator iter = fgMap.begin(); iter != fgMap.end(); ++iter)
		{
			if ((*iter).first != pEntityGraph)
				fgSortedVec.push_back((*iter).first);
		}

		// sort rest of list by name
		if (fgSortedVec.size() > 1)
		{
			if (pEntityGraph != NULL)
				std::sort(fgSortedVec.begin() + 1, fgSortedVec.end(), CmpByName());
			else
				std::sort(fgSortedVec.begin(), fgSortedVec.end(), CmpByName());
		}
		outFlowGraphs.swap(fgSortedVec);
		outEntityFG = pEntityGraph;
	}
	else
	{
		outFlowGraphs.resize(0);
		outEntityFG = 0;
	}
}

void ListFlowGraphsForEntity(CEntityObject* pEntity)
{
	std::vector<CHyperFlowGraph*> flowgraphs;
	CHyperFlowGraph* entityFG = 0;
	FlowGraphHelpers::FindGraphsForEntity(pEntity, flowgraphs, entityFG);
	if (flowgraphs.size() > 0)
	{
		CDynamicPopupMenu menu;
		CPopupMenuItem& root = menu.GetRoot();

		unsigned int id = 1;
		std::vector<CHyperFlowGraph*>::const_iterator iter(flowgraphs.begin());
		while (iter != flowgraphs.end())
		{
			CString name;
			FlowGraphHelpers::GetHumanName(*iter, name);
			if (*iter == entityFG)
			{
				name += " <GraphEntity>";
			}
			root.AddCommand(name, CommandString("flowgraph.open_view_and_select", (*iter)->GetName(), (const char*)pEntity->GetName()));
			if (*iter == entityFG && flowgraphs.size() > 1)
				root.AddSeparator();
			++id;
			++iter;
		}
		menu.SpawnAtCursor();
	}
}
}

