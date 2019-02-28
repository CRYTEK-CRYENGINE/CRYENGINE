// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"
#include "TrackViewAnimNode.h"
#include "TrackViewTrack.h"
#include "TrackViewSequence.h"
#include "TrackViewUndo.h"
#include "TrackViewNodeFactories.h"
#include "TrackViewPlugin.h"
#include "Nodes/TrackViewEntityNode.h"
#include "AnimationContext.h"
#include "Animators/CommentNodeAnimator.h"
#include "Animators/LayerNodeAnimator.h"
#include "Animators/DirectorNodeAnimator.h"
#include "Objects/EntityObject.h"
#include "Objects/CameraObject.h"
#include "Objects/SequenceObject.h"
#include "Objects/ObjectLayerManager.h"
#include "Objects/MiscEntities.h"
#include "Objects/ObjectManager.h"
#include "RenderViewport.h"
#include "Util/Clipboard.h"

void CTrackViewAnimNodeBundle::AppendAnimNode(CTrackViewAnimNode* pNode)
{
	stl::push_back_unique(m_animNodes, pNode);
}

void CTrackViewAnimNodeBundle::AppendAnimNodeBundle(const CTrackViewAnimNodeBundle& bundle)
{
	for (auto iter = bundle.m_animNodes.begin(); iter != bundle.m_animNodes.end(); ++iter)
	{
		AppendAnimNode(*iter);
	}
}

const bool CTrackViewAnimNodeBundle::DoesContain(const CTrackViewNode* pTargetNode)
{
	return stl::find(m_animNodes, pTargetNode);
}

void CTrackViewAnimNodeBundle::Clear()
{
	m_animNodes.clear();
}

CTrackViewAnimNode::CTrackViewAnimNode(IAnimSequence* pSequence, IAnimNode* pAnimNode, CTrackViewNode* pParentNode)
	: CTrackViewNode(pParentNode), m_pAnimSequence(pSequence), m_pAnimNode(pAnimNode), m_pNodeAnimator(nullptr)
{
	if (pAnimNode)
	{
		// Search for child nodes
		const int nodeCount = pSequence->GetNodeCount();
		for (int i = 0; i < nodeCount; ++i)
		{
			IAnimNode* pNode = pSequence->GetNode(i);
			IAnimNode* pParentNode = pNode->GetParent();

			// If our node is the parent, then the current node is a child of it
			if (pAnimNode == pParentNode)
			{
				CTrackViewAnimNodeFactory animNodeFactory;
				CTrackViewAnimNode* pNewTVAnimNode = animNodeFactory.BuildAnimNode(pSequence, pNode, this, nullptr);
				m_childNodes.push_back(std::unique_ptr<CTrackViewNode>(pNewTVAnimNode));

				CTrackViewSequence* pTrackViewSequence = GetSequence();
				if (pTrackViewSequence)
				{
					pTrackViewSequence->OnNodeChanged(pNewTVAnimNode, ITrackViewSequenceListener::eNodeChangeType_Added);
				}
			}
		}

		// Search for tracks
		const int trackCount = pAnimNode->GetTrackCount();
		for (int i = 0; i < trackCount; ++i)
		{
			IAnimTrack* pTrack = pAnimNode->GetTrackByIndex(i);

			CTrackViewTrackFactory trackFactory;
			CTrackViewTrack* pNewTVTrack = trackFactory.BuildTrack(pTrack, this, this);
			AddNode(pNewTVTrack);
		}
	}

	SortNodes();

	switch (GetType())
	{
	case eAnimNodeType_Comment:
		m_pNodeAnimator.reset(new CCommentNodeAnimator(this));
		break;
	case eAnimNodeType_Layer:
		m_pNodeAnimator.reset(new CLayerNodeAnimator());
		break;
	case eAnimNodeType_Director:
		m_pNodeAnimator.reset(new CDirectorNodeAnimator(this));
		break;
	}
}

