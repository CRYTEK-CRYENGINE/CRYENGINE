// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"
#include "TrackViewSequence.h"
#include "TrackViewSequenceManager.h"
#include "TrackViewAnimNode.h"
#include "TrackViewUndo.h"
#include "TrackViewTrack.h"
#include "TrackViewNodeFactories.h"
#include "TrackViewSequence.h"
#include "TrackViewPlugin.h"
#include "AnimationContext.h"
#include "Objects/ObjectLayer.h"
#include "Objects/EntityObject.h"
#include "Util/Image.h"
#include "Util/Clipboard.h"
#include "IUndoManager.h"

#include <IObjectManager.h>
#include <CrySerialization/Decorators/BitFlags.h>

enum EOutOfRange
{
	eOutOfRange_Once     = 0,
	eOutOfRange_Loop     = IAnimSequence::EAnimSequenceFlags::eSeqFlags_OutOfRangeLoop,
	eOutOfRange_Constant = IAnimSequence::EAnimSequenceFlags::eSeqFlags_OutOfRangeConstant
};
SERIALIZATION_ENUM_BEGIN(EOutOfRange, "Out of Range")
SERIALIZATION_ENUM(eOutOfRange_Once, "once", "Once")
SERIALIZATION_ENUM(eOutOfRange_Loop, "loop", "Loop")
SERIALIZATION_ENUM(eOutOfRange_Constant, "constant", "Constant")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(IAnimSequence, EAnimSequenceFlags, "Sequence Flags")
SERIALIZATION_ENUM(IAnimSequence::EAnimSequenceFlags::eSeqFlags_PlayOnReset, "play_on_reset", "Play on reset")
SERIALIZATION_ENUM(IAnimSequence::EAnimSequenceFlags::eSeqFlags_CutScene, "cut_scene", "Cutscene sequence")
SERIALIZATION_ENUM(IAnimSequence::EAnimSequenceFlags::eSeqFlags_NoUI, "no_ui", "Disable UI")
SERIALIZATION_ENUM(IAnimSequence::EAnimSequenceFlags::eSeqFlags_NoPlayer, "no_player", "Disable player")
SERIALIZATION_ENUM(IAnimSequence::EAnimSequenceFlags::eSeqFlags_NoSeek, "no_seek", "Disable seeking")
SERIALIZATION_ENUM(IAnimSequence::EAnimSequenceFlags::eSeqFlags_NoAbort, "no_abort", "Disable aborting")
SERIALIZATION_ENUM(IAnimSequence::EAnimSequenceFlags::eSeqFlags_NoSpeed, "no_speed", "Disable speed changes")
SERIALIZATION_ENUM(IAnimSequence::EAnimSequenceFlags::eSeqFlags_CanWarpInFixedTime, "can_warp_in_fixed_time", "Timewarping in fixed time step")
SERIALIZATION_ENUM(IAnimSequence::EAnimSequenceFlags::eSeqFlags_EarlyMovieUpdate, "early_movie_update", "Animate before game logic")
SERIALIZATION_ENUM(IAnimSequence::EAnimSequenceFlags::eSeqFlags_NoMPSyncingNeeded, "no_mp_syncing_needed", "No MP synchronization")
SERIALIZATION_ENUM_END()

namespace Private_TrackViewSequence
{
void ForEachNode(CTrackViewNode& node, std::function<void(CTrackViewNode& node)> fun)
{
	fun(node);

	const uint numChildNodes = node.GetChildCount();
	for (uint i = 0; i < numChildNodes; ++i)
	{
		CTrackViewNode* pChildNode = node.GetChild(i);
		ForEachNode(*pChildNode, fun);
	}
}
}

CTrackViewSequence::CTrackViewSequence(IAnimSequence* pSequence)
	: CTrackViewAnimNode(pSequence, nullptr, nullptr)
	, m_bBoundToEditorObjects(false)
	, m_bAnimating(false)
	, m_pAnimSequence(pSequence)
	, m_notificationRecursionLevel(0)
	, m_bLoaded(false)
	, m_bNoNotifications(false)
	, m_bQueueNotifications(false)
	, m_bNodeSelectionChanged(false)
	, m_bForceAnimation(false)
	, m_time(0.0f)
{
	m_playbackRange.start = SAnimTime(-1);
	m_playbackRange.end = SAnimTime(-1);
	assert(m_pAnimSequence);
}

