// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IHyperGraph.h"
#include "NodePainter/DisplayList.h"
#include <CryFlowGraph/IFlowSystem.h>
#include <stack>
#include "Util/Variable.h"

class CHyperGraph;
class CHyperGraphView;
class CHyperNode;
class CObjectCloneContext;
struct IHyperNodePainter;

typedef _smart_ptr<CHyperNode> THyperNodePtr;

enum EHyperNodeSubObjectId
{
	eSOID_InputTransparent = -1,

	eSOID_InputGripper     = 1,
	eSOID_OutputGripper,
	eSOID_AttachedEntity,
	eSOID_Title,
	eSOID_Background,
	eSOID_Minimize,
	eSOID_Border_UpRight,
	eSOID_Border_Right,
	eSOID_Border_DownRight,
	eSOID_Border_Down,
	eSOID_Border_DownLeft,
	eSOID_Border_Left,
	eSOID_Border_UpLeft,
	eSOID_Border_Up,

	eSOID_FirstInputPort       = 1000,
	eSOID_LastInputPort        = 1999,
	eSOID_FirstOutputPort      = 2000,
	eSOID_LastOutputPort       = 2999,
	eSOID_ShiftFirstOutputPort = 3000,
	eSOID_ShiftLastOutputPort  = 3999,
};

#define GRID_SIZE 18

//////////////////////////////////////////////////////////////////////////
//! Description of hyper node port. Used for both input and output ports.
class CHyperNodePort
{
public:
	_smart_ptr<IVariable> pVar;

	// True if input/false if output.
	unsigned int   bInput      : 1;
	unsigned int   bSelected   : 1;
	unsigned int   bVisible    : 1;
	unsigned int   bAllowMulti : 1;
	unsigned int   nConnected;
	int            nPortIndex;
	// Local rectangle in node space, where this port was drawn.
	Gdiplus::RectF rect;

	CHyperNodePort()
	{
		nPortIndex = 0;
		bSelected = false;
		bInput = false;
		bVisible = true;
		bAllowMulti = false;
		nConnected = 0;
	}
	CHyperNodePort(IVariable* _pVar, bool _bInput, bool _bAllowMulti = false)
	{
		nPortIndex = 0;
		bInput = _bInput;
		pVar = _pVar;
		bSelected = false;
		bVisible = true;
		bAllowMulti = _bAllowMulti;
		nConnected = 0;
	}
	CHyperNodePort(const CHyperNodePort& port)
	{
		bInput = port.bInput;
		bSelected = port.bSelected;
		bVisible = port.bVisible;
		nPortIndex = port.nPortIndex;
		nConnected = port.nConnected;
		bAllowMulti = port.bAllowMulti;
		rect = port.rect;
		pVar = port.pVar->Clone(true);
	}
	const char* GetName()      { return pVar->GetName(); }
	const char* GetHumanName() { return pVar->GetHumanName(); }
};

enum EHyperNodeFlags
{
	EHYPER_NODE_ENTITY        = 0x0001,
	EHYPER_NODE_ENTITY_VALID  = 0x0002,
	EHYPER_NODE_GRAPH_ENTITY  = 0x0004,    // This node targets graph default entity.
	EHYPER_NODE_GRAPH_ENTITY2 = 0x0008,    // Second graph default entity.
	EHYPER_NODE_INSPECTED     = 0x0010,    // Node is being inspected [dynamic, visual-only hint]
	EHYPER_NODE_HIDE_UI       = 0x0100,    // Not visible in components tree.
	EHYPER_NODE_CUSTOM_COLOR1 = 0x0200,    // Custom Color1
	EHYPER_NODE_UNREMOVEABLE  = 0x0400,    // Node cannot be deleted by user
};

//////////////////////////////////////////////////////////////////////////
// IHyperNode is an interface to the single hyper graph node.
//////////////////////////////////////////////////////////////////////////
class CHyperNode : public IHyperNode
{
public:
	typedef std::vector<CHyperNodePort> Ports;