void CTrackViewAnimNode::BindToEditorObjects()
{
	if (!IsActive())
	{
		return;
	}

	CTrackViewSequenceNotificationContext context(GetSequence());

	CTrackViewAnimNode* pDirector = GetDirector();
	const bool bBelongsToActiveDirector = pDirector ? pDirector->IsActiveDirector() : true;

	if (bBelongsToActiveDirector)
	{
		Bind();

		if (m_pAnimNode)
		{
			m_pAnimNode->SetNodeOwner(this);
		}

		if (m_pNodeAnimator)
		{
			m_pNodeAnimator->Bind(this);
		}

		for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
		{
			CTrackViewNode* pChildNode = (*iter).get();
			if (pChildNode->GetNodeType() == eTVNT_AnimNode)
			{
				CTrackViewAnimNode* pChildAnimNode = (CTrackViewAnimNode*)pChildNode;
				pChildAnimNode->BindToEditorObjects();
			}
		}
	}
}

void CTrackViewAnimNode::UnBindFromEditorObjects()
{
	CTrackViewSequenceNotificationContext context(GetSequence());

	UnBind();

	if (m_pAnimNode)
	{
		m_pAnimNode->SetNodeOwner(nullptr);
	}

	if (m_pNodeAnimator)
	{
		m_pNodeAnimator->UnBind(this);
	}

	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pChildNode = (*iter).get();
		if (pChildNode->GetNodeType() == eTVNT_AnimNode)
		{
			CTrackViewAnimNode* pChildAnimNode = (CTrackViewAnimNode*)pChildNode;
			pChildAnimNode->UnBindFromEditorObjects();
		}
	}
}

bool CTrackViewAnimNode::IsBoundToEditorObjects() const
{
	return m_pAnimNode ? (m_pAnimNode->GetNodeOwner() != NULL) : false;
}

void CTrackViewAnimNode::SyncToConsole(const SAnimContext& animContext)
{
	ConsoleSync(animContext);

	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pChildNode = (*iter).get();
		if (pChildNode->GetNodeType() == eTVNT_AnimNode)
		{
			CTrackViewAnimNode* pChildAnimNode = (CTrackViewAnimNode*)pChildNode;
			pChildAnimNode->SyncToConsole(animContext);
		}
	}
}

CTrackViewAnimNode* CTrackViewAnimNode::CreateSubNode(const string& name, const EAnimNodeType animNodeType, CEntityObject* pEntity)
{
	assert(CUndo::IsRecording());

	const bool bIsGroupNode = IsGroupNode();
	assert(bIsGroupNode);
	if (!bIsGroupNode)
	{
		return nullptr;
	}

	// Check if the node's director or sequence already contains a node with this name
	CTrackViewAnimNode* pDirector = (GetType() == eAnimNodeType_Director) ? this : GetDirector();
	pDirector = pDirector ? pDirector : GetSequence();
	CTrackViewAnimNodeBundle bundle = pDirector->GetAnimNodesByName(name);
	if (bundle.GetCount() > 0)
	{
		if (animNodeType == eAnimNodeType_Entity) // attach entity back to existing animation node
		{
			for (size_t i = 0; i < bundle.GetCount(); i++)
			{
				CTrackViewEntityNode* entityNode = static_cast<CTrackViewEntityNode*>(bundle.GetNode(i));

				entityNode->SetNodeEntity(pEntity);
				entityNode->BindToEditorObjects();
			}
		}
		return nullptr;
	}

	// Create CryMovie and TrackView node
	IAnimNode* pNewAnimNode = m_pAnimSequence->CreateNode(animNodeType);
	if (!pNewAnimNode)
	{
		return nullptr;
	}

	pNewAnimNode->SetName(name);
	pNewAnimNode->CreateDefaultTracks();
	pNewAnimNode->SetParent(m_pAnimNode);

	CTrackViewAnimNodeFactory animNodeFactory;
	CTrackViewAnimNode* pNewNode = animNodeFactory.BuildAnimNode(m_pAnimSequence, pNewAnimNode, this, pEntity);

	pNewAnimNode->SetNodeOwner(pNewNode);
	pNewNode->BindToEditorObjects();

	AddNode(pNewNode);
	CUndo::Record(new CUndoAnimNodeAdd(pNewNode));

	return pNewNode;
}

void CTrackViewAnimNode::RemoveSubNode(CTrackViewAnimNode* pSubNode)
{
	assert(CUndo::IsRecording());

	const bool bIsGroupNode = IsGroupNode();
	assert(bIsGroupNode);
	if (!bIsGroupNode)
	{
		return;
	}

	CUndo::Record(new CUndoAnimNodeRemove(pSubNode));
}

