// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HyperGraphNode.h"

#include "HyperGraph.h"
#include "NodePainter/HyperNodePainter_Default.h"
#include "Nodes/BlackBoxNode.h"
#include "Nodes/MissingNode.h"
#include "FlowGraphVariables.h"
#include "Objects/ObjectLoader.h"

#define MIN_ZOOM_DRAW_ALL_ELEMS 0.1f

static const WCHAR* CvtStr(CString& str)
{
	static const size_t MAX_BUF = 1024;
	static WCHAR buffer[MAX_BUF];
	assert(str.GetLength() < MAX_BUF);
	int nLen = MultiByteToWideChar(CP_ACP, 0, str.GetBuffer(), -1, NULL, NULL);
	assert(nLen < MAX_BUF);
	nLen = MIN(nLen, MAX_BUF - 1);
	MultiByteToWideChar(CP_ACP, 0, str.GetBuffer(), -1, buffer, nLen);
	return buffer;
}

//////////////////////////////////////////////////////////////////////////
// Undo object for CHyperNode.
class CUndoHyperNode : public IUndoObject
{
public:
	CUndoHyperNode(CHyperNode* node)
	{
		// Stores the current state of this object.
		assert(node != 0);
		m_node = node;
		m_redo = 0;
		m_undo = XmlHelpers::CreateXmlNode("Undo");
		m_node->Serialize(m_undo, false);
	}
protected:
	virtual const char* GetDescription() { return "HyperNodeUndo"; };

	virtual void        Undo(bool bUndo)
	{
		if (bUndo)
		{
			m_redo = XmlHelpers::CreateXmlNode("Redo");
			m_node->Serialize(m_redo, false);
		}
		// Undo object state.
		m_node->Invalidate(true, false); // Invalidate previous position too.
		m_node->Serialize(m_undo, true);
		m_node->Invalidate(true, false);
	}
	virtual void Redo()
	{
		m_node->Invalidate(true, false); // Invalidate previous position too.
		m_node->Serialize(m_redo, true);
		m_node->Invalidate(true, false);
	}

private:
	THyperNodePtr m_node;
	XmlNodeRef    m_undo;
	XmlNodeRef    m_redo;
};

static CHyperNodePainter_Default painterDefault;

//////////////////////////////////////////////////////////////////////////
CHyperNode::CHyperNode()
{
	m_pGraph = NULL;
	m_bSelected = 0;
	m_bSizeValid = 0;
	m_nFlags = 0;
	m_id = 0;
	m_pBlackBox = NULL;
	m_debugCount = 0;
	m_drawPriority = MAX_DRAW_PRIORITY;
	m_iMissingPort = 0;
	m_rect = Gdiplus::RectF(0, 0, 0, 0);
	m_pPainter = &painterDefault;
	m_name = "";
	m_classname = "";
}

