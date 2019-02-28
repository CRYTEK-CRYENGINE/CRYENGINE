// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PrefabEvents_h__
#define __PrefabEvents_h__
#pragma once

#include <HyperGraph/IHyperGraph.h>
#include <CryAction/ICustomEvents.h>

class CPrefabObject;
class CFlowNode;

/** Manages prefab events.
 */
class CPrefabEvents : public IHyperGraphManagerListener, public IHyperGraphListener
{
public:
	CPrefabEvents();
	~CPrefabEvents();

	// IHyperGraphManagerListener
	virtual void OnHyperGraphManagerEvent(EHyperGraphEvent event, IHyperGraph* pGraph, IHyperNode* pNode);
	// ~IHyperGraphManagerListener

	// IHyperGraphListener
	virtual void OnHyperGraphEvent(IHyperNode* pNode, EHyperGraphEvent event);
	// ~IHyperGraphListener

	// Initialization code to handle prefab events
	bool Init();

	// Deinitialization code for prefab events
	void Deinit();

	// Adds prefab instance node when selecting a prefab and clicking add selected
	bool AddPrefabInstanceNodeFromSelection(CFlowNode* pNode, CPrefabObject* pPrefabObj);

	// Updates a prefab instance node to refer to another prefab instance. A null object is valid to clear the assignment
	void UpdatePrefabInstanceNodeFromSelection(CFlowNode* pNode, CPrefabObject *pPrefabObj);

	// Sets currently setting prefab flag (When cloning, serializing, updating prefabs), used to delay adding of prefab event source nodes since prefab name + instance can't be determined yet
	void SetCurrentlySettingPrefab(const bool bCurrentlySetting);

	// Removes all data associated to prefab events
	void RemoveAllEventData();

	// Handle flow node removal
	void OnFlowNodeRemoval(CFlowNode* pNode);

	// Handle prefab object name change
	void OnPrefabObjectNameChange(CPrefabObject* pPrefabObj, const string& oldName, const string& newName);

protected:
	// Container to hold prefab instance or event source nodes
	typedef std::vector<CFlowNode*> TPrefabNodeContainer;

	struct SPrefabNodeHelper
	{
		bool                               AddNode(CFlowNode* pFlowNode);
		bool                               RemoveNode(CFlowNode* pFlowNode);
		bool                               HasNode(CFlowNode* pFlowNode);
		bool                               InitiateNodeRemoval(CFlowNode* pFlowNode);
		void                               CompleteNodeRemoval();

		inline bool                        IsEmpty()                                    { return m_nodes.size() == m_nodePosToRemove.size(); }
		inline const TPrefabNodeContainer& GetContainer() const                         { return m_nodes; }
		inline                             operator const TPrefabNodeContainer&() const { return m_nodes; }

	private:
		bool FindNode(CFlowNode* pFlowNode, TPrefabNodeContainer::iterator& itNode);

		typedef size_t                TPosType;
		typedef std::vector<TPosType> TContainerPos;
		TPrefabNodeContainer m_nodes;
		TContainerPos        m_nodePosToRemove;
	};

	enum EPrefabEventType
	{
		ePrefabEventType_InOut = 0,
		ePrefabEventType_In,
		ePrefabEventType_Out,
	};

	struct SVisibilityInfo
	{
		SVisibilityInfo(EPrefabEventType prefabEventType);
		bool IsVisible(bool bInput);
		bool bInputVisible;
		bool bOutputVisible;
	};

	// Prefab event data associated with a single source event
	struct SPrefabEventSourceData
	{
		SPrefabEventSourceData()
			: m_iEventIndex(-1)
			, m_eventId(CUSTOMEVENTID_INVALID)
		{
		}

		int               m_iEventIndex; // To remember port order on instances
		TCustomEventId    m_eventId;     // Assigned event id for custom event
		EPrefabEventType  m_eventType;   // specifies whether this event is an Input, Output or both, from the Prefab's perspective
		SPrefabNodeHelper m_eventSourceNodes;
	};

	// Event name to event source data inside an instance
	typedef std::map<string, SPrefabEventSourceData> TPrefabEventSourceData;

	// Prefab data associated to an instance
	struct SPrefabEventInstanceEventsData
	{
		TPrefabEventSourceData m_eventSourceData;
		SPrefabNodeHelper      m_eventInstanceNodes;
	};

	// Instance name to instance data
	typedef std::map<string, SPrefabEventInstanceEventsData> TPrefabInstancesData;

	// Utility to collect visibility flags
	struct SPrefabPortVisibility
	{
		bool bEventIdPortVisible;
		bool bEventIndexPortVisible;
		bool bPrefabNamePortVisible;
		bool bInstanceNamePortVisible;

		SPrefabPortVisibility() : bEventIdPortVisible(false), bEventIndexPortVisible(false), bPrefabNamePortVisible(false), bInstanceNamePortVisible(false) {}
	};

	// Retrieve the current port visibility information of a prefab node
	// This information can be used to restore visibility information when re-adding
	void GetPrefabPortVisibility(CFlowNode* pNode, SPrefabPortVisibility& result);