	//////////////////////////////////////////////////////////////////////////
	// Node clone context.
	struct CloneContext
	{
	};
	//////////////////////////////////////////////////////////////////////////

	CHyperNode();
	virtual ~CHyperNode();

	//////////////////////////////////////////////////////////////////////////
	// IHyperNode
	//////////////////////////////////////////////////////////////////////////
	virtual IHyperGraph* GetGraph() const override;
	virtual HyperNodeID  GetId() const override   { return m_id; }
	virtual const char*  GetName() const override { return m_name; };
	virtual void         SetName(const char* sName) override;
	//////////////////////////////////////////////////////////////////////////

	virtual const char*            GetClassName() const   { return m_classname; };
	virtual const char*            GetDescription() const { return "No description available."; }
	virtual const char*            GetInfoAsString() const;
	virtual void                   Serialize(XmlNodeRef& node, bool bLoading, CObjectArchive* ar = 0);

	virtual IHyperNode*            GetParent() const                                                    { return nullptr; }
	virtual IHyperGraphEnumerator* GetChildrenEnumerator() const                                        { return nullptr; }

	virtual Gdiplus::Color         GetCategoryColor() const                                             { return Gdiplus::Color(160, 160, 165); }
	virtual void                   DebugPortActivation(TFlowPortId port, const char* value, bool bIsInitializationStep) {}
	virtual bool                   IsPortActivationModified(const CHyperNodePort* port = nullptr) const { return false; }
	virtual void                   ClearDebugPortActivation()                                           {}
	virtual CString                GetDebugPortValue(const CHyperNodePort& pp)                          { return GetPortName(pp); }
	virtual void                   ResetDebugPortActivation(CHyperNodePort* port)                       {}
	virtual bool                   GetAdditionalDebugPortInformation(const CHyperNodePort& pp, bool& bOutIsInitialization) { return false; }
	virtual bool                   IsDebugPortActivated(CHyperNodePort* port) const                     { return false; }
	virtual bool                   IsObsolete() const                                                   { return false; }

	void                           SetPainter(IHyperNodePainter* pPainter)
	{
		m_pPainter = pPainter;
		Invalidate(true, false);
	}

	void SetFlag(uint32 nFlag, bool bSet) { if (bSet) m_nFlags |= nFlag; else m_nFlags &= ~nFlag; }
	bool CheckFlag(uint32 nFlag) const    { return ((m_nFlags & nFlag) == nFlag); }

	// Changes node class.
	void SetGraph(CHyperGraph* pGraph) { m_pGraph = pGraph; }
	void SetClass(const CString& classname)
	{
		m_classname = classname;
	}

	Gdiplus::PointF GetPos() const              { return Gdiplus::PointF(m_rect.X, m_rect.Y); }
	virtual void    SetPos(Gdiplus::PointF pos)
	{
		m_rect = Gdiplus::RectF(pos.X, pos.Y, m_rect.Width, m_rect.Height);
		Invalidate(false, true);
	}

	// Initializes HyperGraph node for specific graph.
	virtual void        Init() = 0;
	// Deinitializes HyperGraph node.
	virtual void        Done();
	// Clone this hyper graph node.
	virtual CHyperNode* Clone() = 0;
	virtual void        CopyFrom(const CHyperNode& src);