//////////////////////////////////////////////////////////////////////////
CHyperNode::~CHyperNode()
{
	if (m_pBlackBox)
		m_pBlackBox = NULL;
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::Done()
{
	if (m_pBlackBox)
	{
		((CBlackBoxNode*)m_pBlackBox)->RemoveNode(this);
		m_pBlackBox = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::CopyFrom(const CHyperNode& src)
{
	m_id = src.m_id;
	m_name = src.m_name;
	m_classname = src.m_classname;
	m_rect = src.m_rect;
	m_nFlags = src.m_nFlags;
	m_iMissingPort = src.m_iMissingPort;

	m_inputs = src.m_inputs;
	m_outputs = src.m_outputs;
}

//////////////////////////////////////////////////////////////////////////
IHyperGraph* CHyperNode::GetGraph() const
{
	return m_pGraph;
};

//////////////////////////////////////////////////////////////////////////
void CHyperNode::SetName(const char* sName)
{
	m_name = sName;
	if (m_pGraph)
		m_pGraph->SendNotifyEvent(this, EHG_NODE_SET_NAME);
	Invalidate(true, true);
}

const char* CHyperNode::GetInfoAsString() const
{
	string sNodeInfo;
	if (IsEditorSpecialNode())
	{
		sNodeInfo.Format("Class:  %s", GetClassName());
	}
	else
	{
		const CFlowNode* pFlowNode = static_cast<const CFlowNode*>(this);
		sNodeInfo.Format("Node ID:  %d\nClass:  %s\nCategory:  %s",
			GetId(),
			GetClassName(),
			pFlowNode->GetCategoryName());
	}
	return sNodeInfo.c_str();
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::Invalidate(bool bNeedsRedraw, bool bSetChanged)
{
	if (bNeedsRedraw)
		m_dispList.Clear();

	if (m_pGraph)
		m_pGraph->InvalidateNode(this, bSetChanged);
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::SetRect(const Gdiplus::RectF& rc)
{
	if (rc.Equals(m_rect))
		return;

	RecordUndo();
	m_rect = rc;
	SetModified();
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::SetSelected(bool bSelected)
{
	if (bSelected != m_bSelected)
	{
		m_bSelected = bSelected;
		if (!bSelected)
		{
			// Clear selection of all ports.
			int i;
			for (i = 0; i < m_inputs.size(); i++)
				m_inputs[i].bSelected = false;
			for (i = 0; i < m_outputs.size(); i++)
				m_outputs[i].bSelected = false;
		}

		m_pGraph->SendNotifyEvent(this, bSelected ? EHG_NODE_SELECT : EHG_NODE_UNSELECT);
		Invalidate(true, false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::Draw(CHyperGraphView* pView, Gdiplus::Graphics& gr, bool render)
{
	Gdiplus::Graphics* pOld = m_dispList.GetGraphics();
	m_dispList.SetGraphics(&gr);
	if (m_dispList.Empty())
		Redraw(gr);
	if (render)
	{
		Gdiplus::Matrix mat;
		gr.GetTransform(&mat);
		Gdiplus::PointF pos = GetPos();
		gr.TranslateTransform(pos.X, pos.Y);
		m_dispList.Draw(m_pGraph->GetViewZoom() > MIN_ZOOM_DRAW_ALL_ELEMS);
		gr.SetTransform(&mat);
	}
	m_dispList.SetGraphics(pOld);
}

void CHyperNode::Redraw(Gdiplus::Graphics& gr)
{
	m_dispList.Empty();
	Gdiplus::Graphics* pOld = m_dispList.GetGraphics();
	m_dispList.SetGraphics(&gr);
	m_pPainter->Paint(this, &m_dispList);

	Gdiplus::RectF temp = m_rect;
	m_rect = m_dispList.GetBounds();
	m_rect.X = temp.X;
	m_rect.Y = temp.Y;

	m_dispList.SetGraphics(pOld);
}

Gdiplus::SizeF CHyperNode::CalculateSize(Gdiplus::Graphics& gr)
{
	Gdiplus::Graphics* pOld = m_dispList.GetGraphics();
	m_dispList.SetGraphics(&gr);
	if (m_dispList.Empty())
		Redraw(gr);
	m_dispList.SetGraphics(pOld);

	Gdiplus::SizeF size;
	m_rect.GetSize(&size);
	return size;
}

//////////////////////////////////////////////////////////////////////////
CString CHyperNode::GetTitle() const
{
	if (m_name.IsEmpty())
	{
		CString title = m_classname;
#if 0 // AlexL: 10/03/2006 show full group:class
		int p = title.Find(':');
		if (p >= 0)
			title = title.Mid(p + 1);
#endif
		return title;
	}
	return m_name + " (" + m_classname + ")";
};

CString CHyperNode::VarToValue(IVariable* var)
{
	CString value = var->GetDisplayValue();

	// smart object helpers are stored in format class:name
	// but it's enough to display only the name
	if (var->GetDataType() == IVariable::DT_SOHELPER || var->GetDataType() == IVariable::DT_SONAVHELPER || var->GetDataType() == IVariable::DT_SOANIMHELPER)
		value.Delete(0, value.Find(':') + 1);
	else if (var->GetDataType() == IVariable::DT_SEQUENCE_ID)
	{
		// Return the human-readable name instead of the ID.
		IAnimSequence* pSeq = GetIEditorImpl()->GetMovieSystem()->FindSequenceById((uint32)atoi(value));
		if (pSeq)
			return pSeq->GetName();
	}

	return value;
}

//////////////////////////////////////////////////////////////////////////
CString CHyperNode::GetPortName(const CHyperNodePort& port)
{
	if (port.bInput)
	{
		CString text = port.pVar->GetHumanName();
		if (port.pVar->GetType() != IVariable::UNKNOWN && !port.nConnected)
		{
			text = text + "=" + VarToValue(port.pVar);
		}
		return text;
	}
	return port.pVar->GetHumanName();
}

//////////////////////////////////////////////////////////////////////////
CHyperNodePort* CHyperNode::FindPort(const char* portname, bool bInput)
{
	if (bInput)
	{
		for (int i = 0; i < m_inputs.size(); i++)
		{
			if (stricmp(m_inputs[i].GetName(), portname) == 0)
				return &m_inputs[i];
		}
	}
	else
	{
		for (int i = 0; i < m_outputs.size(); i++)
		{
			if (stricmp(m_outputs[i].GetName(), portname) == 0)
				return &m_outputs[i];
		}
	}
	return 0;
}

CHyperNodePort* CHyperNode::FindPort(int idx, bool bInput)
{
	if (bInput)
	{
		if (idx >= m_inputs.size())
			return NULL;

		return &m_inputs[idx];
	}
	else
	{
		if (idx >= m_outputs.size())
			return NULL;

		return &m_outputs[idx];
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
CHyperNodePort* CHyperNode::CreateMissingPort(const char* portname, bool bInput, bool bAttribute)
{
	if (m_pGraph && m_pGraph->IsLoading())
	{
		IVariablePtr pVar;
		if (bAttribute)
			pVar = new CVariableFlowNode<string>();
		else
			pVar = new CVariableFlowNodeVoid();

		if (strcmp(GetClassName(), MISSING_NODE_CLASS) != 0)
			pVar->SetHumanName(CString("MISSING: ") + portname);
		else
			pVar->SetHumanName(portname);

		pVar->SetName(portname);
		pVar->SetDescription("MISSING PORT");

		CHyperNodePort newPort(pVar, bInput, !bInput);    // bAllowMulti if output
		AddPort(newPort);

		++m_iMissingPort;

		if (bInput)
			return &(*m_inputs.rbegin());
		else
			return &(*m_outputs.rbegin());

	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::Serialize(XmlNodeRef& node, bool bLoading, CObjectArchive* ar)
{
	if (bLoading)
	{
		uint64 nInputHideMask = 0;
		uint64 nOutputHideMask = 0;
		// Saving.
		Vec3 pos(0, 0, 0);
		node->getAttr("Name", m_name);
		node->getAttr("Class", m_classname);
		node->getAttr("pos", pos);
		node->getAttr("InHideMask", nInputHideMask, false);
		node->getAttr("OutHideMask", nOutputHideMask, false);

		m_rect.X = pos.x;
		m_rect.Y = pos.y;
		m_rect.Width = m_rect.Height = 0;
		m_bSizeValid = false;

		{
			XmlNodeRef portsNode = node->findChild("Inputs");
			if (portsNode)
			{
				for (int i = 0; i < m_inputs.size(); i++)
				{
					IVariable* pVar = m_inputs[i].pVar;
					if (pVar->GetType() != IVariable::UNKNOWN)
						pVar->Serialize(portsNode, true);

					if (ar && pVar->GetDataType() == IVariable::DT_SEQUENCE_ID)
					{
						// A remapping might be necessary for a sequence Id.
						int seqId = 0;
						pVar->Get(seqId);
						pVar->Set((int)ar->RemapSequenceId((uint32)seqId));
					}

					if ((nInputHideMask & (1ULL << i)) && m_inputs[i].nConnected == 0)
						m_inputs[i].bVisible = false;
					else
						m_inputs[i].bVisible = true;
				}
				/*
				        for (int i = 0; i < portsNode->getChildCount(); i++)
				        {
				          XmlNodeRef portNode = portsNode->getChild(i);
				          CHyperNodePort *pPort = FindPort( portNode->getTag(),true );
				          if (!pPort)
				            continue;
				          pPort->pVar->Set( portNode->getContent() );
				        }
				 */
			}
		}
		// restore outputs visibility.
		{
			for (int i = 0; i < m_outputs.size(); i++)
			{
				if ((nOutputHideMask & (1ULL << i)) && m_outputs[i].nConnected == 0)
					m_outputs[i].bVisible = false;
				else
					m_outputs[i].bVisible = true;
			}
		}

		m_pBlackBox = NULL;

		Invalidate(true, true);
	}
	else
	{
		// Saving.
		if (!m_name.IsEmpty())
			node->setAttr("Name", m_name);
		node->setAttr("Class", m_classname);

		Vec3 pos(m_rect.X, m_rect.Y, 0);
		node->setAttr("pos", pos);

		uint64 nInputHideMask = 0;
		uint64 nOutputHideMask = 0;
		if (!m_inputs.empty())
		{
			XmlNodeRef portsNode = node->newChild("Inputs");
			for (int i = 0; i < m_inputs.size(); i++)
			{
				if (!m_inputs[i].bVisible)
					nInputHideMask |= (1ULL << i);
				IVariable* pVar = m_inputs[i].pVar;

				if (pVar->GetType() != IVariable::UNKNOWN)
					pVar->Serialize(portsNode, false);
			}
		}
		{
			// Calc output vis mask.
			for (int i = 0; i < m_outputs.size(); i++)
			{
				if (!m_outputs[i].bVisible)
					nOutputHideMask |= (1ULL << i);
			}
		}
		if (nInputHideMask != 0)
			node->setAttr("InHideMask", nInputHideMask, false);
		if (nOutputHideMask != 0)
			node->setAttr("OutHideMask", nOutputHideMask, false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::SetModified(bool bModified)
{
	m_pGraph->SetModified(bModified);
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::ClearPorts()
{
	m_inputs.clear();
	m_outputs.clear();
	m_iMissingPort = 0;
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::AddPort(const CHyperNodePort& port)
{
	if (port.bInput)
	{
		CHyperNodePort temp = port;
		temp.nPortIndex = m_inputs.size();
		m_inputs.push_back(temp);
	}
	else
	{
		CHyperNodePort temp = port;
		temp.nPortIndex = m_outputs.size();
		m_outputs.push_back(temp);
	}
	Invalidate(true, true);
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::PopulateContextMenu(CMenu& menu, int baseCommandId)
{
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::OnContextMenuCommand(int nCmd)
{
}

//////////////////////////////////////////////////////////////////////////
CVarBlock* CHyperNode::GetInputsVarBlock()
{
	CVarBlock* pVarBlock = new CVarBlock;
	for (int i = 0; i < m_inputs.size(); i++)
	{
		IVariable* pVar = m_inputs[i].pVar;
		if (pVar->GetType() != IVariable::UNKNOWN)
			pVarBlock->AddVariable(pVar);
	}
	return pVarBlock;
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::RecordUndo()
{
	if (CUndo::IsRecording() && !m_pBlackBox)
	{
		IUndoObject* pUndo = CreateUndo();
		assert(pUndo != 0);
		CUndo::Record(pUndo);
	}
}

//////////////////////////////////////////////////////////////////////////
IUndoObject* CHyperNode::CreateUndo()
{
	return new CUndoHyperNode(this);
}

//////////////////////////////////////////////////////////////////////////
IHyperGraphEnumerator* CHyperNode::GetRelatedNodesEnumerator()
{
	// Traverse the parent hierarchy upwards to find the ultimate ancestor.
	CHyperNode* pNode = this;
	while (pNode->GetParent())
		pNode = static_cast<CHyperNode*>(pNode->GetParent());

	// Create an enumerator that returns all the descendants of this node.
	return new CHyperNodeDescendentEnumerator(pNode);
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::SelectInputPort(int nPort, bool bSelected)
{
	if (nPort >= 0 && nPort < m_inputs.size())
	{
		if (m_inputs[nPort].bSelected != bSelected)
		{
			m_inputs[nPort].bSelected = bSelected;
			Invalidate(true, false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::SelectOutputPort(int nPort, bool bSelected)
{
	if (nPort >= 0 && nPort < m_outputs.size())
	{
		if (m_outputs[nPort].bSelected != bSelected)
		{
			m_outputs[nPort].bSelected = bSelected;
			Invalidate(true, false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::UnselectAllPorts()
{
	bool bAnySelected = false;
	int i = 0;
	for (i = 0; i < m_inputs.size(); i++)
	{
		if (m_inputs[i].bSelected)
			bAnySelected = true;
		m_inputs[i].bSelected = false;
	}

	for (i = 0; i < m_outputs.size(); i++)
	{
		if (m_outputs[i].bSelected)
			bAnySelected = true;
		m_outputs[i].bSelected = false;
	}

	if (bAnySelected)
		Invalidate(true, false);
}

CHyperNode* CHyperNode::GetBlackBox() const
{
	if (m_pBlackBox)
	{
		if (((CBlackBoxNode*)m_pBlackBox)->IsMinimized())
			return m_pBlackBox;
	}
	return NULL;
}