CTrackViewSequence::~CTrackViewSequence()
{
	// For safety. Should be done by sequence manager
	GetIEditor()->GetIUndoManager()->RemoveListener(this);
}

void CTrackViewSequence::Load()
{
	// Don't load sequence again when undo a sequence deletion
	if (m_bLoaded)
	{
		return;
	}

	CTrackViewSequenceNoNotificationContext context(this);

	const int nodeCount = m_pAnimSequence->GetNodeCount();
	for (int i = 0; i < nodeCount; ++i)
	{
		IAnimNode* pNode = m_pAnimSequence->GetNode(i);

		// Only add top level nodes to sequence
		if (!pNode->GetParent())
		{
			CTrackViewAnimNodeFactory animNodeFactory;
			CTrackViewAnimNode* pNewTVAnimNode = animNodeFactory.BuildAnimNode(m_pAnimSequence, pNode, this, nullptr);
			m_childNodes.push_back(std::unique_ptr<CTrackViewNode>(pNewTVAnimNode));
			OnNodeChanged(pNewTVAnimNode, ITrackViewSequenceListener::eNodeChangeType_Added);
		}
	}

	SortNodes();

	m_uidToNodeMap[m_pAnimSequence->GetGUID()] = this;

	m_bLoaded = true;
}

void CTrackViewSequence::BindToEditorObjects()
{
	m_bBoundToEditorObjects = true;
	CTrackViewAnimNode::BindToEditorObjects();
}

void CTrackViewSequence::UnBindFromEditorObjects()
{
	m_bBoundToEditorObjects = false;
	CTrackViewAnimNode::UnBindFromEditorObjects();
}

bool CTrackViewSequence::IsBoundToEditorObjects() const
{
	return m_bBoundToEditorObjects;
}

CTrackViewKeyHandle CTrackViewSequence::FindSingleSelectedKey()
{
	CTrackViewSequence* pSequence = CTrackViewPlugin::GetAnimationContext()->GetSequence();
	if (!pSequence)
	{
		return CTrackViewKeyHandle();
	}

	CTrackViewKeyBundle selectedKeys = pSequence->GetSelectedKeys();

	if (selectedKeys.GetKeyCount() != 1)
	{
		return CTrackViewKeyHandle();
	}

	return selectedKeys.GetKey(0);
}

bool CTrackViewSequence::IsAncestorOf(CTrackViewSequence* pSequence) const
{
	return m_pAnimSequence->IsAncestorOf(pSequence->m_pAnimSequence);
}

bool CTrackViewSequence::IsLayerLocked() const
{
	IObjectLayer* pLayer = GetSequenceObject()->GetLayer();
	return pLayer ? pLayer->IsFrozen() : false;
}

void CTrackViewSequence::BeginCutScene(const bool bResetFx) const
{
	IMovieUser* pMovieUser = GetIEditor()->GetMovieSystem()->GetUser();

	if (pMovieUser)
	{
		pMovieUser->BeginCutScene(m_pAnimSequence, m_pAnimSequence->GetCutSceneFlags(false), bResetFx);
	}
}

void CTrackViewSequence::EndCutScene() const
{
	IMovieUser* pMovieUser = GetIEditor()->GetMovieSystem()->GetUser();

	if (pMovieUser)
	{
		pMovieUser->EndCutScene(m_pAnimSequence, m_pAnimSequence->GetCutSceneFlags(true));
	}
}

void CTrackViewSequence::Render(const SAnimContext& animContext)
{
	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pChildNode = (*iter).get();
		if (pChildNode->GetNodeType() == eTVNT_AnimNode)
		{
			CTrackViewAnimNode* pChildAnimNode = (CTrackViewAnimNode*)pChildNode;
			pChildAnimNode->Render(animContext);
		}
	}

	m_pAnimSequence->Render();
}

void CTrackViewSequence::Animate(const SAnimContext& animContext)
{
	if (!m_pAnimSequence->IsActivated())
	{
		return;
	}

	m_time = animContext.time;
	m_bAnimating = true;

	m_pAnimSequence->Animate(animContext);

	CTrackViewSequenceNoNotificationContext context(this);
	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pChildNode = (*iter).get();
		if (pChildNode->GetNodeType() == eTVNT_AnimNode)
		{
			CTrackViewAnimNode* pChildAnimNode = (CTrackViewAnimNode*)pChildNode;
			pChildAnimNode->Animate(animContext);
		}
	}

	m_bAnimating = false;
}

