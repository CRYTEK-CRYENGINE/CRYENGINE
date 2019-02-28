// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IHyperGraph.h"
#include "HyperGraphNode.h"

class CHyperGraph;

enum EHyperEdgeDirection
{
	eHED_Up = 0,
	eHED_Down,
	eHED_Left,
	eHED_Right
};

//////////////////////////////////////////////////////////////////////////
//! CHyperGraphEdge connects output and input port of two graph nodes.
class CHyperEdge : public _i_reference_target_t
{
public:
	CHyperEdge()
		: dirIn(eHED_Up)
		, dirOut(eHED_Up)
		, cornerW(0)
		, cornerH(0)
		, cornerModified(0)
		, enabled(true)
		, debugCount(0)
		, debugCountInitialization(0)
		, isInInitializationPhase(false)
	{}

	HyperNodeID nodeIn;
	HyperNodeID nodeOut;
	CString     portIn;
	CString     portOut;
	bool        enabled;

	// Cached Points where edge is drawn.
	Gdiplus::PointF     pointIn;
	Gdiplus::PointF     pointOut;
	EHyperEdgeDirection dirIn;
	EHyperEdgeDirection dirOut;

	Gdiplus::PointF     cornerPoints[4];
	Gdiplus::PointF     originalCenter;
	float               cornerW, cornerH;
	int                 cornerModified;
	int                 debugCount;                 // counts all debug events including initialization
	int                 debugCountInitialization;   // counts only initialization phase events
	bool                isInInitializationPhase;

	//////////////////////////////////////////////////////////////////////////
	uint16 nPortIn;  // Index of input port.
	uint16 nPortOut; // Index of output port.
	//////////////////////////////////////////////////////////////////////////

	virtual bool IsEditable()                                               { return false; }
	virtual void DrawSpecial(Gdiplus::Graphics* pGr, Gdiplus::PointF where) {}
	virtual int  GetCustomSelectionMode()                                   { return -1; }
};

//////////////////////////////////////////////////////////////////////////
//! Node serialization context.
class CHyperGraphSerializer
{
public:
	CHyperGraphSerializer(CHyperGraph* pGraph, CObjectArchive* ar);

	// Check if loading or saving.
	bool IsLoading() const          { return m_bLoading; }
	void SelectLoaded(bool bEnable) { m_bSelectLoaded = bEnable; };

	void SaveNode(CHyperNode* pNode, bool bSaveEdges = true);
	void SaveEdge(CHyperEdge* pEdge);

	// Load nodes from xml.
	// On load: returns true if migration has occurred (-> dirty)
	bool Serialize(XmlNodeRef& node, bool bLoading, bool bLoadEdges = true, bool bIsPaste = false);

	// Get serialized xml node.
	XmlNodeRef  GetXmlNode();

	HyperNodeID GetId(HyperNodeID id);
	void        RemapId(HyperNodeID oldId, HyperNodeID newId);

	void        GetLoadedNodes(std::vector<CHyperNode*>& nodes) const;
	CHyperNode* GetFirstNode();

private:
	bool                               m_bLoading;
	bool                               m_bSelectLoaded;

	CHyperGraph*                       m_pGraph;
	CObjectArchive*                    m_pAR;

	std::map<HyperNodeID, HyperNodeID> m_remap;

	typedef std::set<THyperNodePtr> Nodes;
	typedef std::set<CHyperEdge*>   Edges;
	Nodes m_nodes;
	Edges m_edges;
};

//////////////////////////////////////////////////////////////////////////
//! An HyperGraph contains nodes and edges that link them.
//! This is a base class, specific graphs derive from this, notably CHyperFlowGraph
class CHyperGraph : public IHyperGraph
{
public:
	CHyperGraph(CHyperGraphManager* pManager);
	virtual ~CHyperGraph();

	void                OnHyperGraphManagerDestroyed() { m_pManager = NULL; }
	CHyperGraphManager* GetManager() const             { return m_pManager; }

