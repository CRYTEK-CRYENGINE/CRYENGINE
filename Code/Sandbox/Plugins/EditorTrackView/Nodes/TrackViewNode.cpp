// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"
#include "TrackViewAnimNode.h"
#include "TrackViewTrack.h"
#include "TrackViewSequence.h"

void CTrackViewKeyConstHandle::GetKey(STrackKey* pKey) const
{
	assert(m_bIsValid);
	m_pTrack->GetKey(m_keyIndex, pKey);
}

SAnimTime CTrackViewKeyConstHandle::GetTime() const
{
	assert(m_bIsValid);
	return m_pTrack->GetKeyTime(m_keyIndex);
}

SAnimTime CTrackViewKeyConstHandle::GetDuration() const
{
	assert(m_bIsValid);
	return m_pTrack->GetKeyDuration(m_keyIndex);
}

SAnimTime CTrackViewKeyConstHandle::GetAnimDuration() const
{
	assert(m_bIsValid);
	return m_pTrack->GetKeyAnimDuration(m_keyIndex);
}

SAnimTime CTrackViewKeyConstHandle::GetAnimStart() const
{
	assert(m_bIsValid);
	return m_pTrack->GetKeyAnimStart(m_keyIndex);
}

bool CTrackViewKeyConstHandle::IsAnimLoopable() const
{
	assert(m_bIsValid);
	return m_pTrack->IsKeyAnimLoopable(m_keyIndex);
}

void CTrackViewKeyHandle::SetKey(const STrackKey* pKey)
{
	assert(m_bIsValid);
	m_pTrack->SetKey(m_keyIndex, pKey);
}

void CTrackViewKeyHandle::GetKey(STrackKey* pKey) const
{
	assert(m_bIsValid);
	m_pTrack->GetKey(m_keyIndex, pKey);
}

void CTrackViewKeyHandle::Select(bool bSelect)
{
	assert(m_bIsValid);
	m_pTrack->SelectKey(m_keyIndex, bSelect);
}

bool CTrackViewKeyHandle::IsSelected() const
{
	assert(m_bIsValid);
	return m_pTrack->IsKeySelected(m_keyIndex);
}

SAnimTime CTrackViewKeyHandle::GetTime() const
{
	assert(m_bIsValid);
	return m_pTrack->GetKeyTime(m_keyIndex);
}

void CTrackViewKeyHandle::SetDuration(SAnimTime duration)
{
	assert(m_bIsValid);
	m_pTrack->SetKeyDuration(m_keyIndex, duration);
}

SAnimTime CTrackViewKeyHandle::GetDuration() const
{
	assert(m_bIsValid);
	return m_pTrack->GetKeyDuration(m_keyIndex);
}

SAnimTime CTrackViewKeyHandle::GetCycleDuration() const
{
	assert(m_bIsValid);
	return m_pTrack->GetKeyAnimDuration(m_keyIndex);
}

SAnimTime CTrackViewKeyHandle::GetAnimStart() const
{
	assert(m_bIsValid);
	return m_pTrack->GetKeyAnimStart(m_keyIndex);
}

SAnimTime CTrackViewKeyHandle::GetAnimEnd() const
{
	assert(m_bIsValid);
	return m_pTrack->GetKeyAnimEnd(m_keyIndex);
}

bool CTrackViewKeyHandle::IsAnimLoopable() const
{
	assert(m_bIsValid);
	return m_pTrack->IsKeyAnimLoopable(m_keyIndex);
}

string CTrackViewKeyHandle::GetDescription() const
{
	assert(m_bIsValid);
	return m_pTrack->GetKeyDescription(m_keyIndex);
}

_smart_ptr<IAnimKeyWrapper> CTrackViewKeyHandle::GetWrappedKey()
{
	assert(m_bIsValid);
	return m_pTrack->GetWrappedKey(m_keyIndex);
}

void CTrackViewKeyHandle::Delete()
{
	assert(m_bIsValid);
	m_pTrack->RemoveKey(m_keyIndex);
	m_bIsValid = false;
}

bool CTrackViewKeyHandle::operator==(const CTrackViewKeyHandle& keyHandle) const
{
	return m_pTrack == keyHandle.m_pTrack && m_keyIndex == keyHandle.m_keyIndex;
}

bool CTrackViewKeyHandle::operator!=(const CTrackViewKeyHandle& keyHandle) const
{
	return !(*this == keyHandle);
}

