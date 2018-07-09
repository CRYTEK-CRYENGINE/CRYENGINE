// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PrefabEvents.h"

#include "PrefabItem.h"
#include "Objects/PrefabObject.h"
#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/FlowGraphNode.h"
#include "HyperGraph/FlowGraphVariables.h"
#include <CryGame/IGameFramework.h>
#include "Objects/EntityObject.h"

static TFlowNodeId s_prefabInstanceNodeId = InvalidFlowNodeTypeId;
static TFlowNodeId s_prefabEventSourceNodeId = InvalidFlowNodeTypeId;

//////////////////////////////////////////////////////////////////////////
// CPrefabEvents implementation.
//////////////////////////////////////////////////////////////////////////
CPrefabEvents::SVisibilityInfo::SVisibilityInfo(EPrefabEventType prefabEventType)
	: bInputVisible((prefabEventType == ePrefabEventType_InOut) || (prefabEventType == ePrefabEventType_In))
	, bOutputVisible((prefabEventType == ePrefabEventType_InOut) || (prefabEventType == ePrefabEventType_Out))
{
}

bool CPrefabEvents::SVisibilityInfo::IsVisible(bool bInput)
{
	return bInput ? bInputVisible : bOutputVisible;
}

CPrefabEvents::CPrefabEvents()
	: m_bCurrentlySettingPrefab(false)
{
}

//////////////////////////////////////////////////////////////////////////
CPrefabEvents::~CPrefabEvents()
{
	Deinit();
}