CTrackViewTrack* CTrackViewAnimNode::CreateTrack(const CAnimParamType& paramType)
{
	assert(CUndo::IsRecording());

	if (GetTrackForParameter(paramType) && !(GetParamFlags(paramType) & IAnimNode::eSupportedParamFlags_MultipleTracks))
	{
		return nullptr;
	}

	// Create CryMovie and TrackView track
	IAnimTrack* pNewAnimTrack = m_pAnimNode->CreateTrack(paramType);
	if (!pNewAnimTrack)
	{
		return nullptr;
	}

	CTrackViewTrackFactory trackFactory;
	CTrackViewTrack* pNewTrack = trackFactory.BuildTrack(pNewAnimTrack, this, this);

	AddNode(pNewTrack);
	CUndo::Record(new CUndoTrackAdd(pNewTrack));

	return pNewTrack;
}

void CTrackViewAnimNode::RemoveTrack(CTrackViewTrack* pTrack)
{
	assert(CUndo::IsRecording());
	assert(!pTrack->IsSubTrack());

	if (!pTrack->IsSubTrack())
	{
		CUndo::Record(new CUndoTrackRemove(pTrack));
	}
}

bool CTrackViewAnimNode::SnapTimeToPrevKey(SAnimTime& time) const
{
	const SAnimTime startTime = time;
	SAnimTime closestTrackTime = SAnimTime::Min();
	bool bFoundPrevKey = false;

	for (size_t i = 0; i < m_childNodes.size(); ++i)
	{
		const CTrackViewNode* pNode = m_childNodes[i].get();

		SAnimTime closestNodeTime = startTime;
		if (pNode->SnapTimeToPrevKey(closestNodeTime))
		{
			closestTrackTime = std::max(closestNodeTime, closestTrackTime);
			bFoundPrevKey = true;
		}
	}

	if (bFoundPrevKey)
	{
		time = closestTrackTime;
	}

	return bFoundPrevKey;
}

bool CTrackViewAnimNode::SnapTimeToNextKey(SAnimTime& time) const
{
	const SAnimTime startTime = time;
	SAnimTime closestTrackTime = SAnimTime::Max();
	bool bFoundNextKey = false;

	for (size_t i = 0; i < m_childNodes.size(); ++i)
	{
		const CTrackViewNode* pNode = m_childNodes[i].get();

		SAnimTime closestNodeTime = startTime;
		if (pNode->SnapTimeToNextKey(closestNodeTime))
		{
			closestTrackTime = std::min(closestNodeTime, closestTrackTime);
			bFoundNextKey = true;
		}
	}

	if (bFoundNextKey)
	{
		time = closestTrackTime;
	}

	return bFoundNextKey;
}

CTrackViewKeyBundle CTrackViewAnimNode::GetSelectedKeys()
{
	CTrackViewKeyBundle bundle;

	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		bundle.AppendKeyBundle((*iter)->GetSelectedKeys());
	}

	return bundle;
}

CTrackViewKeyBundle CTrackViewAnimNode::GetAllKeys()
{
	CTrackViewKeyBundle bundle;

	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		bundle.AppendKeyBundle((*iter)->GetAllKeys());
	}

	return bundle;
}

CTrackViewKeyBundle CTrackViewAnimNode::GetKeysInTimeRange(const SAnimTime t0, const SAnimTime t1)
{
	CTrackViewKeyBundle bundle;

	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		bundle.AppendKeyBundle((*iter)->GetKeysInTimeRange(t0, t1));
	}

	return bundle;
}

CTrackViewTrackBundle CTrackViewAnimNode::GetAllTracks()
{
	return GetTracks(false, CAnimParamType());
}

CTrackViewTrackBundle CTrackViewAnimNode::GetSelectedTracks()
{
	return GetTracks(true, CAnimParamType());
}

CTrackViewTrackBundle CTrackViewAnimNode::GetTracksByParam(const CAnimParamType& paramType)
{
	return GetTracks(false, paramType);
}