void CTrackViewSequence::AddListener(ITrackViewSequenceListener* pListener)
{
	stl::push_back_unique(m_sequenceListeners, pListener);
}

void CTrackViewSequence::RemoveListener(ITrackViewSequenceListener* pListener)
{
	stl::find_and_erase(m_sequenceListeners, pListener);
}

void CTrackViewSequence::OnNodeSelectionChanged()
{
	if (m_bNoNotifications)
	{
		return;
	}

	if (m_bQueueNotifications)
	{
		m_bNodeSelectionChanged = true;
	}
	else
	{
		CTrackViewSequenceNoNotificationContext context(this);
		for (auto iter = m_sequenceListeners.begin(); iter != m_sequenceListeners.end(); ++iter)
		{
			(*iter)->OnNodeSelectionChanged(this);
		}
	}
}

void CTrackViewSequence::ForceAnimation()
{
	if (m_bNoNotifications)
	{
		return;
	}

	if (m_bQueueNotifications)
	{
		m_bForceAnimation = true;
	}
	else
	{
		if (IsActive())
		{
			CTrackViewPlugin::GetAnimationContext()->ForceAnimation();
		}
	}
}

void CTrackViewSequence::OnNodeChanged(CTrackViewNode* pNode, ITrackViewSequenceListener::ENodeChangeType type)
{
	switch (type)
	{
	case ITrackViewSequenceListener::eNodeChangeType_Added:
		{
			Private_TrackViewSequence::ForEachNode(*pNode, [&](CTrackViewNode& node)
			{
				const CryGUID nodeGUID = node.GetGUID();
				assert((m_uidToNodeMap.find(nodeGUID) == m_uidToNodeMap.end()) || (m_uidToNodeMap[nodeGUID] == &node));
				m_uidToNodeMap[nodeGUID] = &node;
				pNode->OnAdded();
			});
		}
		break;
	case ITrackViewSequenceListener::eNodeChangeType_Removed:
		{
			Private_TrackViewSequence::ForEachNode(*pNode, [&](CTrackViewNode& node)
			{
				const CryGUID nodeGUID = node.GetGUID();
				assert(m_uidToNodeMap.find(nodeGUID) != m_uidToNodeMap.end());
				m_uidToNodeMap.erase(nodeGUID);
				pNode->OnRemoved();
			});
		}
		break;
	}

	ForceAnimation();

	if (m_bNoNotifications)
	{
		return;
	}

	if (!m_bQueueNotifications)
	{
		CTrackViewSequenceNoNotificationContext context(this);
		for (auto iter = m_sequenceListeners.begin(); iter != m_sequenceListeners.end(); ++iter)
		{
			(*iter)->OnNodeChanged(pNode, type);
		}
	}
	else
	{
		// If node was removed, we need to find all previous queued notifications and remove them
		if (type == ITrackViewSequenceListener::eNodeChangeType_Removed)
		{
			stl::find_and_erase_if(m_queuedNodeChangeNotifications, [=](std::pair<CTrackViewNode*, ITrackViewSequenceListener::ENodeChangeType> entry)
			{
				return (entry.first == pNode) && (entry.second != ITrackViewSequenceListener::eNodeChangeType_Removed);
			});
		}

		stl::push_back_unique(m_queuedNodeChangeNotifications, std::make_pair(pNode, type));

	}
}

void CTrackViewSequence::OnNodeRenamed(CTrackViewNode* pNode, const char* pOldName)
{
	bool bLightAnimationSetActive = GetFlags() & IAnimSequence::eSeqFlags_LightAnimationSet;
	if (bLightAnimationSetActive)
	{
		UpdateLightAnimationRefs(pOldName, pNode->GetName());
	}

	if (m_bNoNotifications)
	{
		return;
	}

	CTrackViewSequenceNoNotificationContext context(this);
	for (auto iter = m_sequenceListeners.begin(); iter != m_sequenceListeners.end(); ++iter)
	{
		(*iter)->OnNodeRenamed(pNode, pOldName);
	}
}

void CTrackViewSequence::OnSequenceSettingsChanged(SSequenceSettingsChangedEventArgs& args)
{
	if (m_bNoNotifications)
	{
		return;
	}

	CTrackViewSequenceNoNotificationContext context(this);
	for (auto iter = m_sequenceListeners.begin(); iter != m_sequenceListeners.end(); ++iter)
	{
		(*iter)->OnSequenceSettingsChanged(this, args);
	}
}