	virtual void        PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx) {}

	//////////////////////////////////////////////////////////////////////////
	Ports* GetInputs()  { return &m_inputs; };
	Ports* GetOutputs() { return &m_outputs; };
	//////////////////////////////////////////////////////////////////////////

	// Finds a port by name.
	virtual CHyperNodePort* FindPort(const char* portname, bool bInput);
	// Finds a port by index
	virtual CHyperNodePort* FindPort(int idx, bool bInput);

	CHyperNodePort*         CreateMissingPort(const char* portname, bool bInput, bool bAttribute = false);
	int                     GetMissingPortCount() const { return m_iMissingPort; }

	// Adds a new port to the hyper graph node.
	// Only used by hypergraph manager.
	void AddPort(const CHyperNodePort& port);
	void ClearPorts();

	// Invalidate is called to notify UI that some parameters of graph node have been changed.
	void Invalidate(bool bNeedsRedraw, bool bSetChanged);

	//////////////////////////////////////////////////////////////////////////
	// Drawing interface.
	//////////////////////////////////////////////////////////////////////////
	void                  Draw(CHyperGraphView* pView, Gdiplus::Graphics& gr, bool render);

	const Gdiplus::RectF& GetRect() const
	{
		return m_rect;
	}
	void SetRect(const Gdiplus::RectF& rc);

	// Calculates node's screen size.
	Gdiplus::SizeF CalculateSize(Gdiplus::Graphics& dc);

	enum { MAX_DRAW_PRIORITY = 255 };
	virtual int                   GetDrawPriority()                                         { return m_drawPriority; }
	virtual const Gdiplus::RectF* GetResizeBorderRect() const                               { return NULL; }
	virtual void                  SetResizeBorderRect(const Gdiplus::RectF& newBordersRect) {}
	virtual void                  OnZoomChange(float zoom)                                  {}
	virtual bool                  IsEditorSpecialNode() const                               { return false; }
	virtual bool                  IsFlowNode() const                                        { return true; }
	virtual bool                  IsTooltipShowable() const                                 { return true; }

	virtual TFlowNodeId           GetTypeId() const                                         { return InvalidFlowNodeTypeId; }

	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	virtual void SetSelected(bool bSelected);
	bool         IsSelected() const { return m_bSelected; };

	void         SelectInputPort(int nPort, bool bSelected);
	void         SelectOutputPort(int nPort, bool bSelected);

	void         SetModified(bool bModified = true);

	// Called when any input ports values changed.
	virtual void OnInputsChanged() {}

	// called when we are about to enter game mode
	virtual void OnEnteringGameMode() {}

	// get FlowNodeId if it is s FlowGraph node
	virtual TFlowNodeId GetFlowNodeId() { return InvalidFlowNodeId; }

	// Called when edge is unlinked from this node.
	virtual void Unlinked(bool bInput)    {}

	virtual int  GetCustomSelectionMode() { return -1; }

	//////////////////////////////////////////////////////////////////////////
	// Context Menu processing.
	//////////////////////////////////////////////////////////////////////////
	virtual void PopulateContextMenu(CMenu& menu, int baseCommandId);
	virtual void OnContextMenuCommand(int nCmd);

	//////////////////////////////////////////////////////////////////////////
	virtual CVarBlock* GetInputsVarBlock();

	virtual CString    GetTitle() const;
	virtual bool       IsEntityValid() const  { return false; }
	virtual CString    GetEntityTitle() const { return ""; }
	virtual CString    GetPortName(const CHyperNodePort& port);

	virtual CString    VarToValue(IVariable* var);

	bool               GetAttachRect(int i, Gdiplus::RectF* pRect)
	{
		return m_dispList.GetAttachRect(i, pRect);
	}

	virtual int GetObjectAt(Gdiplus::Graphics* pGr, Gdiplus::PointF point)
	{
		Gdiplus::Graphics* pOld = m_dispList.GetGraphics();
		m_dispList.SetGraphics(pGr);
		int obj = m_dispList.GetObjectAt(point - GetPos());
		m_dispList.SetGraphics(pOld);
		return obj;
	}

	virtual CHyperNodePort* GetPortAtPoint(Gdiplus::Graphics* pGr, Gdiplus::PointF point)
	{
		int obj = GetObjectAt(pGr, point);
		if (obj >= eSOID_FirstInputPort && obj <= eSOID_LastInputPort)
			return &m_inputs.at(obj - eSOID_FirstInputPort);
		else if (obj >= eSOID_FirstOutputPort && obj <= eSOID_LastOutputPort)
			return &m_outputs.at(obj - eSOID_FirstOutputPort);
		return 0;
	}
	void                   UnselectAllPorts();

	void                   RecordUndo();
	virtual IUndoObject*   CreateUndo();

	IHyperGraphEnumerator* GetRelatedNodesEnumerator();

	//////////////////////////////////////////////////////////////////////////
	//Black Box code
	void        SetBlackBox(CHyperNode* pNode) { m_pBlackBox = pNode; Invalidate(true, true); }
	CHyperNode* GetBlackBox() const;

	int         GetDebugCount() const { return m_debugCount; }
	void        IncrementDebugCount() { m_debugCount++; }
	void        ResetDebugCount()     { m_debugCount = 0; }