CTrackViewTrackBundle CTrackViewAnimNode::GetTracks(const bool bOnlySelected, const CAnimParamType& paramType)
{
	CTrackViewTrackBundle bundle;

	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pNode = (*iter).get();

		if (pNode->GetNodeType() == eTVNT_Track)
		{
			CTrackViewTrack* pTrack = static_cast<CTrackViewTrack*>(pNode);

			if (paramType != eAnimParamType_Invalid && pTrack->GetParameterType() != paramType)
			{
				continue;
			}

			if (!bOnlySelected || pTrack->IsSelected())
			{
				bundle.AppendTrack(pTrack);
			}

			const unsigned int subTrackCount = pTrack->GetChildCount();
			for (unsigned int subTrackIndex = 0; subTrackIndex < subTrackCount; ++subTrackIndex)
			{
				CTrackViewTrack* pSubTrack = static_cast<CTrackViewTrack*>(pTrack->GetChild(subTrackIndex));
				if (!bOnlySelected || pSubTrack->IsSelected())
				{
					bundle.AppendTrack(pSubTrack);
				}
			}
		}
		else if (pNode->GetNodeType() == eTVNT_AnimNode)
		{
			CTrackViewAnimNode* pAnimNode = static_cast<CTrackViewAnimNode*>(pNode);
			bundle.AppendTrackBundle(pAnimNode->GetTracks(bOnlySelected, paramType));
		}
	}

	return bundle;
}

EAnimNodeType CTrackViewAnimNode::GetType() const
{
	return m_pAnimNode ? m_pAnimNode->GetType() : eAnimNodeType_Invalid;
}

EAnimNodeFlags CTrackViewAnimNode::GetFlags() const
{
	return m_pAnimNode ? (EAnimNodeFlags)m_pAnimNode->GetFlags() : (EAnimNodeFlags)0;
}

void CTrackViewAnimNode::SetAsActiveDirector()
{
	if (GetType() == eAnimNodeType_Director)
	{
		m_pAnimSequence->SetActiveDirector(m_pAnimNode);

		GetSequence()->UnBindFromEditorObjects();
		GetSequence()->BindToEditorObjects();

		GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_SetAsActiveDirector);
	}
}

bool CTrackViewAnimNode::IsActiveDirector() const
{
	return m_pAnimNode == m_pAnimSequence->GetActiveDirector();
}

bool CTrackViewAnimNode::IsParamValid(const CAnimParamType& param) const
{
	return m_pAnimNode ? m_pAnimNode->IsParamValid(param) : false;
}

CTrackViewTrack* CTrackViewAnimNode::GetTrackForParameter(const CAnimParamType& paramType, uint32 index) const
{
	uint32 currentIndex = 0;

	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pNode = (*iter).get();

		if (pNode->GetNodeType() == eTVNT_Track)
		{
			CTrackViewTrack* pTrack = static_cast<CTrackViewTrack*>(pNode);

			if (pTrack->GetParameterType() == paramType)
			{
				if (currentIndex == index)
				{
					return pTrack;
				}
				else
				{
					++currentIndex;
				}
			}

			if (pTrack->IsCompoundTrack())
			{
				unsigned int numChildTracks = pTrack->GetChildCount();
				for (unsigned int i = 0; i < numChildTracks; ++i)
				{
					CTrackViewTrack* pChildTrack = static_cast<CTrackViewTrack*>(pTrack->GetChild(i));
					if (pChildTrack->GetParameterType() == paramType)
					{
						if (currentIndex == index)
						{
							return pChildTrack;
						}
						else
						{
							++currentIndex;
						}
					}
				}
			}
		}
	}

	return nullptr;
}

void CTrackViewAnimNode::Render(const SAnimContext& animContext)
{
	if (m_pNodeAnimator && IsActive())
	{
		m_pNodeAnimator->Render(this, animContext);
	}

	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pChildNode = (*iter).get();
		if (pChildNode->GetNodeType() == eTVNT_AnimNode)
		{
			CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
			pChildAnimNode->Render(animContext);
		}
	}
}

void CTrackViewAnimNode::Animate(const SAnimContext& animContext)
{
	if (m_pNodeAnimator && IsActive())
	{
		m_pNodeAnimator->Animate(this, animContext);
	}

	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pChildNode = (*iter).get();
		if (pChildNode->GetNodeType() == eTVNT_AnimNode)
		{
			CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
			pChildAnimNode->Animate(animContext);
		}
	}
}

bool CTrackViewAnimNode::SetName(const char* pName)
{
	// Check if the node's director already contains a node with this name
	CTrackViewAnimNode* pDirector = GetDirector();
	pDirector = pDirector ? pDirector : GetSequence();

	CTrackViewAnimNodeBundle nodes = pDirector->GetAnimNodesByName(pName);
	const uint numNodes = nodes.GetCount();
	for (uint i = 0; i < numNodes; ++i)
	{
		if (nodes.GetNode(i) != this)
		{
			return false;
		}
	}

	string oldName = GetName();
	m_pAnimNode->SetName(pName);

	if (CUndo::IsRecording())
	{
		CUndo::Record(new CUndoAnimNodeRename(this, oldName));
	}

	GetSequence()->OnNodeRenamed(this, oldName);

	return true;
}