CTrackViewNode* CTrackViewSequence::GetNodeFromGUID(const CryGUID guid) const
{
	return stl::find_in_map(m_uidToNodeMap, guid, nullptr);
}

void CTrackViewSequence::QueueNotifications()
{
	m_bQueueNotifications = true;
	++m_notificationRecursionLevel;
}

void CTrackViewSequence::SubmitPendingNotifcations()
{
	assert(m_notificationRecursionLevel > 0);
	if (m_notificationRecursionLevel > 0)
	{
		--m_notificationRecursionLevel;
	}

	if (m_notificationRecursionLevel == 0)
	{
		m_bQueueNotifications = false;

		{
			CTrackViewSequenceNoNotificationContext context(this);
			for (auto iter = m_sequenceListeners.begin(); iter != m_sequenceListeners.end(); ++iter)
			{
				for (auto changeIter = m_queuedNodeChangeNotifications.begin(); changeIter != m_queuedNodeChangeNotifications.end(); ++changeIter)
				{
					(*iter)->OnNodeChanged(changeIter->first, changeIter->second);
				}
			}
		}

		if (m_bNodeSelectionChanged)
		{
			OnNodeSelectionChanged();
		}

		if (m_bForceAnimation)
		{
			ForceAnimation();
		}

		m_queuedNodeChangeNotifications.clear();
		m_bForceAnimation = false;
		m_bNodeSelectionChanged = false;
	}
}

void CTrackViewSequence::SelectSelectedNodesInViewport()
{
	assert(CUndo::IsRecording());

	CTrackViewAnimNodeBundle selectedNodes = GetSelectedAnimNodes();
	const unsigned int numSelectedNodes = selectedNodes.GetCount();

	std::vector<CBaseObject*> entitiesToBeSelected;

	// Also select objects that refer to light animation
	const bool bLightAnimationSetActive = GetFlags() & IAnimSequence::eSeqFlags_LightAnimationSet;
	if (bLightAnimationSetActive)
	{
		std::vector<string> lightNodes;

		// Construct set of selected light nodes
		for (unsigned int i = 0; i < numSelectedNodes; ++i)
		{
			CTrackViewAnimNode* pCurrentNode = selectedNodes.GetNode(i);
			if (pCurrentNode->GetType() == eAnimNodeType_Light)
			{
				stl::push_back_unique(lightNodes, pCurrentNode->GetName());
			}
		}

		// Check all entities if any is referencing any selected light node
		std::vector<CBaseObject*> entityObjects;
		GetIEditor()->GetObjectManager()->FindObjectsOfType(RUNTIME_CLASS(CEntityObject), entityObjects);

		for (size_t i = 0; i < entityObjects.size(); ++i)
		{
			string lightAnimationName = static_cast<CEntityObject*>(entityObjects[i])->GetLightAnimation();
			if (stl::find(lightNodes, lightAnimationName))
			{
				stl::push_back_unique(entitiesToBeSelected, entityObjects[i]);
			}
		}
	}
	else
	{
		for (unsigned int i = 0; i < numSelectedNodes; ++i)
		{
			CTrackViewAnimNode* pNode = selectedNodes.GetNode(i);
			CEntityObject* pEntity = pNode->GetNodeEntity();
			if (pEntity)
			{
				stl::push_back_unique(entitiesToBeSelected, pEntity);
			}
		}
	}

	for (auto iter = entitiesToBeSelected.begin(); iter != entitiesToBeSelected.end(); ++iter)
	{
		GetIEditor()->SelectObject(*iter);
	}
}

void CTrackViewSequence::SyncSelectedTracksToBase()
{
	CTrackViewAnimNodeBundle selectedNodes = GetSelectedAnimNodes();
	bool bNothingWasSynced = true;

	const unsigned int numSelectedNodes = selectedNodes.GetCount();
	if (numSelectedNodes > 0)
	{
		CUndo undo("Sync selected tracks to base");

		for (unsigned int i = 0; i < numSelectedNodes; ++i)
		{
			CTrackViewAnimNode* pAnimNode = selectedNodes.GetNode(i);
			if (pAnimNode->SyncToBase())
			{
				bNothingWasSynced = false;
			}
		}

		if (bNothingWasSynced)
		{
			undo.Cancel();
		}
	}
}

