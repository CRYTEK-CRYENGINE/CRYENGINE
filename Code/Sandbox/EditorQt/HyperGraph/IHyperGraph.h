// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct IHyperGraph;
struct IHyperNode;
struct IVariable;
struct IAIAction;
struct ICustomAction;
class CHyperEdge;
class CObjectArchive;

enum EHyperGraphEvent
{
	EHG_GRAPH_ADDED,
	EHG_GRAPH_REMOVED,
	EHG_GRAPH_MODIFIED,
	EHG_GRAPH_INVALIDATE,
	EHG_GRAPH_OWNER_CHANGE,
	EHG_GRAPH_UPDATE_FROZEN,
	EHG_GRAPH_CLEAR_SELECTION,
	EHG_GRAPH_UPDATE_ENTITY,
	EHG_GRAPH_CLEAR_ALL,
	EHG_GRAPH_EDGE_STATE_CHANGED,
	EHG_GRAPH_PROPERTIES_CHANGED,
	EHG_GRAPH_TOKENS_UPDATED,
	EHG_GRAPH_UNDO_REDO,
	EHG_NODE_ADD,
	EHG_NODE_DELETE,
	EHG_NODE_CHANGE,
	EHG_NODE_CHANGE_DEBUG_PORT,
	EHG_NODE_SELECT,
	EHG_NODE_UNSELECT,
	EHG_NODE_UPDATE_ENTITY,
	EHG_NODE_SET_NAME,
	EHG_NODE_MOVE,
	EHG_NODE_PROPERTIES_CHANGED,
	EHG_NODE_RESIZE,
	EHG_NODE_DELETED,
	EHG_NODES_PASTED
};

typedef uint32 HyperNodeID;

//! Callback class to intercept item creation and deletion events.
struct IHyperGraphListener
{
	virtual void OnHyperGraphEvent(IHyperNode* pNode, EHyperGraphEvent event) = 0;
	virtual void OnLinkEdit(CHyperEdge* pEdge) {}
};


//! Callback class to receive hyper graph manager notification event.
struct IHyperGraphManagerListener
{
	virtual void OnHyperGraphManagerEvent(EHyperGraphEvent event, IHyperGraph* pGraph, IHyperNode* pNode) = 0;
};


//! Interface to enumerate all items registered to the database manager.
struct IHyperGraphEnumerator
{
	virtual void        Release() = 0;
	virtual IHyperNode* GetFirst() = 0;
	virtual IHyperNode* GetNext() = 0;
};


//! IHyperGraph is the main interface to the hyper graph subsystem
struct IHyperGraph : public _i_reference_target_t
{
	virtual IHyperGraphEnumerator* GetNodesEnumerator() = 0;

	virtual void                   AddListener(IHyperGraphListener* pListener) = 0;
	virtual void                   RemoveListener(IHyperGraphListener* pListener) = 0;

	virtual IHyperNode*            CreateNode(const char* sNodeClass, Gdiplus::PointF& pos = Gdiplus::PointF(0.0f, 0.0f)) = 0;
	virtual IHyperNode*            CloneNode(IHyperNode* pFromNode) = 0;
	virtual void                   RemoveNode(IHyperNode* pNode) = 0;

	virtual void                   Serialize(XmlNodeRef& node, bool bLoading, CObjectArchive* ar) = 0;
	virtual bool                   Save(const char* filename) = 0;
	virtual bool                   Load(const char* filename) = 0;

	//! Migrate needed because not every serialize should migrate.
	//! \return false if nothing has been changed, true otherwise
	virtual bool           Migrate(XmlNodeRef& node) = 0;

	//! Return current hyper graph name.
	virtual const char*    GetName() const = 0;
	//! Assign new name to hyper graph.
	virtual void           SetName(const char* sName) = 0;

	virtual IAIAction*     GetAIAction() const = 0;
	virtual ICustomAction* GetCustomAction() const = 0;
};


//! IHyperNode is an interface to the single hyper graph node.
struct IHyperNode : public _i_reference_target_t
{
	//! Return the graph of this hyper node.
	virtual IHyperGraph* GetGraph() const = 0;

	//! Get node name.
	virtual const char* GetName() const = 0;
	//! Change node name.
	virtual void        SetName(const char* sName) = 0;

	//! Get node id.
	virtual HyperNodeID GetId() const = 0;
};