bool CTrackViewAnimNode::CanBeRenamed() const
{
	return (GetFlags() & eAnimNodeFlags_CanChangeName) != 0;
}

CTrackViewAnimNodeBundle CTrackViewAnimNode::GetAllAnimNodes()
{
	CTrackViewAnimNodeBundle bundle;

	if (GetNodeType() == eTVNT_AnimNode)
	{
		bundle.AppendAnimNode(this);
	}

	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pChildNode = (*iter).get();
		if (pChildNode->GetNodeType() == eTVNT_AnimNode)
		{
			CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
			bundle.AppendAnimNodeBundle(pChildAnimNode->GetAllAnimNodes());
		}
	}

	return bundle;
}

CTrackViewAnimNodeBundle CTrackViewAnimNode::GetSelectedAnimNodes()
{
	CTrackViewAnimNodeBundle bundle;

	if ((GetNodeType() == eTVNT_AnimNode || GetNodeType() == eTVNT_Sequence) && IsSelected())
	{
		bundle.AppendAnimNode(this);
	}

	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pChildNode = (*iter).get();
		if (pChildNode->GetNodeType() == eTVNT_AnimNode)
		{
			CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
			bundle.AppendAnimNodeBundle(pChildAnimNode->GetSelectedAnimNodes());
		}
	}

	return bundle;
}

CTrackViewAnimNodeBundle CTrackViewAnimNode::GetAllOwnedNodes(const CEntityObject* pOwner)
{
	CTrackViewAnimNodeBundle bundle;

	if (IsNodeOwner(pOwner))
	{
		bundle.AppendAnimNode(this);
	}

	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pChildNode = (*iter).get();
		if (pChildNode->GetNodeType() == eTVNT_AnimNode)
		{
			CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
			bundle.AppendAnimNodeBundle(pChildAnimNode->GetAllOwnedNodes(pOwner));
		}
	}

	return bundle;
}

CTrackViewAnimNodeBundle CTrackViewAnimNode::GetAnimNodesByType(EAnimNodeType animNodeType)
{
	CTrackViewAnimNodeBundle bundle;

	if (GetNodeType() == eTVNT_AnimNode && GetType() == animNodeType)
	{
		bundle.AppendAnimNode(this);
	}

	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pChildNode = (*iter).get();
		if (pChildNode->GetNodeType() == eTVNT_AnimNode)
		{
			CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
			bundle.AppendAnimNodeBundle(pChildAnimNode->GetAnimNodesByType(animNodeType));
		}
	}

	return bundle;
}

CTrackViewAnimNodeBundle CTrackViewAnimNode::GetAnimNodesByName(const char* pName)
{
	CTrackViewAnimNodeBundle bundle;

	string nodeName = GetName();
	if (GetNodeType() == eTVNT_AnimNode && stricmp(pName, nodeName) == 0)
	{
		bundle.AppendAnimNode(this);
	}

	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pChildNode = (*iter).get();
		if (pChildNode->GetNodeType() == eTVNT_AnimNode)
		{
			CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
			bundle.AppendAnimNodeBundle(pChildAnimNode->GetAnimNodesByName(pName));
		}
	}

	return bundle;
}

char* CTrackViewAnimNode::GetParamName(const CAnimParamType& paramType) const
{
	const char* pName = m_pAnimNode->GetParamName(paramType);
	return pName ? pName : "";
}

bool CTrackViewAnimNode::IsGroupNode() const
{
	return GetType() == eAnimNodeType_Director || GetType() == eAnimNodeType_Group;
}

string CTrackViewAnimNode::GetAvailableNodeNameStartingWith(const string& name) const
{
	string newName = name;
	unsigned int index = 2;

	while (const_cast<CTrackViewAnimNode*>(this)->GetAnimNodesByName(newName).GetCount() > 0)
	{
		newName.Format("%s %d", name, index);
		++index;
	}

	return newName;
}