void CTrackViewKeyBundle::AppendKey(const CTrackViewKeyHandle& keyHandle)
{
	// Check if newly added key has different type than existing ones
	if (m_bAllOfSameType && m_keys.size() > 0)
	{
		const CTrackViewKeyHandle& lastKey = m_keys.back();

		const CTrackViewTrack* pMyTrack = keyHandle.GetTrack();
		const CTrackViewTrack* pOtherTrack = lastKey.GetTrack();

		// Check if keys are from sub tracks, always compare types of parent track
		if (pMyTrack->IsSubTrack())
		{
			pMyTrack = static_cast<const CTrackViewTrack*>(pMyTrack->GetParentNode());
		}

		if (pOtherTrack->IsSubTrack())
		{
			pOtherTrack = static_cast<const CTrackViewTrack*>(pOtherTrack->GetParentNode());
		}

		// Do comparison
		if (pMyTrack->GetParameterType() != pOtherTrack->GetParameterType()
		    || pMyTrack->GetValueType() != pOtherTrack->GetValueType())
		{
			m_bAllOfSameType = false;
		}
	}

	m_keys.push_back(keyHandle);
}

void CTrackViewKeyBundle::AppendKeyBundle(const CTrackViewKeyBundle& bundle)
{
	for (auto iter = bundle.m_keys.begin(); iter != bundle.m_keys.end(); ++iter)
	{
		AppendKey(*iter);
	}
}

void CTrackViewKeyBundle::SelectKeys(const bool bSelected)
{
	const unsigned int numKeys = GetKeyCount();

	for (unsigned int i = 0; i < numKeys; ++i)
	{
		GetKey(i).Select(bSelected);
	}
}

CTrackViewKeyHandle CTrackViewKeyBundle::GetSingleSelectedKey()
{
	const unsigned int keyCount = GetKeyCount();

	if (keyCount == 1)
	{
		return m_keys[0];
	}
	else if (keyCount > 1 && keyCount <= 4)
	{
		// All keys must have same time & same parent track
		CTrackViewNode* pFirstParent = m_keys[0].GetTrack()->GetParentNode();
		const SAnimTime firstTime = m_keys[0].GetTime();

		// Parent must be a track
		if (pFirstParent->GetNodeType() != eTVNT_Track)
		{
			return CTrackViewKeyHandle();
		}

		// Check other keys for equality
		for (unsigned int i = 0; i < keyCount; ++i)
		{
			if (m_keys[i].GetTrack()->GetParentNode() != pFirstParent || m_keys[i].GetTime() != firstTime)
			{
				return CTrackViewKeyHandle();
			}
		}

		return static_cast<CTrackViewTrack*>(pFirstParent)->GetKeyByTime(firstTime);
	}

	return CTrackViewKeyHandle();
}

CTrackViewNode::CTrackViewNode(CTrackViewNode* pParent)
	: m_pParentNode(pParent)
	, m_bSelected(false)
{
}

void CTrackViewNode::ClearSelection()
{
	CTrackViewSequenceNotificationContext context(GetSequence());

	SetSelected(false);

	const unsigned int numChilds = GetChildCount();
	for (unsigned int childIndex = 0; childIndex < numChilds; ++childIndex)
	{
		GetChild(childIndex)->ClearSelection();
	}
}

void CTrackViewNode::SetSelected(bool bSelected)
{
	if (bSelected != m_bSelected)
	{
		m_bSelected = bSelected;

		if (m_bSelected)
		{
			GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_Selected);
		}
		else
		{
			GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_Deselected);
		}

		GetSequence()->OnNodeSelectionChanged();
	}
}

CTrackViewSequence* CTrackViewNode::GetSequence()
{
	for (CTrackViewNode* pCurrentNode = this; pCurrentNode; pCurrentNode = pCurrentNode->GetParentNode())
	{
		if (pCurrentNode->GetNodeType() == eTVNT_Sequence)
		{
			return static_cast<CTrackViewSequence*>(pCurrentNode);
		}
	}

	return nullptr;
}

const CTrackViewSequence* CTrackViewNode::GetSequence() const
{
	for (const CTrackViewNode* pCurrentNode = this; pCurrentNode; pCurrentNode = pCurrentNode->GetParentNode())
	{
		if (pCurrentNode->GetNodeType() == eTVNT_Sequence)
		{
			return static_cast<const CTrackViewSequence*>(pCurrentNode);
		}
	}

	return nullptr;
}

void CTrackViewNode::AddNode(CTrackViewNode* pNode)
{
	assert(pNode->GetNodeType() != eTVNT_Sequence);

	m_childNodes.push_back(std::unique_ptr<CTrackViewNode>(pNode));
	SortNodes();

	pNode->m_pParentNode = this;
	GetSequence()->OnNodeChanged(pNode, ITrackViewSequenceListener::eNodeChangeType_Added);
}

void CTrackViewNode::SortNodes()
{
	// Sort with operator<
	std::stable_sort(m_childNodes.begin(), m_childNodes.end(),
	                 [&](const std::unique_ptr<CTrackViewNode>& a, const std::unique_ptr<CTrackViewNode>& b) -> bool
	{
		const CTrackViewNode* pA = a.get();
		const CTrackViewNode* pB = b.get();
		return *pA < *pB;
	}
	                 );
}