	// Handles add, change, removing of prefab event source nodes
	void UpdatePrefabEventSourceNode(const EHyperGraphEvent event, CFlowNode* pNode);

	// Handles add, change, removing of prefab event instance nodes
	void UpdatePrefabInstanceNode(const EHyperGraphEvent event, CFlowNode* pNode);

	// Adds prefab instance node from stored data (After initial creation from selection was done)
	bool AddPrefabInstanceNodeFromStoredData(CFlowNode* pNode);

	// Adds prefab source node
	bool AddPrefabEventSourceNode(CFlowNode* pNode, const SPrefabPortVisibility& portVisibility);

	// Removes prefab event source node
	bool RemovePrefabEventSourceNode(SPrefabNodeHelper& eventSourceNodeHelper, CFlowNode* pNode, TPrefabInstancesData& instancesData, SPrefabEventInstanceEventsData& instanceEventsData, const string& prefabName, const string& eventName, bool bRemoveDataOnNode);

	// Unregisters custom event associated to the prefab event source node
	void UnregisterPrefabEventSourceEvent(CFlowNode* pNode);

	// Removes prefab instance node
	bool RemovePrefabInstanceNode(SPrefabNodeHelper& instanceNodeHelper, CFlowNode* pNode, bool bRemoveDataOnNode);

	// Unregisters custom events associated to the prefab event instance node
	void UnregisterPrefabInstanceEvents(CFlowNode* pNode);

	// Gets event instance node data if it exists
	bool GetEventInstanceNodeData(CFlowNode* pNode, const string** ppPrefabName, const string** ppPrefabInstanceName, SPrefabEventInstanceEventsData** ppInstanceEventsData);

	// Gets event source node data if it exists
	bool GetEventSourceNodeData(
	  CFlowNode* pNode,
	  const string** ppPrefabName,
	  const string** ppPrefabInstanceName,
	  const string** ppEventName,
	  TPrefabInstancesData** ppInstancesData,
	  SPrefabEventInstanceEventsData** ppInstanceEventsData,
	  SPrefabEventSourceData** ppEventSourceData);

	// Gets prefab identifiers from stored data
	bool GetPrefabIdentifiersFromStoredData(CFlowNode* pNode, string& prefabName, string& prefabInstanceName);

	// Attempts to get prefab identifiers from owning object
	bool GetPrefabIdentifiersFromObject(CFlowNode* pNode, string& prefabName, string& prefabInstanceName);

	// Sets prefab identifiers to stored data
	bool SetPrefabIdentifiersToEventNode(CFlowNode* pNode, const string& prefabName, const string& prefabInstanceName);

	// Retrieves associated event name
	bool GetPrefabEventNameFromEventSourceNode(CFlowNode* pNode, string& eventName);

	// Assigns custom event id to event source node
	bool SetPrefabEventIdToEventSourceNode(CFlowNode* pNode, const TCustomEventId eventId);

	// Retrieves custom event id from event source node
	bool GetPrefabEventIdFromEventSourceNode(CFlowNode* pNode, TCustomEventId& eventId);

	// Assigns custom event index (Used to preserve port order on instances) to event source node
	bool SetPrefabEventIndexToEventSourceNode(CFlowNode* pNode, const int iEventIndex);

	// Retrieves custom event index (Used to preserve port order on instances) from event source node
	bool GetPrefabEventIndexFromEventSourceNode(CFlowNode* pNode, int& iEventIndex);

	// Retrieves custom event type (Used to specify whether the event should be displayed in inputs, outputs or both) from event source node
	bool GetPrefabEventTypeFromEventSourceNode(CFlowNode* pNode, EPrefabEventType& eventType);

	// Updates all prefab instance nodes associated to a particular prefab instance to represent current events from event sources
	void UpdatePrefabInstanceNodeEvents(TPrefabInstancesData& instancesData);

	// Updates a prefab instance node to represent current events from event sources
	void UpdatePrefabEventsOnInstance(SPrefabEventInstanceEventsData* instanceEventsData, CFlowNode* pInstanceNode);

	// Attempts to retrieve associated event source data corresponding to an event index
	bool GetPrefabEventSourceData(TPrefabEventSourceData& eventsSourceData, const int iEventIndex, SPrefabEventSourceData** ppSourceData, const string** ppEventName);

	// Sets prefab event node port visibility (Only needs to reduce clutter when looking at nodes in editor)
	bool SetPrefabEventNodePortVisibility(CFlowNode* pNode, const string& portName, const bool bVisible, const bool bInputPort);

	typedef std::map<string, TPrefabInstancesData> TPrefabNameToInstancesData;

	// Mapping of all prefabs that have instances with events
	TPrefabNameToInstancesData m_prefabEventData;

	// Used to delay adding of event source nodes until setting prefab is complete (Only then can determine prefab + instance names)
	TPrefabNodeContainer m_delayedAddingEventSourceNodes;

	// Whether currently setting prefab or not (Prefab event source nodes created when this is true are put into m_delayedAddingEventSourceNodes)
	bool m_bCurrentlySettingPrefab;
};

#endif // __PrefabEvents_h__