CTrackViewAnimNodeBundle CTrackViewAnimNode::AddSelectedEntities()
{
	assert(IsGroupNode());
	assert(CUndo::IsRecording());

	CTrackViewAnimNodeBundle addedNodes;

	// Add selected nodes.
	const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();
	for (int i = 0; i < pSelection->GetCount(); i++)
	{
		CBaseObject* pObject = pSelection->GetObject(i);
		if (!pObject)
		{
			continue;
		}

		// Check if object already assigned to some AnimNode.
		CTrackViewAnimNode* pExistingNode = CTrackViewPlugin::GetSequenceManager()->GetActiveAnimNode(static_cast<const CEntityObject*>(pObject));
		if (pExistingNode)
		{
			// If it has the same director than the current node, reject it
			if (pExistingNode->GetDirector() == GetDirector())
			{
				continue;
			}
		}

		// Get node type (either entity or camera)
		EAnimNodeType nodeType = eAnimNodeType_Invalid;
		CTrackViewAnimNode* pAnimNode = nullptr;
		if (pObject->IsKindOf(RUNTIME_CLASS(CCameraObject)))
		{
			pAnimNode = CreateSubNode(pObject->GetName().GetString(), eAnimNodeType_Camera, static_cast<CEntityObject*>(pObject));
		}
#if defined(USE_GEOM_CACHES)
		else if (pObject->IsKindOf(RUNTIME_CLASS(CGeomCacheEntity)))
		{
			pAnimNode = CreateSubNode(pObject->GetName().GetString(), eAnimNodeType_GeomCache, static_cast<CEntityObject*>(pObject));
		}
#endif
		else if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			pAnimNode = CreateSubNode(pObject->GetName().GetString(), eAnimNodeType_Entity, static_cast<CEntityObject*>(pObject));
		}

		if (pAnimNode)
		{
			addedNodes.AppendAnimNode(pAnimNode);
		}
	}

	return addedNodes;
}

void CTrackViewAnimNode::AddCurrentLayer()
{
	assert(IsGroupNode());

	CObjectLayerManager* pLayerManager = GetIEditor()->GetObjectManager()->GetLayersManager();
	CObjectLayer* pLayer = pLayerManager->GetCurrentLayer();
	const string name = pLayer->GetName();

	CreateSubNode(name, eAnimNodeType_Entity);
}

std::vector<EAnimNodeType> CTrackViewAnimNode::GetAddableNodeTypes() const
{
	std::vector<EAnimNodeType> addableNodeTypes;

	DynArray<EAnimNodeType> nodeTypes = gEnv->pMovieSystem->GetAllNodeTypes();
	const size_t numNodeTypes = nodeTypes.size();
	for (size_t i = 0; i < numNodeTypes; ++i)
	{
		const EAnimNodeType nodeType = nodeTypes[i];
		addableNodeTypes.push_back(nodeType);
	}

	return addableNodeTypes;
}

void CTrackViewAnimNode::RefreshDynamicParams()
{
	if (m_pAnimNode)
		m_pAnimNode->UpdateDynamicParams();
}

unsigned int CTrackViewAnimNode::GetParamCount() const
{
	return m_pAnimNode ? m_pAnimNode->GetParamCount() : 0;
}

CAnimParamType CTrackViewAnimNode::GetParamType(unsigned int index) const
{
	unsigned int paramCount = GetParamCount();
	if (!m_pAnimNode || index >= paramCount)
	{
		return eAnimParamType_Invalid;
	}

	return m_pAnimNode->GetParamType(index);
}

IAnimNode::ESupportedParamFlags CTrackViewAnimNode::GetParamFlags(const CAnimParamType& paramType) const
{
	if (m_pAnimNode)
	{
		return m_pAnimNode->GetParamFlags(paramType);
	}

	return IAnimNode::ESupportedParamFlags(0);
}

EAnimValue CTrackViewAnimNode::GetParamValueType(const CAnimParamType& paramType) const
{
	if (m_pAnimNode)
	{
		return m_pAnimNode->GetParamValueType(paramType);
	}

	return eAnimValue_Unknown;
}

void CTrackViewAnimNode::UpdateDynamicParams()
{
	if (m_pAnimNode)
	{
		m_pAnimNode->UpdateDynamicParams();
	}

	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pChildNode = (*iter).get();
		if (pChildNode->GetNodeType() == eTVNT_AnimNode)
		{
			CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
			pChildAnimNode->UpdateDynamicParams();
		}
	}
}