//////////////////////////////////////////////////////////////////////////
void CPrefabEvents::OnHyperGraphManagerEvent(EHyperGraphEvent event, IHyperGraph* pGraph, IHyperNode* pNode)
{
	if (pNode)
	{
		switch (event)
		{
		case EHG_NODE_ADD:
			{
				CHyperNode* pHyperNode = static_cast<CHyperNode*>(pNode);
				if(pHyperNode->IsFlowNode())
				{
					CFlowNode* pFlowNode = static_cast<CFlowNode*>(pNode);
					const TFlowNodeId typeId = pFlowNode->GetTypeId();

					if (typeId == s_prefabEventSourceNodeId)
					{
						UpdatePrefabEventSourceNode(event, pFlowNode);
					}
					else if (typeId == s_prefabInstanceNodeId)
					{
						UpdatePrefabInstanceNode(event, pFlowNode);
					}
				}
			}
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabEvents::OnHyperGraphEvent(IHyperNode* pINode, EHyperGraphEvent event)
{
	if (event == EHG_NODE_PROPERTIES_CHANGED && pINode)   // Only node change are handled here, add and remove are from OnHyperGraphManagerEvent
	{
		CHyperNode* pHyperNode = static_cast<CHyperNode*>(pINode);
		if (pHyperNode->IsFlowNode())
		{
			CFlowNode* pFlowNode = static_cast<CFlowNode*>(pINode);
			TFlowNodeId typeId = pFlowNode->GetTypeId();

			if (pFlowNode->GetTypeId() == s_prefabInstanceNodeId)
			{
				UpdatePrefabInstanceNode(event, pFlowNode);
			}
			else if (pFlowNode->GetTypeId() == s_prefabEventSourceNodeId)
			{
				UpdatePrefabEventSourceNode(event, pFlowNode);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::Init()
{
	CFlowGraphManager* pFlowGraphManager = GetIEditorImpl()->GetFlowGraphManager();
	CRY_ASSERT(pFlowGraphManager != NULL);
	if (pFlowGraphManager)
	{
		pFlowGraphManager->AddListener(this);
		s_prefabInstanceNodeId = gEnv->pFlowSystem->GetTypeId("Prefab:Instance");
		s_prefabEventSourceNodeId = gEnv->pFlowSystem->GetTypeId("Prefab:EventSource");

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabEvents::Deinit()
{
	CFlowGraphManager* pFlowGraphManager = GetIEditorImpl()->GetFlowGraphManager();
	CRY_ASSERT(pFlowGraphManager != NULL);
	if (pFlowGraphManager)
	{
		pFlowGraphManager->RemoveListener(this);
	}

	s_prefabInstanceNodeId = InvalidFlowNodeTypeId;
	s_prefabEventSourceNodeId = InvalidFlowNodeTypeId;

	RemoveAllEventData();
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::AddPrefabInstanceNodeFromSelection(CFlowNode* pNode, CPrefabObject* pPrefabObj)
{
	CRY_ASSERT(pPrefabObj != NULL);
	CRY_ASSERT(pNode != NULL);
	CPrefabItem* pPrefabItem = pPrefabObj->GetPrefabItem();
	CRY_ASSERT(pPrefabItem != NULL);

	const string& prefabName = pPrefabItem->GetFullName();
	const string& prefabInstanceName = pPrefabObj->GetName();

	// Ensure doesn't already exist
	const string* pPrefabName = NULL;
	const string* pPrefabInstanceName = NULL;
	SPrefabEventInstanceEventsData* pInstanceEventsData = NULL;
	const bool bResult = GetEventInstanceNodeData(pNode, &pPrefabName, &pPrefabInstanceName, &pInstanceEventsData);
	CRY_ASSERT(!bResult);

	if (!bResult)
	{
		IHyperGraph* pGraph = pNode->GetGraph();
		CRY_ASSERT(pGraph != NULL);
		if (pGraph)
		{
			pGraph->AddListener(this);   // Need to register with graph for events to receive change events
		}

		TPrefabInstancesData& instancesData = m_prefabEventData[prefabName];                    // Creates new entry if doesn't exist
		SPrefabEventInstanceEventsData& instanceEventsData = instancesData[prefabInstanceName]; // Creates new entry if doesn't exist

		SetPrefabIdentifiersToEventNode(pNode, prefabName, prefabInstanceName);
		UpdatePrefabEventsOnInstance(&instanceEventsData, pNode);
		instanceEventsData.m_eventInstanceNodes.AddNode(pNode);

		return true;
	}
	else
	{
		Warning("CPrefabEvents::AddPrefabInstanceNode: Node already added for prefab: %s", prefabName);
		return false;
	}
}

void CPrefabEvents::UpdatePrefabInstanceNodeFromSelection(CFlowNode* pNode, CPrefabObject *pPrefabObj)
{
	CRY_ASSERT(pNode);

	string newPrefabName("");
	string newPrefabInstanceName = "";
	string curPrefabName = "";
	string curPrefabInstanceName = "";

	CHyperNodePort* pPrefabPort = pNode->FindPort("PrefabName", true);
	pPrefabPort->pVar->Get(curPrefabName);
	CHyperNodePort* pInstancePort = pNode->FindPort("InstanceName", true);
	pInstancePort->pVar->Get(curPrefabInstanceName);


	TPrefabNameToInstancesData::iterator instancesData = m_prefabEventData.find(curPrefabName);
	if (instancesData != m_prefabEventData.end())
	{
		TPrefabInstancesData::iterator instanceData = instancesData->second.find(curPrefabInstanceName);
		if (instanceData != instancesData->second.end())
		{
			SPrefabNodeHelper& instanceEventNodeHelper = instanceData->second.m_eventInstanceNodes;
			RemovePrefabInstanceNode(instanceEventNodeHelper, pNode, true);
			instanceEventNodeHelper.CompleteNodeRemoval();
		}
	}

	if (pPrefabObj)
	{
		CPrefabItem* pPrefabItem = pPrefabObj->GetPrefabItem();
		newPrefabName = pPrefabItem->GetFullName();
		newPrefabInstanceName = pPrefabObj->GetName();
	}

	pPrefabPort->pVar->Set(newPrefabName);
	pInstancePort->pVar->Set(newPrefabInstanceName);
	if (pPrefabObj)
	{
		AddPrefabInstanceNodeFromStoredData(pNode);
	}
	else
	{
		UpdatePrefabEventsOnInstance(nullptr, pNode);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabEvents::SetCurrentlySettingPrefab(const bool bCurrentlySetting)
{
	m_bCurrentlySettingPrefab = bCurrentlySetting;
	if (!bCurrentlySetting)   // Need to check if event source nodes were attempted to be added during cloning (Only after cloning is prefab object + instance name set)
	{
		if (m_delayedAddingEventSourceNodes.size() > 0)
		{
			TPrefabNodeContainer::iterator nodeContainerIter = m_delayedAddingEventSourceNodes.begin();
			const TPrefabNodeContainer::const_iterator nodeContainerIterEnd = m_delayedAddingEventSourceNodes.end();
			while (nodeContainerIter != nodeContainerIterEnd)
			{
				CFlowNode* pNode = *nodeContainerIter;
				CRY_ASSERT(pNode != NULL);

				const bool bResult = AddPrefabEventSourceNode(pNode, SPrefabPortVisibility());
				if (!bResult)
				{
					Warning("CPrefabEvents::SetCurrentlyCloningPrefab: Failed adding cloned event source node");
				}

				++nodeContainerIter;
			}

			m_delayedAddingEventSourceNodes.clear();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabEvents::RemoveAllEventData()
{
	m_prefabEventData.clear();
}

//////////////////////////////////////////////////////////////////////////
void CPrefabEvents::OnFlowNodeRemoval(CFlowNode* pNode)
{
	CRY_ASSERT(pNode != NULL);

	if (pNode->GetTypeId() == s_prefabInstanceNodeId)
	{
		UpdatePrefabInstanceNode(EHG_NODE_DELETE, pNode);
	}
	else if (pNode->GetTypeId() == s_prefabEventSourceNodeId)
	{
		UpdatePrefabEventSourceNode(EHG_NODE_DELETE, pNode);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabEvents::OnPrefabObjectNameChange(CPrefabObject* pPrefabObj, const string& oldName, const string& newName)
{
	CRY_ASSERT(pPrefabObj != NULL);

	CPrefabItem* pPrefabItem = pPrefabObj->GetPrefabItem();
	if (pPrefabItem)
	{
		const string& prefabName = pPrefabItem->GetFullName();
		TPrefabNameToInstancesData::iterator allPrefabDataIter = m_prefabEventData.find(prefabName);
		if (allPrefabDataIter != m_prefabEventData.end())
		{
			TPrefabInstancesData& instancesData = allPrefabDataIter->second;
			TPrefabInstancesData::iterator instanceNameDataIter = instancesData.find(oldName);
			if (instanceNameDataIter != instancesData.end())
			{
				instancesData[newName] = instanceNameDataIter->second; // Copy into new name mapping
				instancesData.erase(instanceNameDataIter);             // Remove data in old mapping
				instanceNameDataIter = instancesData.find(newName);    // Reused iterator to point to data in new name mapping

				// Now go through all the nodes switching names
				SPrefabEventInstanceEventsData& instanceEventsData = instanceNameDataIter->second;

				// First event instance ones
				const TPrefabNodeContainer& eventInstanceNodes = instanceEventsData.m_eventInstanceNodes;
				TPrefabNodeContainer::const_iterator eventInstanceNodesIter = eventInstanceNodes.begin();
				const TPrefabNodeContainer::const_iterator eventInstanceNodesIterEnd = eventInstanceNodes.end();

				for (; eventInstanceNodesIter != eventInstanceNodesIterEnd; ++eventInstanceNodesIter)
				{
					SetPrefabIdentifiersToEventNode(*eventInstanceNodesIter, prefabName, newName);
				}
				instanceEventsData.m_eventInstanceNodes.CompleteNodeRemoval();

				// Now go through all the event source nodes
				TPrefabEventSourceData& eventsSourceData = instanceEventsData.m_eventSourceData;
				TPrefabEventSourceData::iterator eventDataIter = eventsSourceData.begin();
				const TPrefabEventSourceData::const_iterator eventDataIterEnd = eventsSourceData.end();

				for (; eventDataIter != eventDataIterEnd; ++eventDataIter)
				{
					SPrefabEventSourceData& eventData = eventDataIter->second;
					const TPrefabNodeContainer& eventSourceNodes = eventData.m_eventSourceNodes;
					TPrefabNodeContainer::const_iterator eventSourceNodeIter = eventSourceNodes.begin();
					const TPrefabNodeContainer::const_iterator eventSourceNodeIterEnd = eventSourceNodes.end();

					for (; eventSourceNodeIter != eventSourceNodeIterEnd; ++eventSourceNodeIter)
					{
						SetPrefabIdentifiersToEventNode(*eventSourceNodeIter, prefabName, newName);
					}
					eventData.m_eventSourceNodes.CompleteNodeRemoval();
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabEvents::GetPrefabPortVisibility(CFlowNode* pNode, SPrefabPortVisibility& result)
{
	for (CHyperNode::Ports::iterator it = pNode->GetInputs()->begin(); it != pNode->GetInputs()->end(); ++it)
	{
		const char* pName = it->GetName();
		if (strcmp(pName, "PrefabName") == 0)
		{
			result.bPrefabNamePortVisible = it->bVisible;
		}
		else if (strcmp(pName, "InstanceName") == 0)
		{
			result.bInstanceNamePortVisible = it->bVisible;
		}
		else if (strcmp(pName, "EventId") == 0)
		{
			result.bEventIdPortVisible = it->bVisible;
		}
		else if (strcmp(pName, "EventIndex") == 0)
		{
			result.bEventIndexPortVisible = it->bVisible;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabEvents::UpdatePrefabEventSourceNode(const EHyperGraphEvent event, CFlowNode* pNode)
{
	CRY_ASSERT(pNode != NULL);

	const string* pPrefabName = NULL;
	const string* pPrefabInstanceName = NULL;
	const string* pEventName = NULL;
	TPrefabInstancesData* pInstancesData = NULL;
	SPrefabEventInstanceEventsData* pInstanceEventsData = NULL;
	SPrefabEventSourceData* pEventSourceData = NULL;
	bool bResult = GetEventSourceNodeData(
	  pNode,
	  &pPrefabName,
	  &pPrefabInstanceName,
	  &pEventName,
	  &pInstancesData,
	  &pInstanceEventsData,
	  &pEventSourceData);

	switch (event)
	{
	case EHG_NODE_ADD:
		{
			if (!bResult)
			{
				IHyperGraph* pGraph = pNode->GetGraph();
				CRY_ASSERT(pGraph != NULL);
				if (pGraph)
				{
					pGraph->AddListener(this);   // Need to register with graph for events to receive change events
				}

				if (m_bCurrentlySettingPrefab)   // After cloning is completely and prefab object + instance name assigned, these can be processed
				{
					stl::push_back_unique(m_delayedAddingEventSourceNodes, pNode);
					return;
				}

				bResult = AddPrefabEventSourceNode(pNode, SPrefabPortVisibility());
				CRY_ASSERT(bResult);
				if (!bResult)
				{
					Warning("CPrefabEvents::UpdatePrefabEventSourceNode: Failed to add prefab event source node");
				}
			}
		}
		break;

	case EHG_NODE_PROPERTIES_CHANGED:
		{
			if (bResult)
			{
				// Need to check what changed
				string prefabName;
				string prefabInstanceName;
				string eventName;
				EPrefabEventType eventType;

				GetPrefabIdentifiersFromStoredData(pNode, prefabName, prefabInstanceName);
				GetPrefabEventNameFromEventSourceNode(pNode, eventName);
				GetPrefabEventTypeFromEventSourceNode(pNode, eventType);

				if ((prefabName.compare(*pPrefabName) != 0) ||
					(prefabInstanceName.compare(*pPrefabInstanceName) != 0) ||
					(eventName.compare(*pEventName) != 0) ||
					(eventType != pEventSourceData->m_eventType))
				{
					SPrefabPortVisibility portVisibility;
					GetPrefabPortVisibility(pNode, portVisibility);

					SPrefabNodeHelper& eventSourceNodeHelper = pEventSourceData->m_eventSourceNodes;
					RemovePrefabEventSourceNode(eventSourceNodeHelper, pNode, *pInstancesData, *pInstanceEventsData, *pPrefabName, *pEventName, true);

					bResult = AddPrefabEventSourceNode(pNode, portVisibility);
					CRY_ASSERT(bResult);
				}
			}
		}
		break;

	case EHG_NODE_DELETE:
		{
			if (bResult)
			{
				SPrefabNodeHelper& eventSourceNodeHelper = pEventSourceData->m_eventSourceNodes;
				RemovePrefabEventSourceNode(eventSourceNodeHelper, pNode, *pInstancesData, *pInstanceEventsData, *pPrefabName, *pEventName, false);
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabEvents::UpdatePrefabInstanceNode(const EHyperGraphEvent event, CFlowNode* pNode)
{
	CRY_ASSERT(pNode != NULL);

	const string* pPrefabName = NULL;
	const string* pPrefabInstanceName = NULL;
	SPrefabEventInstanceEventsData* pInstanceEventsData = NULL;
	bool bResult = GetEventInstanceNodeData(pNode, &pPrefabName, &pPrefabInstanceName, &pInstanceEventsData);

	switch (event)
	{
	// Add is handled externally from AddPrefabInstanceNode
	case EHG_NODE_ADD:
		{
			if (!bResult)   // Wasn't added from selection ( Serialized or cloned )
			{
				IHyperGraph* pGraph = pNode->GetGraph();
				CRY_ASSERT(pGraph != NULL);
				if (pGraph)
				{
					pGraph->AddListener(this);   // Need to register with graph for events to receive change events
				}

				bResult = AddPrefabInstanceNodeFromStoredData(pNode);
				CRY_ASSERT(bResult);
			}
		}
		break;

	case EHG_NODE_PROPERTIES_CHANGED:
		{
			// Remove and re-add from node data
			if (bResult)
			{
				CRY_ASSERT(pInstanceEventsData);
				SPrefabNodeHelper& instanceEventNodeHelper = pInstanceEventsData->m_eventInstanceNodes;
				RemovePrefabInstanceNode(instanceEventNodeHelper, pNode, true);
				instanceEventNodeHelper.CompleteNodeRemoval();
			}
			AddPrefabInstanceNodeFromStoredData( pNode );
		}
		break;

	case EHG_NODE_DELETE:
		{
			if (bResult)
			{
				CRY_ASSERT(pInstanceEventsData);
				SPrefabNodeHelper& instanceNodeHelper = pInstanceEventsData->m_eventInstanceNodes;
				RemovePrefabInstanceNode(instanceNodeHelper, pNode, false);
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::AddPrefabInstanceNodeFromStoredData(CFlowNode* pNode)
{
	string prefabName;
	string prefabInstanceName;
	bool bResult = GetPrefabIdentifiersFromStoredData(pNode, prefabName, prefabInstanceName);
	if (bResult)
	{
		TPrefabInstancesData& instancesData = m_prefabEventData[prefabName];                    // Creates new entry if doesn't exist
		SPrefabEventInstanceEventsData& instanceEventsData = instancesData[prefabInstanceName]; // Creates new entry if doesn't exist
		instanceEventsData.m_eventInstanceNodes.AddNode(pNode);

		// Updates node ports readability
		SetPrefabIdentifiersToEventNode(pNode, prefabName, prefabInstanceName);

		// Flownode event eFE_Initialize causes custom events to be registered. Currently no way to only do that
		// only when gEnv->bEditorGame == true as when entering editor, bEditorGame = true assignment happens later than it should
		// For now clear events to be safe won't be leaving a dangling pointer
		// Needs to be inside here which is called when adding a node since initialize is called from flownode serialization
		UnregisterPrefabInstanceEvents(pNode);

		UpdatePrefabEventsOnInstance(&instanceEventsData, pNode);
		// It's possible during serializing that not all source events have been added at this point and instance node isn't complete
		// That's ok since source node contains event index and updates the instances when they're added

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::AddPrefabEventSourceNode(CFlowNode* pNode, const SPrefabPortVisibility& portVisibility)
{
	CRY_ASSERT(pNode != NULL);

	// Need to check what changed
	string prefabName;
	string prefabInstanceName;
	string eventName;

	bool bResult = GetPrefabIdentifiersFromObject(pNode, prefabName, prefabInstanceName);
	CRY_ASSERT(bResult);
	if (bResult)
	{
		SetPrefabIdentifiersToEventNode(pNode, prefabName, prefabInstanceName);
		SetPrefabEventNodePortVisibility(pNode, "PrefabName", portVisibility.bPrefabNamePortVisible, true);
		SetPrefabEventNodePortVisibility(pNode, "InstanceName", portVisibility.bInstanceNamePortVisible, true);
	}
	else
	{
		Warning("CPrefabEvents::AddPrefabEventSourceNode: Failed to find prefab data, node should only be created inside a prefab");
		return false;
	}

	GetPrefabEventNameFromEventSourceNode(pNode, eventName);
	TPrefabInstancesData& instancesData = m_prefabEventData[prefabName];                    // Creates new entry if doesn't exist
	SPrefabEventInstanceEventsData& instanceEventsData = instancesData[prefabInstanceName]; // Creates new entry if doesn't exist
	SPrefabEventSourceData& eventData = instanceEventsData.m_eventSourceData[eventName];    // Creates new entry if doesn't exist

	eventData.m_eventSourceNodes.AddNode(pNode);   // Keep track of node regardless if has an empty event or not

	TCustomEventId& eventId = eventData.m_eventId;
	int iEventIndex = -1;
	// First attempt to get saved event index
	GetPrefabEventIndexFromEventSourceNode(pNode, iEventIndex);
	GetPrefabEventTypeFromEventSourceNode(pNode, eventData.m_eventType);

	SVisibilityInfo eventVisibilityInfo(eventData.m_eventType);

	if (eventId == CUSTOMEVENTID_INVALID)   // Try to create event id
	{
		if (!eventName.IsEmpty())   // Has an event name so assign an id
		{
			// Determine port position on instance
			if (iEventIndex == -1)   // Didn't exist so need to attempt to get a free one
			{
				SPrefabEventSourceData* pFoundSourceData = NULL;
				const string* pFoundEventName = NULL;
				for (int i = 1; i <= CUSTOMEVENTS_PREFABS_MAXNPERINSTANCE; ++i)
				{
					bResult = GetPrefabEventSourceData(instanceEventsData.m_eventSourceData, i, &pFoundSourceData, &pFoundEventName);
					if (!bResult)
					{
						iEventIndex = i;
						break;
					}
				}
			}

			if (iEventIndex != -1)
			{
				ICustomEventManager* pCustomEventManager = gEnv->pGameFramework->GetICustomEventManager();
				CRY_ASSERT(pCustomEventManager != NULL);
				eventData.m_eventId = pCustomEventManager->GetNextCustomEventId();
				eventData.m_iEventIndex = iEventIndex;

				// Need to update all instances
				UpdatePrefabInstanceNodeEvents(instancesData);
			}
			else // Unable to find a port position for instances, must have exceeded amount of unique events
			{
				Warning("CPrefabEvents::AddPrefabEventSourceNode: Unable to add unique event, at max limit of: %d", CUSTOMEVENTS_PREFABS_MAXNPERINSTANCE);
			}
		}
	}

	// Flownode event eFE_Initialize causes custom events to be registered. Currently no way to only do that
	// only when gEnv->bEditorGame == true as when entering editor, bEditorGame = true assignment happens later than it should
	// For now clear events to be safe won't be leaving a dangling pointer
	// Needs to be inside here which is called when adding a node since initialize is called from flownode serialization
	UnregisterPrefabEventSourceEvent(pNode);
	SetPrefabEventIdToEventSourceNode(pNode, eventId);

	SetPrefabEventIndexToEventSourceNode(pNode, iEventIndex);

	SetPrefabEventNodePortVisibility(pNode, "EventId", portVisibility.bEventIdPortVisible, true);
	SetPrefabEventNodePortVisibility(pNode, "EventIndex", portVisibility.bEventIndexPortVisible, true);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::RemovePrefabEventSourceNode(
  SPrefabNodeHelper& eventSourceNodeHelper,
  CFlowNode* pNode,
  TPrefabInstancesData& instancesData,
  SPrefabEventInstanceEventsData& instanceEventsData,
  const string& prefabName,
  const string& eventName,
  bool bRemoveDataOnNode)
{
	CRY_ASSERT(pNode != NULL);

	bool bNodeRemoved;

	if (bRemoveDataOnNode) // Won't do this when node is being destroyed
	{
		SetPrefabEventIndexToEventSourceNode(pNode, -1);   // Want to redetermine event index

		// Flownode event eFE_Initialize causes custom events to be registered. Currently no way to only do that
		// only when gEnv->bEditorGame == true as when entering editor, bEditorGame = true assignment happens later than it should
		// For now clear events to be safe won't be leaving a dangling pointer
		UnregisterPrefabEventSourceEvent(pNode);

		bNodeRemoved = eventSourceNodeHelper.InitiateNodeRemoval(pNode);
	}
	else
	{
		bNodeRemoved = eventSourceNodeHelper.RemoveNode(pNode);
	}

	if (bNodeRemoved)
	{
		// If just removed last event source, need to remove event and update all instances
		if (eventSourceNodeHelper.IsEmpty())
		{
			TPrefabEventSourceData& eventSourcesData = instanceEventsData.m_eventSourceData;
			TPrefabEventSourceData::iterator eventSourcesDataIter = eventSourcesData.find(eventName);
			CRY_ASSERT(eventSourcesDataIter != eventSourcesData.end());
			if (eventSourcesDataIter != eventSourcesData.end())
			{
				eventSourcesData.erase(eventSourcesDataIter);

				if (!instanceEventsData.m_eventInstanceNodes.GetContainer().empty())
				{
					// Need to update all instances
					UpdatePrefabInstanceNodeEvents(instancesData);
				}
			}
		}
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabEvents::UnregisterPrefabEventSourceEvent(CFlowNode* pNode)
{
	TCustomEventId curEventId = CUSTOMEVENTID_INVALID;
	GetPrefabEventIdFromEventSourceNode(pNode, curEventId);
	if (curEventId != CUSTOMEVENTID_INVALID)
	{
		SetPrefabEventIdToEventSourceNode(pNode, -1);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::RemovePrefabInstanceNode(SPrefabNodeHelper& instanceNodeHelper, CFlowNode* pNode, bool bRemoveDataOnNode)
{
	CRY_ASSERT(pNode != NULL);

	if (bRemoveDataOnNode)  // Won't do this when node is being destroyed
	{
		if (instanceNodeHelper.InitiateNodeRemoval(pNode))
		{
			// Flownode event eFE_Initialize causes custom events to be registered. Currently no way to only do that
			// only when gEnv->bEditorGame == true as when entering editor, bEditorGame = true assignment happens later than it should
			// For now clear events to be safe won't be leaving a dangling pointer
			UnregisterPrefabInstanceEvents(pNode);

			return true;
		}
	}
	else
	{
		if (instanceNodeHelper.RemoveNode(pNode))
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabEvents::UnregisterPrefabInstanceEvents(CFlowNode* pNode)
{
	// Flownode event eFE_Initialize causes custom events to be registered. Currently no way to only do that
	// only when gEnv->bEditorGame == true as when entering editor, bEditorGame = true assignment happens later than it should
	// For now clear events to be safe won't be leaving a dangling pointer
	string eventName;

	for (int i = 1; i <= CUSTOMEVENTS_PREFABS_MAXNPERINSTANCE; ++i)
	{
		eventName.Format("Event%d", i);

		CHyperNodePort* pEventTriggerInputPort = pNode->FindPort(eventName, true);
		CHyperNodePort* pEventTriggerOutputPort = pNode->FindPort(eventName, false);
		CHyperNodePort* pEventNamePort = pNode->FindPort(eventName + "Name", true);
		CHyperNodePort* pEventIdPort = pNode->FindPort(eventName + "Id", true);

		CRY_ASSERT(pEventTriggerInputPort && pEventTriggerOutputPort && pEventNamePort && pEventIdPort);
		if (pEventTriggerInputPort && pEventTriggerOutputPort && pEventNamePort && pEventIdPort)
		{
			int iCustomEventId = (int)CUSTOMEVENTID_INVALID;
			pEventIdPort->pVar->Get(iCustomEventId);

			if (iCustomEventId != (int)CUSTOMEVENTID_INVALID)
			{
				// Get rid of the event id set on node itself in case this node is going to be reused from a change
				pEventIdPort->pVar->Set((int)CUSTOMEVENTID_INVALID);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::GetEventInstanceNodeData(CFlowNode* pNode, const string** ppPrefabName, const string** ppPrefabInstanceName, SPrefabEventInstanceEventsData** ppInstanceEventsData)
{
	CRY_ASSERT(pNode != NULL);
	CRY_ASSERT(ppPrefabName != NULL);
	CRY_ASSERT(ppPrefabInstanceName != NULL);
	CRY_ASSERT(ppInstanceEventsData != NULL);

	TPrefabNameToInstancesData::iterator allDataIter = m_prefabEventData.begin();
	const TPrefabNameToInstancesData::const_iterator allDataIterEnd = m_prefabEventData.end();
	while (allDataIter != allDataIterEnd)
	{
		TPrefabInstancesData& instancesData = allDataIter->second;

		// Search assigned events + containers first
		TPrefabInstancesData::iterator instancesDataIter = instancesData.begin();
		const TPrefabInstancesData::const_iterator instancesDataIterEnd = instancesData.end();
		while (instancesDataIter != instancesDataIterEnd)
		{
			SPrefabEventInstanceEventsData& instanceData = instancesDataIter->second;
			if (instanceData.m_eventInstanceNodes.HasNode(pNode))
			{
				*ppPrefabName = &allDataIter->first;
				*ppPrefabInstanceName = &instancesDataIter->first;
				*ppInstanceEventsData = &instanceData;
				return true;
			}

			++instancesDataIter;
		}

		++allDataIter;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::GetEventSourceNodeData(
  CFlowNode* pNode,
  const string** ppPrefabName,
  const string** ppPrefabInstanceName,
  const string** ppEventName,
  TPrefabInstancesData** ppInstancesData,
  SPrefabEventInstanceEventsData** ppInstanceEventsData,
  SPrefabEventSourceData** ppEventSourceData)
{
	CRY_ASSERT(pNode != NULL);
	CRY_ASSERT(ppPrefabName != NULL);
	CRY_ASSERT(ppPrefabInstanceName != NULL);
	CRY_ASSERT(ppEventName != NULL);
	CRY_ASSERT(ppInstancesData != NULL);
	CRY_ASSERT(ppInstanceEventsData != NULL);
	CRY_ASSERT(ppEventSourceData != NULL);

	TPrefabNameToInstancesData::iterator allDataIter = m_prefabEventData.begin();
	const TPrefabNameToInstancesData::const_iterator allDataIterEnd = m_prefabEventData.end();
	while (allDataIter != allDataIterEnd)
	{
		TPrefabInstancesData& instancesData = allDataIter->second;

		// Search assigned events + containers first
		TPrefabInstancesData::iterator instancesDataIter = instancesData.begin();
		const TPrefabInstancesData::const_iterator instancesDataIterEnd = instancesData.end();
		while (instancesDataIter != instancesDataIterEnd)
		{
			SPrefabEventInstanceEventsData& instanceData = instancesDataIter->second;

			TPrefabEventSourceData::iterator sourceEventDataIter = instanceData.m_eventSourceData.begin();
			const TPrefabEventSourceData::const_iterator sourceEventDataIterEnd = instanceData.m_eventSourceData.end();

			while (sourceEventDataIter != sourceEventDataIterEnd)
			{
				SPrefabEventSourceData& sourceData = sourceEventDataIter->second;
				if (sourceData.m_eventSourceNodes.HasNode(pNode))
				{
					*ppInstancesData = &instancesData;
					*ppPrefabName = &allDataIter->first;
					*ppPrefabInstanceName = &instancesDataIter->first;
					*ppInstanceEventsData = &instanceData;
					*ppEventName = &sourceEventDataIter->first;
					*ppEventSourceData = &sourceData;
					return true;
				}

				++sourceEventDataIter;
			}

			++instancesDataIter;
		}

		++allDataIter;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::GetPrefabIdentifiersFromStoredData(CFlowNode* pNode, string& prefabName, string& prefabInstanceName)
{
	CRY_ASSERT(pNode != NULL);
	CHyperNodePort* pPort = pNode->FindPort("PrefabName", true);
	CRY_ASSERT(pPort != NULL);
	if (pPort)
	{
		pPort->pVar->Get(prefabName);

		if (prefabName.IsEmpty())
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	pPort = pNode->FindPort("InstanceName", true);
	CRY_ASSERT(pPort != NULL);
	if (pPort)
	{
		pPort->pVar->Get(prefabInstanceName);

		if (prefabInstanceName.IsEmpty())
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::GetPrefabIdentifiersFromObject(CFlowNode* pNode, string& prefabName, string& prefabInstanceName)
{
	CRY_ASSERT(pNode != NULL);

	// Determine prefab object from graph holding the node
	IFlowGraph* pGraph = pNode->GetIFlowGraph();
	CRY_ASSERT(pGraph != NULL);
	if (pGraph)
	{
		const EntityId graphEntityId = pGraph->GetGraphEntity(0);
		CRY_ASSERT(graphEntityId != 0);
		if (graphEntityId != 0)
		{
			// Get editor object that represents the entity
			CEntityObject* pEntityObject = CEntityObject::FindFromEntityId(graphEntityId);
			CRY_ASSERT(pEntityObject != NULL);
			if (pEntityObject != NULL)
			{
				if (CPrefabObject* pFoundPrefabObject = (CPrefabObject*)pEntityObject->GetPrefab())
				{
					CPrefabItem* pPrefabItem = pFoundPrefabObject->GetPrefabItem();
					CRY_ASSERT(pPrefabItem != NULL);
					if (pPrefabItem)
					{
						prefabName = pPrefabItem->GetFullName();
						prefabInstanceName = pFoundPrefabObject->GetName();
						return true;
					}
				}
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::SetPrefabIdentifiersToEventNode(CFlowNode* pNode, const string& prefabName, const string& prefabInstanceName)
{
	if (pNode)
	{
		CHyperNodePort* pPrefabNamePort = pNode->FindPort("PrefabName", true);
		CHyperNodePort* pPrefabInstancePort = pNode->FindPort("InstanceName", true);
		CRY_ASSERT(pPrefabNamePort && pPrefabInstancePort);
		if (pPrefabNamePort && pPrefabInstancePort)
		{
			pPrefabNamePort->pVar->Set(prefabName);
			pPrefabNamePort->pVar->SetHumanName("PREFAB");
			pPrefabInstancePort->pVar->Set(prefabInstanceName);
			pPrefabInstancePort->pVar->SetHumanName("INSTANCE");
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::SetPrefabEventIdToEventSourceNode(CFlowNode* pNode, const TCustomEventId eventId)
{
	CRY_ASSERT(pNode != NULL);
	CHyperNodePort* pPort = pNode->FindPort("EventId", true);
	CRY_ASSERT(pPort != NULL);
	if (pPort)
	{
		pPort->pVar->Set((int)eventId);
		pPort->bVisible = false;
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::SetPrefabEventIndexToEventSourceNode(CFlowNode* pNode, const int iEventIndex)
{
	CRY_ASSERT(pNode != NULL);
	CHyperNodePort* pPort = pNode->FindPort("EventIndex", true);
	CRY_ASSERT(pPort != NULL);
	if (pPort)
	{
		pPort->pVar->Set(iEventIndex);
		pPort->bVisible = false;
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::GetPrefabEventIndexFromEventSourceNode(CFlowNode* pNode, int& iEventIndex)
{
	CRY_ASSERT(pNode != NULL);
	CHyperNodePort* pPort = pNode->FindPort("EventIndex", true);
	CRY_ASSERT(pPort != NULL);
	if (pPort)
	{
		pPort->pVar->Get(iEventIndex);
		return true;
	}

	iEventIndex = -1;
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::GetPrefabEventTypeFromEventSourceNode(CFlowNode* pNode, EPrefabEventType& eventType)
{
	CRY_ASSERT(pNode != NULL);
	CHyperNodePort* pPort = pNode->FindPort("EventType", true);
	CRY_ASSERT(pPort != NULL);
	if (pPort)
	{
		int eventTypeAsInt = 0;
		pPort->pVar->Get(eventTypeAsInt);
		eventType = static_cast<EPrefabEventType>(eventTypeAsInt);
		return true;
	}
	eventType = ePrefabEventType_InOut;

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::GetPrefabEventNameFromEventSourceNode(CFlowNode* pNode, string& eventName)
{
	CRY_ASSERT(pNode != NULL);
	CHyperNodePort* pPort = pNode->FindPort("EventName", true);
	CRY_ASSERT(pPort != NULL);
	if (pPort)
	{
		pPort->pVar->Get(eventName);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::GetPrefabEventIdFromEventSourceNode(CFlowNode* pNode, TCustomEventId& eventId)
{
	CRY_ASSERT(pNode != NULL);
	CHyperNodePort* pPort = pNode->FindPort("EventId", true);
	CRY_ASSERT(pPort != NULL);
	if (pPort)
	{
		int iEventIdVal;
		pPort->pVar->Get(iEventIdVal);
		eventId = (TCustomEventId)iEventIdVal;
		return true;
	}

	eventId = CUSTOMEVENTID_INVALID;
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabEvents::UpdatePrefabInstanceNodeEvents(TPrefabInstancesData& instancesData)
{
	TPrefabInstancesData::iterator instancesDataIter = instancesData.begin();
	const TPrefabInstancesData::const_iterator instancesDataIterEnd = instancesData.end();

	for (; instancesDataIter != instancesDataIterEnd; ++instancesDataIter)
	{
		SPrefabEventInstanceEventsData& instanceEventsData = instancesDataIter->second;
		const TPrefabNodeContainer& instancesNodeContainer = instanceEventsData.m_eventInstanceNodes;
		TPrefabNodeContainer::const_iterator instanceNodeContainerIter = instancesNodeContainer.begin();
		const TPrefabNodeContainer::const_iterator instanceNodeContainerIterEnd = instancesNodeContainer.end();

		for (; instanceNodeContainerIter != instanceNodeContainerIterEnd; ++instanceNodeContainerIter)
		{
			UpdatePrefabEventsOnInstance(&instanceEventsData, *instanceNodeContainerIter);
		}
		instanceEventsData.m_eventInstanceNodes.CompleteNodeRemoval();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabEvents::UpdatePrefabEventsOnInstance(SPrefabEventInstanceEventsData* instanceEventsData, CFlowNode* pInstanceNode)
{
	if (pInstanceNode)
	{
		string eventName;

		// Go through all the events, setting and showing if it exists, or hide the ports
		for (int i = 1; i <= CUSTOMEVENTS_PREFABS_MAXNPERINSTANCE; ++i)
		{
			eventName.Format("Event%d", i);

			CHyperNodePort* pEventTriggerInputPort = pInstanceNode->FindPort(eventName, true);
			CHyperNodePort* pEventTriggerOutputPort = pInstanceNode->FindPort(eventName, false);
			CHyperNodePort* pEventNamePort = pInstanceNode->FindPort(eventName + "Name", true);
			CHyperNodePort* pEventIdPort = pInstanceNode->FindPort(eventName + "Id", true);

			CRY_ASSERT(pEventTriggerInputPort && pEventTriggerOutputPort && pEventNamePort && pEventIdPort);
			if (pEventTriggerInputPort && pEventTriggerOutputPort && pEventNamePort && pEventIdPort)
			{
				SPrefabEventSourceData* pEventSourceData = nullptr;

				bool bVisibility = false;
				bool bResult = false;

				if (instanceEventsData)
				{
					const string* pEventName = nullptr;
					bResult = GetPrefabEventSourceData(instanceEventsData->m_eventSourceData, i, &pEventSourceData, &pEventName);
					if (bResult)
					{
						bVisibility = true;
						pEventNamePort->pVar->Set(*pEventName);
						pEventIdPort->pVar->Set((int)pEventSourceData->m_eventId);

						// Set human port names so event name is visible instead of event#
						pEventTriggerInputPort->pVar->SetHumanName(*pEventName);
						pEventTriggerOutputPort->pVar->SetHumanName(*pEventName);
					}
				}
				
				if (!instanceEventsData || !bResult)
				{
					pEventNamePort->pVar->Set("");
					pEventIdPort->pVar->Set((int)CUSTOMEVENTID_INVALID);
					pEventTriggerInputPort->pVar->SetHumanName("InvalidEvent");
					pEventTriggerOutputPort->pVar->SetHumanName("InvalidEvent");
				}

				SVisibilityInfo visibilityInfo(pEventSourceData ? pEventSourceData->m_eventType : ePrefabEventType_InOut);

				pEventTriggerInputPort->bVisible  = bVisibility && visibilityInfo.bInputVisible;
				pEventTriggerOutputPort->bVisible = bVisibility && visibilityInfo.bOutputVisible;
				pEventNamePort->bVisible = false;
				pEventIdPort->bVisible = false;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::GetPrefabEventSourceData(TPrefabEventSourceData& eventsSourceData, const int iEventIndex, SPrefabEventSourceData** ppSourceData, const string** ppEventName)
{
	TPrefabEventSourceData::iterator iter = eventsSourceData.begin();
	const TPrefabEventSourceData::const_iterator iterEnd = eventsSourceData.end();
	while (iter != iterEnd)
	{
		SPrefabEventSourceData& sourceData = iter->second;
		if (sourceData.m_iEventIndex == iEventIndex)
		{
			*ppSourceData = &sourceData;
			*ppEventName = &iter->first;
			return true;
		}

		++iter;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabEvents::SetPrefabEventNodePortVisibility(CFlowNode* pNode, const string& portName, const bool bVisible, const bool bInputPort)
{
	CRY_ASSERT(pNode != NULL);
	CHyperNodePort* pPort = pNode->FindPort(portName, bInputPort);
	CRY_ASSERT(pPort != NULL);
	if (pPort)
	{
		pPort->bVisible = bVisible;
		return true;
	}

	return false;
}

bool CPrefabEvents::SPrefabNodeHelper::AddNode(CFlowNode* pFlowNode)
{
	if (pFlowNode != nullptr)
	{
		if (!HasNode(pFlowNode))
		{
			TContainerPos::iterator itPos = m_nodePosToRemove.begin();

			if (itPos != m_nodePosToRemove.end())
			{
				m_nodes[*itPos] = pFlowNode;
				m_nodePosToRemove.erase(itPos);
				return true;
			}
			else
			{
				m_nodes.push_back(pFlowNode);
				return true;
			}
		}
	}
	return false;
}

bool CPrefabEvents::SPrefabNodeHelper::RemoveNode(CFlowNode* pFlowNode)
{
	if (pFlowNode != nullptr)
	{
		CompleteNodeRemoval();

		TPrefabNodeContainer::iterator itNode;
		if (FindNode(pFlowNode, itNode))
		{
			m_nodes.erase(itNode);
			return true;
		}
	}
	return false;
}

bool CPrefabEvents::SPrefabNodeHelper::HasNode(CFlowNode* pFlowNode)
{
	TPrefabNodeContainer::iterator itNode;
	itNode = std::find(m_nodes.begin(), m_nodes.end(), pFlowNode);

	if (itNode != m_nodes.end())
	{
		return true;
	}
	return false;
}

bool CPrefabEvents::SPrefabNodeHelper::FindNode(CFlowNode* pFlowNode, TPrefabNodeContainer::iterator& itNode)
{
	itNode = std::find(m_nodes.begin(), m_nodes.end(), pFlowNode);

	if (itNode != m_nodes.end())
	{
		return true;
	}
	return false;
}

bool CPrefabEvents::SPrefabNodeHelper::InitiateNodeRemoval(CFlowNode* pFlowNode)
{
	if (pFlowNode != nullptr)
	{
		TPrefabNodeContainer::iterator itNode;

		if (FindNode(pFlowNode, itNode))
		{
			*itNode = nullptr;
			const TPosType pos = static_cast<TPosType>(std::distance(m_nodes.begin(), itNode));
			m_nodePosToRemove.push_back(pos);
			return true;
		}
	}
	return false;
}

void CPrefabEvents::SPrefabNodeHelper::CompleteNodeRemoval()
{
	if (!m_nodePosToRemove.empty())
	{
		auto sortDescending = [](TPosType i, TPosType j) { return i > j; };
		std::sort(m_nodePosToRemove.begin(), m_nodePosToRemove.end(), sortDescending);

		for (TPosType i = 0, count = m_nodePosToRemove.size(); i < count; ++i)
		{
			const TPosType pos = m_nodePosToRemove[i];
			CRY_ASSERT(pos < m_nodes.size());
			m_nodes.erase(m_nodes.begin() + pos);
		}
		m_nodePosToRemove.clear();
	}
}