void CTrackViewSequence::SyncSelectedTracksFromBase()
{
	CTrackViewAnimNodeBundle selectedNodes = GetSelectedAnimNodes();
	bool bNothingWasSynced = true;

	const unsigned int numSelectedNodes = selectedNodes.GetCount();
	if (numSelectedNodes > 0)
	{
		CUndo undo("Sync selected tracks to base");

		for (unsigned int i = 0; i < numSelectedNodes; ++i)
		{
			CTrackViewAnimNode* pAnimNode = selectedNodes.GetNode(i);
			if (pAnimNode->SyncFromBase())
			{
				bNothingWasSynced = false;
			}
		}

		if (bNothingWasSynced)
		{
			undo.Cancel();
		}
	}

	if (IsActive())
	{
		CTrackViewPlugin::GetAnimationContext()->ForceAnimation();
	}
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewSequence::UpdateLightAnimationRefs(const char* szOldName, const char* szNewName)
{
	std::vector<CBaseObject*> entityObjects;
	GetIEditor()->GetObjectManager()->FindObjectsOfType(RUNTIME_CLASS(CEntityObject), entityObjects);
	std::for_each(std::begin(entityObjects), std::end(entityObjects), [&szOldName, &szNewName](CBaseObject* pBaseObject)
	{
		CEntityObject* pEntityObject = static_cast<CEntityObject*>(pBaseObject);
		bool bLight = pEntityObject && pEntityObject->GetEntityClass().Compare("Light") == 0;
		if (bLight)
		{
		  string lightAnimation = pEntityObject->GetEntityPropertyString("lightanimation_LightAnimation");
		  if (lightAnimation == szOldName)
		  {
		    pEntityObject->SetEntityPropertyString("lightanimation_LightAnimation", szNewName);
		  }
		}
	});
}

bool CTrackViewSequence::SetName(const char* pName)
{
	// Check if there is already a sequence with that name or the new name is zero length
	const CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
	if (pSequenceManager->GetSequenceByName(pName) || (strlen(pName) == 0))
	{
		return false;
	}

	string oldName = GetName();
	m_pAnimSequence->SetName(pName);

	if (CUndo::IsRecording())
	{
		CUndo::Record(new CUndoAnimNodeRename(this, oldName));
	}

	GetSequence()->OnNodeRenamed(this, oldName);

	return true;
}

void CTrackViewSequence::CopyKeysToClipboard(const bool bOnlySelectedKeys, const bool bOnlyFromSelectedTracks)
{
	XmlNodeRef copyNode = XmlHelpers::CreateXmlNode("CopyKeysNode");
	CopyKeysToClipboard(copyNode, bOnlySelectedKeys, bOnlyFromSelectedTracks);

	CClipboard clip;
	clip.Put(copyNode, "Track view keys");
}

void CTrackViewSequence::CopyKeysToClipboard(XmlNodeRef& xmlNode, const bool bOnlySelectedKeys, const bool bOnlyFromSelectedTracks)
{
	for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
	{
		CTrackViewNode* pChildNode = (*iter).get();
		pChildNode->CopyKeysToClipboard(xmlNode, bOnlySelectedKeys, bOnlyFromSelectedTracks);
	}
}

void CTrackViewSequence::PasteKeysFromClipboard(CTrackViewAnimNode* pTargetNode, CTrackViewTrack* pTargetTrack, const SAnimTime time)
{
	assert(CUndo::IsRecording());

	CTrackViewSequenceNotificationContext(this);

	CClipboard clipboard;
	XmlNodeRef clipboardContent = clipboard.Get();
	if (clipboardContent)
	{
		std::vector<TMatchedTrackLocation> matchedLocations = GetMatchedPasteLocations(clipboardContent, pTargetNode, pTargetTrack);

		for (auto iter = matchedLocations.begin(); iter != matchedLocations.end(); ++iter)
		{
			const TMatchedTrackLocation& location = *iter;
			CTrackViewTrack* pTrack = location.first;
			const XmlNodeRef& trackNode = location.second;
			pTrack->PasteKeys(trackNode, time);
		}
	}
}

std::vector<CTrackViewSequence::TMatchedTrackLocation>
CTrackViewSequence::GetMatchedPasteLocations(XmlNodeRef clipboardContent, CTrackViewAnimNode* pTargetNode, CTrackViewTrack* pTargetTrack)
{
	std::vector<TMatchedTrackLocation> matchedLocations;

	bool bPastingSingleNode = false;
	XmlNodeRef singleNode;
	bool bPastingSingleTrack = false;
	XmlNodeRef singleTrack;

	// Check if the XML tree only contains one node and if so if that node only contains one track
	for (XmlNodeRef currentNode = clipboardContent; currentNode->getChildCount() > 0; currentNode = currentNode->getChild(0))
	{
		bool bAllChildsAreTracks = true;
		const unsigned int numChilds = currentNode->getChildCount();
		for (unsigned int i = 0; i < numChilds; ++i)
		{
			XmlNodeRef childNode = currentNode->getChild(i);
			if (strcmp(currentNode->getChild(0)->getTag(), "Track") != 0)
			{
				bAllChildsAreTracks = false;
				break;
			}
		}

		if (bAllChildsAreTracks)
		{
			bPastingSingleNode = true;
			singleNode = currentNode;

			if (currentNode->getChildCount() == 1)
			{
				bPastingSingleTrack = true;
				singleTrack = currentNode->getChild(0);
			}
		}
		else if (currentNode->getChildCount() != 1)
		{
			break;
		}
	}

	if (bPastingSingleTrack && pTargetNode && pTargetTrack)
	{
		// We have a target node & track, so try to match the value type
		int valueType = 0;
		if (singleTrack->getAttr("valueType", valueType))
		{
			if (pTargetTrack->GetValueType() == valueType)
			{
				matchedLocations.push_back(TMatchedTrackLocation(pTargetTrack, singleTrack));
				return matchedLocations;
			}
		}
	}

	if (bPastingSingleNode && pTargetNode)
	{
		// Set of tracks that were already matched
		std::vector<CTrackViewTrack*> matchedTracks;

		// We have a single node to paste and have been given a target node
		// so try to match the tracks by param type
		const unsigned int numTracks = singleNode->getChildCount();
		for (unsigned int i = 0; i < numTracks; ++i)
		{
			XmlNodeRef trackNode = singleNode->getChild(i);

			// Try to match the track
			auto matchingTracks = GetMatchingTracks(pTargetNode, trackNode);
			for (auto iter = matchingTracks.begin(); iter != matchingTracks.end(); ++iter)
			{
				CTrackViewTrack* pMatchedTrack = *iter;
				// Pick the first track that was matched *and* was not already matched
				if (!stl::find(matchedTracks, pMatchedTrack))
				{
					stl::push_back_unique(matchedTracks, pMatchedTrack);
					matchedLocations.push_back(TMatchedTrackLocation(pMatchedTrack, trackNode));
					break;
				}
			}
		}

		// Return if matching succeeded
		if (matchedLocations.size() > 0)
		{
			return matchedLocations;
		}
	}

	if (!bPastingSingleNode)
	{
		// Ok, we're pasting keys from multiple nodes, haven't been given any target
		// or matching the targets failed. Ignore given target pointers and start
		// a recursive match at the sequence root.
		GetMatchedPasteLocationsRec(matchedLocations, this, clipboardContent);
	}

	return matchedLocations;
}

std::deque<CTrackViewTrack*> CTrackViewSequence::GetMatchingTracks(CTrackViewAnimNode* pAnimNode, XmlNodeRef trackNode)
{
	std::deque<CTrackViewTrack*> matchingTracks;

	const string trackName = trackNode->getAttr("name");

	CAnimParamType animParamType;
	animParamType.Serialize(trackNode, true);

	int valueType;
	if (!trackNode->getAttr("valueType", valueType))
	{
		return matchingTracks;
	}

	CTrackViewTrackBundle tracks = pAnimNode->GetTracksByParam(animParamType);
	const unsigned int trackCount = tracks.GetCount();

	if (trackCount > 0)
	{
		// Search for a track with the given name and value type
		for (unsigned int i = 0; i < trackCount; ++i)
		{
			CTrackViewTrack* pTrack = tracks.GetTrack(i);

			if (pTrack->GetValueType() == valueType)
			{
				if (pTrack->GetName() == trackName)
				{
					matchingTracks.push_back(pTrack);
				}
			}
		}

		// Then with lower precedence add the tracks that only match the value
		for (unsigned int i = 0; i < trackCount; ++i)
		{
			CTrackViewTrack* pTrack = tracks.GetTrack(i);

			if (pTrack->GetValueType() == valueType)
			{
				stl::push_back_unique(matchingTracks, pTrack);
			}
		}
	}

	return matchingTracks;
}

void CTrackViewSequence::GetMatchedPasteLocationsRec(std::vector<TMatchedTrackLocation>& locations, CTrackViewNode* pCurrentNode, XmlNodeRef clipboardNode)
{
	if (pCurrentNode->GetNodeType() == eTVNT_Sequence)
	{
		if (strcmp(clipboardNode->getTag(), "CopyKeysNode") != 0)
		{
			return;
		}
	}

	const unsigned int numChildNodes = clipboardNode->getChildCount();
	for (unsigned int nodeIndex = 0; nodeIndex < numChildNodes; ++nodeIndex)
	{
		XmlNodeRef xmlChildNode = clipboardNode->getChild(nodeIndex);
		const string tagName = xmlChildNode->getTag();

		if (tagName == "Node")
		{
			const string nodeName = xmlChildNode->getAttr("name");

			int nodeType = eAnimNodeType_Invalid;
			xmlChildNode->getAttr("type", nodeType);

			const unsigned int childCount = pCurrentNode->GetChildCount();
			for (unsigned int i = 0; i < childCount; ++i)
			{
				CTrackViewNode* pChildNode = pCurrentNode->GetChild(i);

				if (pChildNode->GetNodeType() == eTVNT_AnimNode)
				{
					CTrackViewAnimNode* pAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
					if (pAnimNode->GetName() == nodeName && pAnimNode->GetType() == nodeType)
					{
						GetMatchedPasteLocationsRec(locations, pChildNode, xmlChildNode);
					}
				}
			}
		}
		else if (tagName == "Track")
		{
			const string trackName = xmlChildNode->getAttr("name");

			CAnimParamType trackParamType;
			trackParamType.Serialize(xmlChildNode, true);

			int trackParamValue = eAnimValue_Unknown;
			xmlChildNode->getAttr("valueType", trackParamValue);

			const unsigned int childCount = pCurrentNode->GetChildCount();
			for (unsigned int i = 0; i < childCount; ++i)
			{
				CTrackViewNode* pNode = pCurrentNode->GetChild(i);

				if (pNode->GetNodeType() == eTVNT_Track)
				{
					CTrackViewTrack* pTrack = static_cast<CTrackViewTrack*>(pNode);
					if (pTrack->GetName() == trackName && pTrack->GetParameterType() == trackParamType)
					{
						locations.push_back(TMatchedTrackLocation(pTrack, xmlChildNode));
					}
				}
			}
		}
	}
}

bool CTrackViewSequence::SetTimeRange(TRange<SAnimTime> timeRange, bool skipSettingsChanged)
{
	if (timeRange.start >= timeRange.end)
	{
		return false;
	}

	const TRange<SAnimTime> currentTimeRange = GetTimeRange();

	if (CUndo::IsRecording())
	{
		// Store old sequence settings
		CUndo::Record(new CUndoSequenceSettings(this));
	}

	if (m_time < timeRange.start || m_time > timeRange.end)
	{
		m_time = timeRange.start;
		CTrackViewPlugin::GetAnimationContext()->SetTime(timeRange.start);
	}

	m_pAnimSequence->SetTimeRange(timeRange);

	CTrackViewPlugin::GetAnimationContext()->UpdateTimeRange();

	if (!skipSettingsChanged)
	{
		OnSequenceSettingsChanged();
	}

	return true;
}

void CTrackViewSequence::SetPlaybackRange(TRange<SAnimTime> playbackRange, bool skipSettingsChanged)
{
	const TRange<SAnimTime> currentPlaybackRange = GetPlaybackRange();
	if (CUndo::IsRecording())
	{
		// Store old sequence settings
		CUndo::Record(new CUndoSequenceSettings(this));
	}

	const TRange<SAnimTime> currentTimeRange = GetTimeRange();

	if (playbackRange.start < currentTimeRange.start || playbackRange.end > currentTimeRange.end)
		playbackRange.start = currentTimeRange.start;

	if (playbackRange.end > currentTimeRange.end || playbackRange.end < currentTimeRange.start)
		playbackRange.end = currentTimeRange.end;

	m_playbackRange = playbackRange;

	CTrackViewPlugin::GetAnimationContext()->UpdateTimeRange();

	if (!skipSettingsChanged)
	{
		OnSequenceSettingsChanged();
	}
}

TRange<SAnimTime> CTrackViewSequence::GetTimeRange() const
{
	return m_pAnimSequence->GetTimeRange();
}

void CTrackViewSequence::SetFlags(IAnimSequence::EAnimSequenceFlags flags, bool skipSettingsChanged)
{
	const int32 currentFlags = m_pAnimSequence->GetFlags();
	if (CUndo::IsRecording())
	{
		// Store old sequence settings
		CUndo::Record(new CUndoSequenceSettings(this));
	}

	m_pAnimSequence->SetFlags(flags);

	if (!skipSettingsChanged)
	{
		OnSequenceSettingsChanged();
	}
}

IAnimSequence::EAnimSequenceFlags CTrackViewSequence::GetFlags() const
{
	return (IAnimSequence::EAnimSequenceFlags)m_pAnimSequence->GetFlags();
}

void CTrackViewSequence::BeginUndoTransaction()
{
	QueueNotifications();
}

void CTrackViewSequence::EndUndoTransaction()
{
	SubmitPendingNotifcations();
}

void CTrackViewSequence::BeginRestoreTransaction()
{
	QueueNotifications();
}

void CTrackViewSequence::EndRestoreTransaction()
{
	SubmitPendingNotifcations();
}

bool CTrackViewSequence::IsActiveSequence() const
{
	return CTrackViewPlugin::GetAnimationContext()->GetSequence() == this;
}

const SAnimTime CTrackViewSequence::GetTime() const
{
	return m_time;
}

void CTrackViewSequence::Serialize(Serialization::IArchive& ar)
{
	if (ar.isInput())
	{
		bool bChangeRejected = false;

		string name;
		ar(name, "name", "Name");
		SetName(name);

		CTrackViewSequenceNoNotificationContext context(this);

		SAnimTime start;
		ar(start, "start", "Start Time");
		SAnimTime end;
		ar(end, "end", "End Time");
		bChangeRejected |= !SetTimeRange(TRange<SAnimTime>(start, end), true);

		SAnimTime playbackStart;
		ar(playbackStart, "playback_start", "Playback Start Time");
		SAnimTime playbackEnd;
		ar(playbackEnd, "playback_end", "Playback End Time");
		SetPlaybackRange(TRange<SAnimTime>(playbackStart, playbackEnd), true);

		uint32 flags = GetFlags();

		EOutOfRange outOfRange = eOutOfRange_Once;
		ar(outOfRange, "out_of_range", "Out of Range");

		flags &= ~(IAnimSequence::eSeqFlags_OutOfRangeLoop | IAnimSequence::eSeqFlags_OutOfRangeConstant);
		ar(Serialization::BitFlags<IAnimSequence::EAnimSequenceFlags>(flags), "flags", "+Flags");

		switch (outOfRange)
		{
		case eOutOfRange_Loop:
			flags |= IAnimSequence::eSeqFlags_OutOfRangeLoop;
			break;
		case eOutOfRange_Constant:
			flags |= IAnimSequence::eSeqFlags_OutOfRangeConstant;
			break;
		}

		SetFlags(static_cast<IAnimSequence::EAnimSequenceFlags>(flags), true);

		SSequenceAudioTrigger audioTrigger;
		ar(audioTrigger, "audioTrigger", "+Audio Trigger");
		m_pAnimSequence->SetAudioTrigger(audioTrigger);
	}
	else
	{
		const string name = GetName();
		ar(name, "name", "Name");

		const TRange<SAnimTime> timeRange = GetTimeRange();
		ar(timeRange.start, "start", "Start Time");
		ar(timeRange.end, "end", "End Time");

		const TRange<SAnimTime> playbackTimeRange = GetPlaybackRange();
		ar(playbackTimeRange.start, "playback_start", "Playback Start Time");
		ar(playbackTimeRange.end, "playback_end", "Playback End Time");

		uint32 flags = GetFlags();

		EOutOfRange outOfRange = static_cast<EOutOfRange>(flags & (IAnimSequence::eSeqFlags_OutOfRangeLoop | IAnimSequence::eSeqFlags_OutOfRangeConstant));
		ar(outOfRange, "out_of_range", "Out of Range");

		flags &= ~(IAnimSequence::eSeqFlags_OutOfRangeLoop | IAnimSequence::eSeqFlags_OutOfRangeConstant);
		ar(Serialization::BitFlags<IAnimSequence::EAnimSequenceFlags>(flags), "flags", "+Flags");

		ar(m_pAnimSequence->GetAudioTrigger(), "audioTrigger", "+Audio Trigger");
	}
}