std::vector<CAnimParamType> CTrackViewAnimNode::GetAddableParams() const
{
	std::vector<CAnimParamType> addableParams;

	const int paramCount = GetParamCount();
	for (int i = 0; i < paramCount; ++i)
	{
		const CAnimParamType paramType = GetParamType(i);
		if (paramType == eAnimParamType_Invalid)
		{
			continue;
		}

		const IAnimNode::ESupportedParamFlags paramFlags = GetParamFlags(paramType);

		CTrackViewTrack* pTrack = GetTrackForParameter(paramType);
		if (pTrack && !(paramFlags & IAnimNode::eSupportedParamFlags_MultipleTracks))
		{
			continue;
		}

		addableParams.push_back(paramType);
	}

	return addableParams;
}

void CTrackViewAnimNode::CopyKeysToClipboard(const bool bOnlySelectedKeys, const bool bOnlyFromSelectedTracks)
{
	XmlNodeRef copyNode = XmlHelpers::CreateXmlNode("CopyKeysNode");
	CopyKeysToClipboard(copyNode, bOnlySelectedKeys, bOnlyFromSelectedTracks);

	CClipboard clip;
	clip.Put(copyNode, "Track view keys");
}

void CTrackViewAnimNode::CopyKeysToClipboard(XmlNodeRef& xmlNode, const bool bOnlySelectedKeys, const bool bOnlyFromSelectedTracks)
{
	XmlNodeRef childNode = xmlNode->createNode("Node");
	childNode->setAttr("name", GetName());
	childNode->setAttr("type", GetType());

	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pChildNode = (*iter).get();
		pChildNode->CopyKeysToClipboard(childNode, bOnlySelectedKeys, bOnlyFromSelectedTracks);
	}

	if (childNode->getChildCount() > 0)
	{
		xmlNode->addChild(childNode);
	}
}

void CTrackViewAnimNode::CopyNodesToClipboard(const bool bOnlySelected)
{
	XmlNodeRef animNodesRoot = XmlHelpers::CreateXmlNode("CopyAnimNodesRoot");

	CopyNodesToClipboardRec(this, animNodesRoot, bOnlySelected);

	CClipboard clipboard;
	clipboard.Put(animNodesRoot, "Track view entity nodes");
}

void CTrackViewAnimNode::CopyNodesToClipboardRec(CTrackViewAnimNode* pCurrentAnimNode, XmlNodeRef& xmlNode, const bool bOnlySelected)
{
	XmlNodeRef childXmlNode = 0;
	if (!bOnlySelected || pCurrentAnimNode->IsSelected())
	{
		childXmlNode = xmlNode->newChild("Node");
		pCurrentAnimNode->m_pAnimNode->Serialize(childXmlNode, false, true);
	}

	for (auto iter = pCurrentAnimNode->m_childNodes.begin(); iter != pCurrentAnimNode->m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pChildNode = (*iter).get();
		if (pChildNode->GetNodeType() == eTVNT_AnimNode)
		{
			CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);

			// If selected and group node, force copying of children
			const bool bSelectedAndGroupNode = pCurrentAnimNode->IsSelected() && pCurrentAnimNode->IsGroupNode();
			CopyNodesToClipboardRec(pChildAnimNode, childXmlNode != 0 ? childXmlNode : xmlNode, !bSelectedAndGroupNode && bOnlySelected);
		}
	}
}

bool CTrackViewAnimNode::PasteNodesFromClipboard()
{
	assert(CUndo::IsRecording());

	CClipboard clipboard;
	if (clipboard.IsEmpty())
	{
		return false;
	}

	XmlNodeRef animNodesRoot = clipboard.Get();
	if (animNodesRoot == NULL || strcmp(animNodesRoot->getTag(), "CopyAnimNodesRoot") != 0)
	{
		return false;
	}

	const bool bLightAnimationSetActive = GetSequence()->GetFlags() & IAnimSequence::eSeqFlags_LightAnimationSet;
	const unsigned int numNodes = animNodesRoot->getChildCount();
	for (int i = 0; i < numNodes; ++i)
	{
		XmlNodeRef xmlNode = animNodesRoot->getChild(i);

		int type;
		if (!xmlNode->getAttr("Type", type))
		{
			continue;
		}

		if (bLightAnimationSetActive && (EAnimNodeType)type != eAnimNodeType_Light)
		{
			// Ignore non light nodes in light animation set
			continue;
		}

		PasteNodeFromClipboard(xmlNode);
	}

	return true;
}

