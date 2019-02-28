// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SequencerSequence.h"
#include "SequencerNode.h"
#include <ICryMannequin.h>

CSequencerSequence::CSequencerSequence()
{
}

CSequencerSequence::~CSequencerSequence()
{
}

void CSequencerSequence::SetName(const char* name)
{
	m_name = name;
}

const char* CSequencerSequence::GetName() const
{
	return m_name.c_str();
}

int CSequencerSequence::GetNodeCount() const
{
	return m_nodes.size();
}

CSequencerNode* CSequencerSequence::GetNode(int index) const
{
	assert(index >= 0 && index < (int)m_nodes.size());
	return m_nodes[index];
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerSequence::AddNode(CSequencerNode* node)
{
	assert(node != 0);

	// Check if this node already in sequence.
	for (int i = 0; i < (int)m_nodes.size(); i++)
	{
		if (node == m_nodes[i])
		{
			// Fail to add node second time.
			return false;
		}
	}

	node->SetSequence(this);
	node->SetTimeRange(m_timeRange);
	m_nodes.push_back(node);

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CSequencerSequence::RemoveNode(CSequencerNode* node)
{
	assert(node != 0);
	for (int i = 0; i < (int)m_nodes.size(); )
	{
		if (node == m_nodes[i])
		{
			m_nodes.erase(m_nodes.begin() + i);
			continue;
		}
		i++;
	}
}

void CSequencerSequence::ReorderNode(CSequencerNode* node, CSequencerNode* pPivotNode, bool next)
{
	if (node == pPivotNode || !node)
		return;

	_smart_ptr<CSequencerNode> pTempHolder = node; // Keep reference to node so it is not deleted by erasing from list.
	stl::find_and_erase(m_nodes, node);

	SequencerNodes::iterator it;
	for (it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		CSequencerNode* anode = *it;
		if (anode == pPivotNode)
		{
			if (next)
				m_nodes.insert(it + 1, node);
			else
				m_nodes.insert(it, node);
			break;
		}
	}
	if (it == m_nodes.end())
	{
		m_nodes.insert(m_nodes.begin(), node);
	}
}

CSequencerNode* CSequencerSequence::FindNodeByName(const char* sNodeName)
{
	for (SequencerNodes::const_iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		CSequencerNode* anode = *it;
		// Case insensitive name comparison.
		if (stricmp(anode->GetName(), sNodeName) == 0)
		{
			return anode;
		}
	}
	return 0;
}

void CSequencerSequence::RemoveAll()
{
	stl::free_container(m_nodes);
}

// TODO: Ensure this is called!
void CSequencerSequence::UpdateKeys()
{
	for (SequencerNodes::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		CSequencerNode* anode = *it;
		anode->UpdateKeys();
	}
}

void CSequencerSequence::SetTimeRange(Range timeRange)
{
	m_timeRange = timeRange;
	for (SequencerNodes::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		CSequencerNode* anode = *it;
		anode->SetTimeRange(timeRange);
	}
}