	//////////////////////////////////////////////////////////////////////////
	// IHyperGraph implementation
	//////////////////////////////////////////////////////////////////////////
	virtual IHyperGraphEnumerator* GetNodesEnumerator();
	virtual void                   AddListener(IHyperGraphListener* pListener);
	virtual void                   RemoveListener(IHyperGraphListener* pListener);
	virtual IHyperNode*            CreateNode(const char* sNodeClass, Gdiplus::PointF& pos = Gdiplus::PointF(0.0f, 0.0f));
	virtual CHyperEdge*            CreateEdge();
	virtual IHyperNode*            CloneNode(IHyperNode* pFromNode);
	virtual void                   RemoveNode(IHyperNode* pNode);
	virtual void                   RemoveNodeKeepLinks(IHyperNode* pNode);
	virtual IHyperNode*            FindNode(HyperNodeID id) const;
	virtual void                   Serialize(XmlNodeRef& node, bool bLoading, CObjectArchive* ar = 0);
	virtual bool                   Save(const char* filename);
	virtual bool                   Load(const char* filename);
	virtual bool                   Migrate(XmlNodeRef& node);
	virtual bool                   Import(const char* filename);
	virtual const char*            GetName() const            { return m_name; };
	virtual void                   SetName(const char* sName) { m_name = sName; if (m_name.IsEmpty()) m_filename.Empty(); };
	virtual IHyperGraph*           Clone()                    { return 0; };
	virtual void                   PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx);
	//////////////////////////////////////////////////////////////////////////

	bool Reload(bool bForceReload = false);
	void InvalidateNode(IHyperNode* pNode, bool bModified = true);

	// Unselect all nodes.
	void UnselectAll();
	// Is any node selected
	bool IsSelectedAny();

	// Returns true if ports can be connected.
	virtual bool CanConnectPorts(CHyperNode* pSrcNode, CHyperNodePort* pSrcPort, CHyperNode* pTrgNode, CHyperNodePort* pTrgPort, CHyperEdge* pExistingEdge = nullptr);

	bool         HasEdge(CHyperNode* pSrcNode, CHyperNodePort* pSrcPort, CHyperNode* pTrgNode, CHyperNodePort* pTrgPort, CHyperEdge* pExistingEdge = nullptr);

	// Creates an edge between source node and target node.
	bool ConnectPorts(CHyperNode* pSrcNode, CHyperNodePort* pSrcPort, CHyperNode* pTrgNode, CHyperNodePort* pTrgPort);

	// Removes Edge.
	virtual void RemoveEdge(CHyperEdge* pEdge);
	virtual void RemoveEdgeSilent(CHyperEdge* pEdge);
	void         EditEdge(CHyperEdge* pEdge);
	virtual void EnableEdge(CHyperEdge* pEdge, bool bEnable);
	virtual bool IsNodeActivationModified();
	virtual void ClearDebugPortActivation();

	bool         GetAllEdges(std::vector<CHyperEdge*>& edges) const;
	bool         FindEdges(CHyperNode* pNode, std::vector<CHyperEdge*>& edges) const;
	CHyperEdge*  FindEdge(CHyperNode* pNode, CHyperNodePort* pPort) const;

	// Check if graph have any nodes.
	bool IsEmpty() const { return m_nodesMap.empty(); };

	// Mark hyper graph as modified.
	void                   SetModified(bool bModified = true);
	bool                   IsModified() const      { return m_bModified; }

	HyperNodeID            AllocateId()            { return m_nextNodeId++; };

	virtual IAIAction*     GetAIAction() const     { return NULL; }

	virtual ICustomAction* GetCustomAction() const { return NULL; }

	void                   SetViewPosition(CPoint point, float zoom);
	bool                   GetViewPosition(CPoint& point, float& zoom);

	//////////////////////////////////////////////////////////////////////////
	virtual void    SetGroupName(const CString& sName)   { m_groupName = sName; };
	virtual CString GetGroupName() const                 { return m_groupName; };

	virtual void    SetDescription(const CString& sDesc) { m_description = sDesc; };
	virtual CString GetDescription() const               { return m_description; };

	virtual void    SetEnabled(bool bEnable)             { m_bEnabled = bEnable; }
	virtual bool    IsEnabled() const                    { return m_bEnabled; }
	//////////////////////////////////////////////////////////////////////////

	// Specializations of this class may set this to true
	virtual bool         IsFlowGraph() { return false; }

	virtual IUndoObject* CreateUndo();
	virtual void         RecordUndo();

	const CString& GetFilename() const { return m_filename; }

	void           SendNotifyEvent(IHyperNode* pNode, EHyperGraphEvent event);

	float          GetViewZoom()    { return m_fViewZoom; }
	bool           HasError() const { return GetMissingNodesCount() > 0 || GetMissingPortsCount() > 0; }
	int            GetMissingNodesCount() const;
	int            GetMissingPortsCount() const;
	bool           IsLoading() const { return m_bLoadingNow; }
	void           SetHTreeItem(HTREEITEM hTreeItem) { m_HGVTreeItem = hTreeItem; }
	HTREEITEM      GetHTreeItem() const { return m_HGVTreeItem; }

protected:
	virtual CHyperEdge* MakeEdge(CHyperNode* pNodeOut, CHyperNodePort* pPortOut, CHyperNode* pNodeIn, CHyperNodePort* pPortIn, bool bEnabled);
	virtual void        OnPostLoad();

	// Clear all nodes and edges.
	virtual void ClearAll();

	virtual void RegisterEdge(CHyperEdge* pEdge);
	void         AddNode(CHyperNode* pNode);

	typedef std::map<HyperNodeID, THyperNodePtr> NodesMap;
	NodesMap m_nodesMap;

	CHyperGraphManager* m_pManager;

	typedef std::multimap<HyperNodeID, _smart_ptr<CHyperEdge>> EdgesMap;
	EdgesMap m_edgesMap;

	typedef std::list<IHyperGraphListener*> Listeners;
	Listeners   m_listeners;

	HTREEITEM   m_HGVTreeItem;

	CString     m_name;
	CString     m_filename;
	CString     m_groupName;
	CString     m_description;

	HyperNodeID m_nextNodeId;

	bool        m_bViewPosInitialized;
	CPoint      m_viewOffset;
	float       m_fViewZoom;

	bool        m_bModified;
	bool        m_bEnabled;

	int         m_iMissingNodes;
	int         m_iMissingPorts;

	void UpdateMissingCount();

	bool m_bLoadingNow;

private:
	friend class CHyperGraphSerializer;
};