void CTrackViewAnimNode::PasteNodeFromClipboard(XmlNodeRef xmlNode)
{
	const char* pName = xmlNode->getAttr("Name");
	if (!pName)
	{
		return;
	}

	const bool bIsGroupNode = IsGroupNode();
	assert(bIsGroupNode);
	if (!bIsGroupNode)
	{
		return;
	}

	// Check if the node's director or sequence already contains a node with this name
	CTrackViewAnimNode* pDirector = GetDirector();
	pDirector = pDirector ? pDirector : GetSequence();
	if (pDirector->GetAnimNodesByName(pName).GetCount() > 0)
	{
		return;
	}

	// Create CryMovie and TrackView node
	IAnimNode* pNewAnimNode = m_pAnimSequence->CreateNode(xmlNode);
	if (!pNewAnimNode)
	{
		return;
	}

	pNewAnimNode->SetParent(m_pAnimNode);

	CTrackViewAnimNodeFactory animNodeFactory;
	CTrackViewAnimNode* pNewNode = animNodeFactory.BuildAnimNode(m_pAnimSequence, pNewAnimNode, this, nullptr);

	AddNode(pNewNode);
	CUndo::Record(new CUndoAnimNodeAdd(pNewNode));

	const int numChilds = xmlNode->getChildCount();
	for (int i = 0; i < numChilds; ++i)
	{
		XmlNodeRef childNode = xmlNode->getChild(i);
		if (childNode->getAttr("Node"))
		{
			pNewNode->PasteNodeFromClipboard(childNode);
		}
	}
}

bool CTrackViewAnimNode::IsValidReparentingTo(CTrackViewAnimNode* pNewParent)
{
	if ((pNewParent == this) || pNewParent == GetParentNode() || !pNewParent->IsGroupNode())
	{
		return false;
	}

	// Check if the new parent already contains a node with this name
	CTrackViewAnimNodeBundle foundNodes = pNewParent->GetAnimNodesByName(GetName());
	if (foundNodes.GetCount() > 1 || (foundNodes.GetCount() == 1 && foundNodes.GetNode(0) != this))
	{
		return false;
	}

	return true;
}

void CTrackViewAnimNode::SetNewParent(CTrackViewAnimNode* pNewParent)
{
	if (pNewParent == GetParentNode())
	{
		return;
	}

	assert(CUndo::IsRecording());
	assert(IsValidReparentingTo(pNewParent));

	CUndo::Record(new CUndoAnimNodeReparent(this, pNewParent));
}

void CTrackViewAnimNode::SetDisabled(bool bDisabled)
{
	if (m_pAnimNode)
	{
		bool oldState = IsDisabled();
		if (bDisabled)
		{
			m_pAnimNode->SetFlags(m_pAnimNode->GetFlags() | eAnimNodeFlags_Disabled);
			GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_Disabled);
		}
		else
		{
			m_pAnimNode->SetFlags(m_pAnimNode->GetFlags() & ~eAnimNodeFlags_Disabled);
			GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_Enabled);
		}
		m_pAnimNode->Activate(!bDisabled);

		if (CUndo::IsRecording())
		{
			CUndo::Record(new CUndoTrackNodeDisable(this, oldState));
		}
	}
}

bool CTrackViewAnimNode::IsDisabled() const
{
	return m_pAnimNode ? (m_pAnimNode->GetFlags() & eAnimNodeFlags_Disabled) != 0 : false;
}

bool CTrackViewAnimNode::IsActive()
{
	CTrackViewSequence* pSequence = GetSequence();
	const bool bInActiveSequence = pSequence ? GetSequence()->IsBoundToEditorObjects() : false;

	CTrackViewAnimNode* pDirector = GetDirector();
	const bool bMemberOfActiveDirector = pDirector ? GetDirector()->IsActiveDirector() : true;

	return bInActiveSequence && bMemberOfActiveDirector;
}

bool CTrackViewAnimNode::CheckTrackAnimated(const CAnimParamType& paramType) const
{
	if (!m_pAnimNode)
	{
		return false;
	}

	CTrackViewTrack* pTrack = GetTrackForParameter(paramType);
	return pTrack && pTrack->GetKeyCount() > 0;
}

void CTrackViewAnimNode::Serialize(Serialization::IArchive& ar)
{
	CTrackViewNode::Serialize(ar);
}