namespace
{
static int GetNodeOrder(EAnimNodeType nodeType)
{
	assert(nodeType < eAnimNodeType_Num);

	static int nodeOrder[eAnimNodeType_Num];
	nodeOrder[eAnimNodeType_Invalid] = 0;
	nodeOrder[eAnimNodeType_Director] = 1;
	nodeOrder[eAnimNodeType_Camera] = 2;
	nodeOrder[eAnimNodeType_Entity] = 3;
	nodeOrder[eAnimNodeType_Alembic] = 4;
	nodeOrder[eAnimNodeType_GeomCache] = 5;
	nodeOrder[eAnimNodeType_CVar] = 6;
	nodeOrder[eAnimNodeType_ScriptVar] = 7;
	nodeOrder[eAnimNodeType_Material] = 8;
	nodeOrder[eAnimNodeType_Event] = 9;
	nodeOrder[eAnimNodeType_Layer] = 10;
	nodeOrder[eAnimNodeType_Comment] = 11;
	nodeOrder[eAnimNodeType_RadialBlur] = 12;
	nodeOrder[eAnimNodeType_ColorCorrection] = 13;
	nodeOrder[eAnimNodeType_DepthOfField] = 14;
	nodeOrder[eAnimNodeType_ScreenFader] = 15;
	nodeOrder[eAnimNodeType_Light] = 16;
	nodeOrder[eAnimNodeType_HDRSetup] = 17;
	nodeOrder[eAnimNodeType_ShadowSetup] = 18;
	nodeOrder[eAnimNodeType_Environment] = 19;
	nodeOrder[eAnimNodeType_Group] = 20;

	return nodeOrder[nodeType];
}
}

bool CTrackViewNode::operator<(const CTrackViewNode& otherNode) const
{
	// Order nodes before tracks
	if (GetNodeType() < otherNode.GetNodeType())
	{
		return true;
	}
	else if (GetNodeType() > otherNode.GetNodeType())
	{
		return false;
	}

	// Same node type
	switch (GetNodeType())
	{
	case eTVNT_AnimNode:
		{
			const CTrackViewAnimNode& thisAnimNode = static_cast<const CTrackViewAnimNode&>(*this);
			const CTrackViewAnimNode& otherAnimNode = static_cast<const CTrackViewAnimNode&>(otherNode);

			const int thisTypeOrder = GetNodeOrder(thisAnimNode.GetType());
			const int otherTypeOrder = GetNodeOrder(otherAnimNode.GetType());

			if (thisTypeOrder == otherTypeOrder)
			{
				// Same node type, sort by name
				return stricmp(thisAnimNode.GetName(), otherAnimNode.GetName()) < 0;
			}

			return thisTypeOrder < otherTypeOrder;
		}
	case eTVNT_Track:
		const CTrackViewTrack& thisTrack = static_cast<const CTrackViewTrack&>(*this);
		const CTrackViewTrack& otherTrack = static_cast<const CTrackViewTrack&>(otherNode);

		if (thisTrack.GetParameterType() == otherTrack.GetParameterType())
		{
			// Same parameter type, sort by name
			return stricmp(thisTrack.GetName(), otherTrack.GetName()) < 0;
		}

		return thisTrack.GetParameterType() < otherTrack.GetParameterType();
	}

	return false;
}

CTrackViewNode* CTrackViewNode::GetFirstSelectedNode()
{
	if (IsSelected())
	{
		return this;
	}

	const unsigned int numChilds = GetChildCount();
	for (unsigned int childIndex = 0; childIndex < numChilds; ++childIndex)
	{
		CTrackViewNode* pSelectedNode = GetChild(childIndex)->GetFirstSelectedNode();
		if (pSelectedNode)
		{
			return pSelectedNode;
		}
	}

	return nullptr;
}

CTrackViewAnimNode* CTrackViewNode::GetDirector()
{
	for (CTrackViewNode* pCurrentNode = GetParentNode(); pCurrentNode; pCurrentNode = pCurrentNode->GetParentNode())
	{
		if (pCurrentNode->GetNodeType() == eTVNT_AnimNode)
		{
			CTrackViewAnimNode* pParentAnimNode = static_cast<CTrackViewAnimNode*>(pCurrentNode);

			if (pParentAnimNode->GetType() == eAnimNodeType_Director)
			{
				return pParentAnimNode;
			}
		}
		else if (pCurrentNode->GetNodeType() == eTVNT_Sequence)
		{
			return static_cast<CTrackViewAnimNode*>(pCurrentNode);
		}
	}

	return nullptr;
}

void CTrackViewNode::Serialize(Serialization::IArchive& ar)
{
	if (CanBeRenamed())
	{
		if (ar.isInput())
		{
			string name;
			ar(name, "name", "Name");
			SetName(name);
		}
		else
		{
			const string name = GetName();
			ar(name, "name", "Name");
		}
	}

	if (ar.isInput())
	{
		bool bDisabled;
		ar(bDisabled, "disabled", "Disabled");
		SetDisabled(bDisabled);
	}
	else
	{
		bool bDisabled = IsDisabled();
		ar(bDisabled, "disabled", "Disabled");
	}
}
