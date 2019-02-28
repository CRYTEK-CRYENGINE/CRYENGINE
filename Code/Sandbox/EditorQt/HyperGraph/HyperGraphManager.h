// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SandboxAPI.h"
#include "HyperGraphNode.h"

struct IHyperGraph;

class CHyperGraph;
class CHyperNode;
class CHyperGraphDialog;

//////////////////////////////////////////////////////////////////////////
//! Manages collection of hyper node classes that can be created for hyper graphs.
class SANDBOX_API CHyperGraphManager
{
public:
	typedef Functor1wRet<CHyperNode*, bool> NodeFilterFunctor;

	//! Initialize graph manager. Must be called after full game initialization.
	virtual void Init();

	//! Reload prototype node classes and all graphs
	virtual void         ReloadClasses() = 0;
	//! Reload all graphs with current prototypes
	virtual void         ReloadGraphs() = 0;
	//! Update/Add a registered type's prototype without reloading all graphs
	virtual void         UpdateNodePrototypeWithoutReload(const char* nodeClassName) = 0;

	void                 GetPrototypes(std::vector<CString>& prototypes, bool bForUI, NodeFilterFunctor filterFunc = NodeFilterFunctor());
	void                 GetPrototypesEx(std::vector<THyperNodePtr>& prototypes, bool bForUI, NodeFilterFunctor filterFunc = NodeFilterFunctor());

	virtual CHyperGraph* CreateGraph() = 0;
	virtual CHyperNode*  CreateNode(CHyperGraph* pGraph, const char* sNodeClass, HyperNodeID nodeId,
                                  const Gdiplus::PointF& pos = Gdiplus::PointF(0.0f, 0.0f),
                                  CBaseObject* pObj = nullptr, bool bAllowMissing = false);

	virtual void         AddListener(IHyperGraphManagerListener* pListener);
	virtual void         RemoveListener(IHyperGraphManagerListener* pListener);

	//! Opens view of the specified hyper graph.
	CHyperGraphDialog*   OpenView(IHyperGraph* pGraph);

	void                 SendNotifyEvent(EHyperGraphEvent event, IHyperGraph* pGraph = 0, IHyperNode* pNode = 0);
	//! Call this function to stop the listeners from getting events from the manager
	void                 DisableNotifyListeners(bool disable) { m_notifyListenersDisabled = disable; }
	//! Main function to enable/disable event processing in GUI controls displaying flowgraph upon FG changes
	void                 SetGUIControlsProcessEvents(bool active, bool refreshTreeCtrList);
	//! Function to set a current hypergraph in the active view
	void                 SetCurrentViewedGraph(CHyperGraph* pGraph);

protected:
	typedef std::map<string, THyperNodePtr> NodesPrototypesMap;
	NodesPrototypesMap m_prototypes;

	typedef std::list<IHyperGraphManagerListener*> Listeners;
	Listeners m_listeners;
	bool      m_notifyListenersDisabled;
};