protected:
	void Redraw(Gdiplus::Graphics& gr);

protected:
	friend class CHyperGraph;
	friend class CHyperGraphManager;

	CHyperGraph*   m_pGraph;
	HyperNodeID    m_id;

	CString        m_name;
	CString        m_classname;

	Gdiplus::RectF m_rect;

	uint32         m_nFlags;
	int            m_drawPriority;

	unsigned int   m_bSizeValid : 1;
	unsigned int   m_bSelected  : 1;

	// Input/Output ports.
	Ports m_inputs;
	Ports m_outputs;

	IHyperNodePainter* m_pPainter;
	CDisplayList       m_dispList;

	CHyperNode*        m_pBlackBox;

	int                m_iMissingPort;

private:
	int m_debugCount;
};

template<typename It> class CHyperGraphIteratorEnumerator : public IHyperGraphEnumerator
{
public:
	typedef It Iterator;

	CHyperGraphIteratorEnumerator(Iterator begin, Iterator end)
		: m_begin(begin),
		m_end(end)
	{
	}
	virtual void        Release() { delete this; };
	virtual IHyperNode* GetFirst()
	{
		if (m_begin != m_end)
			return *m_begin++;
		return 0;
	}
	virtual IHyperNode* GetNext()
	{
		if (m_begin != m_end)
			return *m_begin++;
		return 0;
	}

private:
	Iterator m_begin;
	Iterator m_end;
};

//////////////////////////////////////////////////////////////////////////
class CHyperNodeDescendentEnumerator : public IHyperGraphEnumerator
{
	CHyperNode*                        m_pNextNode;

	std::stack<IHyperGraphEnumerator*> m_stack;

public:
	CHyperNodeDescendentEnumerator(CHyperNode* pParent)
		: m_pNextNode(pParent)
	{
	}
	virtual void        Release() { delete this; };
	virtual IHyperNode* GetFirst()
	{
		return Iterate();
	}
	virtual IHyperNode* GetNext()
	{
		return Iterate();
	}

private:
	IHyperNode* Iterate()
	{
		CHyperNode* pNode = m_pNextNode;
		if (!pNode)
			return 0;

		// Check whether the current node has any children.
		IHyperGraphEnumerator* children = m_pNextNode->GetChildrenEnumerator();
		CHyperNode* pChild = 0;
		if (children)
			pChild = static_cast<CHyperNode*>(children->GetFirst());
		if (!pChild && children)
			children->Release();

		// The current node has children.
		if (pChild)
		{
			m_pNextNode = pChild;
			m_stack.push(children);
		}
		else
		{
			// The current node has no children - go to the next node.
			CHyperNode* pNextNode = 0;
			while (!pNextNode && !m_stack.empty())
			{
				pNextNode = static_cast<CHyperNode*>(m_stack.top()->GetNext());
				if (!pNextNode)
				{
					m_stack.top()->Release();
					m_stack.pop();
				}
			}
			m_pNextNode = pNextNode;
		}

		return pNode;
	}
};

