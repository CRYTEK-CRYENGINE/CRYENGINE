// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "TrackEventNode.h"

#include "HyperGraph/HyperGraph.h"
#include "HyperGraph/FlowGraph.h"
#include "HyperGraph/FlowGraphVariables.h"

#include "Objects/EntityObject.h"

namespace
{
const int kSequenceName = 0;
const int kSequenceId = 1;
}

////////////////////////////////////////////////////////////////////////////
CTrackEventNode::CTrackEventNode()
{
	SetClass(GetClassType());
	m_pSequence = NULL;
}

////////////////////////////////////////////////////////////////////////////
CTrackEventNode::~CTrackEventNode()
{
	if (NULL != m_pSequence)
	{
		m_pSequence->RemoveTrackEventListener(this);
		m_pSequence->Release();
		m_pSequence = NULL;
	}
}

////////////////////////////////////////////////////////////////////////////
void CTrackEventNode::Init()
{
	CFlowNode::Init();
	m_pSequence = NULL;
}

////////////////////////////////////////////////////////////////////////////
void CTrackEventNode::Done()
{
	if (NULL != m_pSequence)
	{
		m_pSequence->RemoveTrackEventListener(this);
		m_pSequence->Release();
		m_pSequence = NULL;
	}

	CFlowNode::Done();
}

////////////////////////////////////////////////////////////////////////////
CHyperNode* CTrackEventNode::Clone()
{
	CTrackEventNode* pNode = new CTrackEventNode;
	pNode->CopyFrom(*this);

	pNode->m_entityGuid = m_entityGuid;
	pNode->m_pEntity = m_pEntity;
	pNode->m_flowSystemNodeFlags = m_flowSystemNodeFlags;
	pNode->m_szDescription = m_szDescription;
	pNode->m_szUIClassName = m_szUIClassName;

	Ports::iterator iter = pNode->m_inputs.begin();
	while (iter != pNode->m_inputs.end())
	{
		IVariable* pVar = (*iter).pVar;
		CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*>(pVar->GetUserData());
		if (pGetCustomItems != 0)
		{
			pGetCustomItems->SetFlowNode(pNode);
		}
		++iter;
	}

	return pNode;
}

////////////////////////////////////////////////////////////////////////////
void CTrackEventNode::Serialize(XmlNodeRef& node, bool bLoading, CObjectArchive* ar)
{
	CFlowNode::Serialize(node, bLoading, ar);

	if (bLoading)
	{
		PopulateOutput(true);

		// Load output ports from file
		XmlNodeRef outputs = node->findChild("Outputs");
		if (outputs)
		{
			string eventName;
			for (int i = 0; i < outputs->getChildCount(); i++)
			{
				XmlNodeRef xn = outputs->getChild(i);
				eventName = xn->getAttr("Name");
				AddOutputEvent(eventName);
			}
		}
	}
	else
	{
		// Save output ports
		if (NULL != m_pSequence)
		{
			XmlNodeRef outputs = node->newChild("Outputs");
			const int iCount = m_pSequence->GetTrackEventsCount();
			for (int i = 0; i < iCount; ++i)
			{
				XmlNodeRef xn = outputs->newChild("Output");
				xn->setAttr("Name", m_pSequence->GetTrackEvent(i));
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////
CString CTrackEventNode::GetTitle() const
{
	CString title = TRACKEVENTNODE_TITLE;
	if (!m_inputs.empty())
	{
		int id = 0;
		CString value;
		m_inputs[kSequenceId].pVar->Get(id);
		IAnimSequence* pSeq = GetIEditorImpl()->GetMovieSystem()->FindSequenceById((uint32)id);
		if (pSeq)
			value = pSeq->GetName();
		else
			m_inputs[kSequenceName].pVar->Get(value);
		if (!value.IsEmpty() && value != "")
		{
			title += ":";
			title += value;
		}
	}
	return title;
}

////////////////////////////////////////////////////////////////////////////
const char* CTrackEventNode::GetClassName() const
{
	return TRACKEVENTNODE_CLASS;
}

////////////////////////////////////////////////////////////////////////////
const char* CTrackEventNode::GetDescription() const
{
	return TRACKEVENTNODE_DESC;
}

////////////////////////////////////////////////////////////////////////////
void CTrackEventNode::OnInputsChanged()
{
	CFlowNode::OnInputsChanged();
	PopulateOutput(false);
}

////////////////////////////////////////////////////////////////////////////
void CTrackEventNode::PopulateOutput(bool bLoading)
{
	if (m_inputs.empty()) return;

	// Remove listener to current sequence
	if (NULL != m_pSequence)
	{
		m_pSequence->RemoveTrackEventListener(this);
		m_pSequence->Release();
		m_pSequence = NULL;
	}

	int id = 0;
	CString sequence;
	m_inputs[kSequenceId].pVar->Get(id);
	m_pSequence = GetIEditorImpl()->GetMovieSystem()->FindSequenceById((uint32)id);
	if (m_pSequence == NULL)
	{
		m_inputs[kSequenceName].pVar->Get(sequence);
		m_pSequence = GetIEditorImpl()->GetMovieSystem()->FindSequence(sequence);
	}

	// Check if sequence exists
	if (NULL == m_pSequence)
	{
		m_inputs[kSequenceName].pVar->Set("");
		m_inputs[kSequenceId].pVar->Set(0);
	}
	else
	{
		// Add listener
		m_pSequence->AddRef();
		m_pSequence->AddTrackEventListener(this);
	}

	// Don't bother with outputs on load
	if (false == bLoading)
	{
		// Clear all outputs
		while (false == m_outputs.empty())
			RemoveOutputEvent(m_outputs.begin()->GetName());

		// Add sequence event outputs
		// Create output port for each track event in selected sequence
		if (m_pSequence)
		{
			const int iCount = m_pSequence->GetTrackEventsCount();
			for (int i = 0; i < iCount; ++i)
			{
				AddOutputEvent(m_pSequence->GetTrackEvent(i));
			}
		}
	}

	// Invalidate and reset properties
	Invalidate(true, true);
}

////////////////////////////////////////////////////////////////////////////
void CTrackEventNode::AddOutputEvent(const char* event)
{
	CString desc;
	desc.Format("Called when %s is triggered from sequence", event);
	IVariablePtr pVar = new CVariableFlowNode<string>;
	pVar->SetName(event);
	pVar->SetHumanName(event);
	pVar->SetDescription(desc);
	CHyperNodePort port(pVar, false, true);
	AddPort(port);
	Invalidate(true, true);
}

////////////////////////////////////////////////////////////////////////////
void CTrackEventNode::RemoveOutputEvent(const char* event)
{
	// Find it
	Ports::iterator i = m_outputs.begin();
	bool bFound = false;
	while (i != m_outputs.end())
	{
		if (strcmp(i->GetName(), event) == 0)
		{
			// Remove the connected edges
			if (CHyperGraph* pGraph = (CHyperGraph*)GetGraph())
			{
				std::vector<CHyperEdge*> edges;
				std::vector<CHyperEdge*>::iterator it;
				if (pGraph->FindEdges(this, edges))
				{
					for (it = edges.begin(); it != edges.end(); ++it)
					{
						if (i->nPortIndex == (*it)->nPortOut)
							pGraph->RemoveEdgeSilent(*it);
					}
				}
			}

			// Erase from map
			i = m_outputs.erase(i);
			bFound = true;
		}
		else
		{
			if (true == bFound)
			{
				// Make a note of any edges connected to it
				if (CHyperGraph* pGraph = (CHyperGraph*)GetGraph())
				{
					std::vector<CHyperEdge*> edges;
					std::vector<CHyperEdge*>::iterator it;
					if (pGraph->FindEdges(this, edges))
					{
						for (it = edges.begin(); it != edges.end(); ++it)
						{
							if (i->nPortIndex == (*it)->nPortOut)
								(*it)->nPortOut--;
						}
					}
				}

				// Decrease its port count so it stays linear
				i->nPortIndex--;
			}
			++i;
		}
	}
	Invalidate(true, true);
}

////////////////////////////////////////////////////////////////////////////
void CTrackEventNode::RenameOutputEvent(const char* event, const char* newName)
{
	for (Ports::iterator i = m_outputs.begin(); i != m_outputs.end(); ++i)
	{
		if (strcmp(i->GetName(), event) == 0)
		{
			i->pVar->SetName(newName);
			i->pVar->SetHumanName(newName);
			CString desc;
			desc.Format("Called when %s is triggered from sequence", newName);
			i->pVar->SetDescription(desc);

			// Update the connected edges.
			if (CHyperGraph* pGraph = (CHyperGraph*)GetGraph())
			{
				std::vector<CHyperEdge*> edges;
				std::vector<CHyperEdge*>::iterator it;
				if (pGraph->FindEdges(this, edges))
				{
					for (it = edges.begin(); it != edges.end(); ++it)
					{
						if (i->nPortIndex == (*it)->nPortOut)
							(*it)->portOut = i->GetName();
					}
				}
			}
			break;
		}
	}
	Invalidate(true, true);
}

////////////////////////////////////////////////////////////////////////////
void CTrackEventNode::MoveUpOutputEvent(const char* event)
{
	for (size_t i = 0; i < m_outputs.size(); ++i)
	{
		if (strcmp(m_outputs[i].GetName(), event) == 0)
		{
			assert(i > 0);
			std::swap(m_outputs[i - 1].nPortIndex, m_outputs[i].nPortIndex);
			std::swap(m_outputs[i - 1], m_outputs[i]);

			// Update the edges connected to two engaged ports.
			if (CHyperFlowGraph* pGraph = (CHyperFlowGraph*)GetGraph())
			{
				std::vector<CHyperEdge*> edges;
				std::vector<CHyperEdge*>::iterator it;
				if (pGraph->FindEdges(this, edges))
				{
					for (it = edges.begin(); it != edges.end(); ++it)
					{
						CFlowEdge* pFlowEdge = static_cast<CFlowEdge*>(*it);
						if (m_outputs[i].nPortIndex == pFlowEdge->nPortOut)
						{
							pFlowEdge->nPortOut = m_outputs[i - 1].nPortIndex;
							pGraph->GetIFlowGraph()->UnlinkNodes(pFlowEdge->addr_out, pFlowEdge->addr_in);
							pFlowEdge->addr_out.port = pFlowEdge->nPortOut;
							pGraph->GetIFlowGraph()->LinkNodes(pFlowEdge->addr_out, pFlowEdge->addr_in);
						}
						else if (m_outputs[i - 1].nPortIndex == pFlowEdge->nPortOut)
						{
							pFlowEdge->nPortOut = m_outputs[i].nPortIndex;
							pGraph->GetIFlowGraph()->UnlinkNodes(pFlowEdge->addr_out, pFlowEdge->addr_in);
							pFlowEdge->addr_out.port = pFlowEdge->nPortOut;
							pGraph->GetIFlowGraph()->LinkNodes(pFlowEdge->addr_out, pFlowEdge->addr_in);
						}
					}
				}
			}
			break;
		}
	}
	Invalidate(true, true);
}

////////////////////////////////////////////////////////////////////////////
void CTrackEventNode::MoveDownOutputEvent(const char* event)
{
	for (size_t i = 0; i < m_outputs.size(); ++i)
	{
		if (strcmp(m_outputs[i].GetName(), event) == 0)
		{
			assert(i < m_outputs.size() - 1);
			std::swap(m_outputs[i].nPortIndex, m_outputs[i + 1].nPortIndex);
			std::swap(m_outputs[i], m_outputs[i + 1]);

			// Update the edges connected to two engaged ports.
			if (CHyperFlowGraph* pGraph = (CHyperFlowGraph*)GetGraph())
			{
				std::vector<CHyperEdge*> edges;
				std::vector<CHyperEdge*>::iterator it;
				if (pGraph->FindEdges(this, edges))
				{
					for (it = edges.begin(); it != edges.end(); ++it)
					{
						CFlowEdge* pFlowEdge = static_cast<CFlowEdge*>(*it);
						if (m_outputs[i].nPortIndex == pFlowEdge->nPortOut)
						{
							pFlowEdge->nPortOut = m_outputs[i + 1].nPortIndex;
							pGraph->GetIFlowGraph()->UnlinkNodes(pFlowEdge->addr_out, pFlowEdge->addr_in);
							pFlowEdge->addr_out.port = pFlowEdge->nPortOut;
							pGraph->GetIFlowGraph()->LinkNodes(pFlowEdge->addr_out, pFlowEdge->addr_in);
						}
						else if (m_outputs[i + 1].nPortIndex == pFlowEdge->nPortOut)
						{
							pFlowEdge->nPortOut = m_outputs[i].nPortIndex;
							pGraph->GetIFlowGraph()->UnlinkNodes(pFlowEdge->addr_out, pFlowEdge->addr_in);
							pFlowEdge->addr_out.port = pFlowEdge->nPortOut;
							pGraph->GetIFlowGraph()->LinkNodes(pFlowEdge->addr_out, pFlowEdge->addr_in);
						}
					}
				}
			}
			break;
		}
	}
	Invalidate(true, true);
}

////////////////////////////////////////////////////////////////////////////
void CTrackEventNode::OnTrackEvent(IAnimSequence* pSequence, int reason, const char* event, void* pUserData)
{
	switch (reason)
	{
	case ITrackEventListener::eTrackEventReason_Added:
		{
			// Check if not already added
			for (Ports::iterator i = m_outputs.begin(); i != m_outputs.end(); ++i)
			{
				if (strcmp(i->GetName(), event) == 0)
					return;   // Already got one for it
			}

			// Make a new one
			AddOutputEvent(event);
		}
		break;

	case ITrackEventListener::eTrackEventReason_Removed:
		{
			// Remove it
			RemoveOutputEvent(event);
		}
		break;

	case ITrackEventListener::eTrackEventReason_Renamed:
		{
			const char* newName = reinterpret_cast<const char*>(pUserData);
			// Check if not already exists
			for (Ports::iterator i = m_outputs.begin(); i != m_outputs.end(); ++i)
			{
				if (strcmp(i->GetName(), newName) == 0)
					return;   // Already got one for it
			}

			RenameOutputEvent(event, newName);
		}
		break;

	case ITrackEventListener::eTrackEventReason_MovedUp:
		{
			MoveUpOutputEvent(event);
		}
		break;

	case ITrackEventListener::eTrackEventReason_MovedDown:
		{
			MoveDownOutputEvent(event);
		}
		break;

	case ITrackEventListener::eTrackEventReason_Triggered:
		{
			// Get param
			const char* param = (const char*)pUserData;

			// Find output port and call it
			for (Ports::iterator i = m_outputs.begin(); i != m_outputs.end(); ++i)
			{
				if (strcmp(i->GetName(), event) == 0)
				{
					// Get its graph
					if (CHyperFlowGraph* pGraph = (CHyperFlowGraph*)GetGraph())
					{
						// Construct address
						SFlowAddress address;
						address.node = GetFlowNodeId();
						address.port = i->nPortIndex;
						address.isOutput = true;

						// Call it using param
						TFlowInputData value;
						value.Set(string(param));
						pGraph->GetIFlowGraph()->ActivatePortAny(address, value);
					}

					break;
				}
			}
		}
		break;
	}
}

