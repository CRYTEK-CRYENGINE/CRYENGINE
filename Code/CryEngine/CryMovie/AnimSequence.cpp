// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimSequence.h"

#include "EntityNode.h"
#include "CVarNode.h"
#include "ScriptVarNode.h"
#include "AnimCameraNode.h"
#include "SceneNode.h"
#include <CryCore/StlUtils.h>
#include "MaterialNode.h"
#include "EventNode.h"
#include "LayerNode.h"
#include "CommentNode.h"
#include "AnimLightNode.h"
#include "AnimPostFXNode.h"
#include "AnimScreenFaderNode.h"
#include "AnimGeomCacheNode.h"
#include "ShadowsSetupNode.h"
#include "AnimEnvironmentNode.h"
#include "AudioNode.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryScriptSystem/IScriptSystem.h>

CAnimSequence::CAnimSequence(IMovieSystem* pMovieSystem, uint32 id)
{
	m_lastGenId = 1;
	m_pMovieSystem = pMovieSystem;
	m_flags = 0;
	m_pParentSequence = NULL;
	m_timeRange.Set(SAnimTime(0.0f), SAnimTime(10.0f));
	m_bPaused = false;
	m_bActive = false;
	m_pOwner = NULL;
	m_pActiveDirector = NULL;
	m_fixedTimeStep = 0;
	m_precached = false;
	m_bResetting = false;
	m_guid = CryGUID::Create();
	m_id = id;
	m_time = SAnimTime::Min();
}

void CAnimSequence::SetName(const char* name)
{
	string originalName = GetName();

	m_name = name;

	if (GetOwner())
	{
		GetOwner()->OnModified();
	}
}

const char* CAnimSequence::GetName() const
{
	return m_name.c_str();
}

void CAnimSequence::SetFlags(int flags)
{
	m_flags = flags;
}

int CAnimSequence::GetFlags() const
{
	return m_flags;
}

int CAnimSequence::GetCutSceneFlags(const bool localFlags) const
{
	int currentFlags = m_flags & (eSeqFlags_NoUI | eSeqFlags_NoPlayer | eSeqFlags_NoAbort);

	if (m_pParentSequence != NULL)
	{
		if (localFlags == true)
		{
			currentFlags &= ~m_pParentSequence->GetCutSceneFlags();
		}
		else
		{
			currentFlags |= m_pParentSequence->GetCutSceneFlags();
		}
	}

	return currentFlags;
}

void CAnimSequence::SetParentSequence(IAnimSequence* pParentSequence)
{
	m_pParentSequence = pParentSequence;
}

const IAnimSequence* CAnimSequence::GetParentSequence() const
{
	return m_pParentSequence;
}

int CAnimSequence::GetNodeCount() const
{
	return m_nodes.size();
}

IAnimNode* CAnimSequence::GetNode(int index) const
{
	assert(index >= 0 && index < (int)m_nodes.size());
	return m_nodes[index];
}

bool CAnimSequence::AddNode(IAnimNode* pAnimNode)
{
	assert(pAnimNode != 0);

	// Check if this node already in sequence.
	for (int i = 0; i < (int)m_nodes.size(); i++)
	{
		if (pAnimNode == m_nodes[i])
		{
			// Fail to add node second time.
			return false;
		}
	}

	static_cast<CAnimNode*>(pAnimNode)->SetSequence(this);

	pAnimNode->SetTimeRange(m_timeRange);

	m_nodes.push_back(pAnimNode);

	const int nodeId = static_cast<CAnimNode*>(pAnimNode)->GetId();

	if (nodeId >= (int)m_lastGenId)
	{
		m_lastGenId = nodeId + 1;
	}

	if (pAnimNode->NeedToRender())
	{
		AddNodeNeedToRender(pAnimNode);
	}

	bool bNewDirectorNode = m_pActiveDirector == NULL && pAnimNode->GetType() == eAnimNodeType_Director;

	if (bNewDirectorNode)
	{
		m_pActiveDirector = pAnimNode;
	}

	return true;
}

IAnimNode* CAnimSequence::CreateNodeInternal(EAnimNodeType nodeType, uint32 nNodeId)
{
	CAnimNode* pAnimNode = NULL;

	if (nNodeId == -1)
	{
		nNodeId = m_lastGenId;
	}

	switch (nodeType)
	{
	case eAnimNodeType_Entity:
		pAnimNode = new CAnimEntityNode(nNodeId);
		break;

	case eAnimNodeType_Camera:
		pAnimNode = new CAnimCameraNode(nNodeId);
		break;

	case eAnimNodeType_CVar:
		pAnimNode = new CAnimCVarNode(nNodeId);
		break;

	case eAnimNodeType_ScriptVar:
		pAnimNode = new CAnimScriptVarNode(nNodeId);
		break;

	case eAnimNodeType_Director:
		pAnimNode = new CAnimSceneNode(nNodeId);
		break;

	case eAnimNodeType_Material:
		pAnimNode = new CAnimMaterialNode(nNodeId);
		break;

	case eAnimNodeType_Event:
		pAnimNode = new CAnimEventNode(nNodeId);
		break;

	case eAnimNodeType_Group:
		pAnimNode = new CAnimNodeGroup(nNodeId);
		break;

	case eAnimNodeType_Layer:
		pAnimNode = new CLayerNode(nNodeId);
		break;

	case eAnimNodeType_Comment:
		pAnimNode = new CCommentNode(nNodeId);
		break;

	case eAnimNodeType_RadialBlur:
	case eAnimNodeType_ColorCorrection:
	case eAnimNodeType_DepthOfField:
	case eAnimNodeType_HDRSetup:
		pAnimNode = CAnimPostFXNode::CreateNode(nNodeId, nodeType);
		break;

	case eAnimNodeType_ShadowSetup:
		pAnimNode = new CShadowsSetupNode(nNodeId);
		break;

	case eAnimNodeType_ScreenFader:
		pAnimNode = new CAnimScreenFaderNode(nNodeId);
		break;

	case eAnimNodeType_Light:
		pAnimNode = new CAnimLightNode(nNodeId);
		break;

#if defined(USE_GEOM_CACHES)
	case eAnimNodeType_GeomCache:
		pAnimNode = new CAnimGeomCacheNode(nNodeId);
		break;
#endif

	case eAnimNodeType_Environment:
		pAnimNode = new CAnimEnvironmentNode(nNodeId);
		break;

	case eAnimNodeType_Audio:
		pAnimNode = new CAudioNode(nNodeId);
		break;
	}

	if (pAnimNode)
	{
		AddNode(pAnimNode);
	}

	return pAnimNode;
}

IAnimNode* CAnimSequence::CreateNode(EAnimNodeType nodeType)
{
	return CreateNodeInternal(nodeType);
}

IAnimNode* CAnimSequence::CreateNode(XmlNodeRef node)
{
	EAnimNodeType type;
	static_cast<CMovieSystem*>(gEnv->pMovieSystem)->SerializeNodeType(type, node, true, IAnimSequence::kSequenceVersion, 0);

	XmlString name;

	if (!node->getAttr("Name", name))
	{
		return 0;
	}

	IAnimNode* pNewNode = CreateNode(type);

	if (!pNewNode)
	{
		return 0;
	}

	pNewNode->SetName(name);
	pNewNode->Serialize(node, true, true);

	return pNewNode;
}

void CAnimSequence::RemoveNode(IAnimNode* node)
{
	assert(node != 0);

	static_cast<CAnimNode*>(node)->Activate(false);
	static_cast<CAnimNode*>(node)->OnReset();

	for (int i = 0; i < (int)m_nodes.size(); )
	{
		if (node == m_nodes[i])
		{
			m_nodes.erase(m_nodes.begin() + i);

			if (node->NeedToRender())
			{
				RemoveNodeNeedToRender(node);
			}

			continue;
		}

		if (m_nodes[i]->GetParent() == node)
		{
			m_nodes[i]->SetParent(0);
		}

		i++;
	}

	// The removed one was the active director node.
	if (m_pActiveDirector == node)
	{
		// Clear the active one.
		m_pActiveDirector = NULL;

		// If there is another director node, set it as active.
		for (AnimNodes::const_iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
		{
			IAnimNode* pNode = *it;

			if (pNode->GetType() == eAnimNodeType_Director)
			{
				SetActiveDirector(pNode);
				break;
			}
		}
	}
}

void CAnimSequence::RemoveAll()
{
	stl::free_container(m_nodes);
	stl::free_container(m_events);
	stl::free_container(m_nodesNeedToRender);
	m_pActiveDirector = NULL;
}

void CAnimSequence::Reset(bool bSeekToStart)
{
	if (GetFlags() & eSeqFlags_LightAnimationSet)
	{
		return;
	}

	m_precached = false;
	m_bResetting = true;

	if (!bSeekToStart)
	{
		for (AnimNodes::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
		{
			IAnimNode* pAnimNode = *it;
			static_cast<CAnimNode*>(pAnimNode)->OnReset();
		}

		m_bResetting = false;
		return;
	}

	bool bWasActive = m_bActive;

	if (!bWasActive)
	{
		Activate();
	}

	SAnimContext animContext;
	animContext.bSingleFrame = true;
	animContext.bResetting = true;
	animContext.pSequence = this;
	animContext.time = m_timeRange.start;
	Animate(animContext);

	if (!bWasActive)
	{
		Deactivate();
	}
	else
	{
		for (AnimNodes::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
		{
			IAnimNode* pAnimNode = *it;
			static_cast<CAnimNode*>(pAnimNode)->OnReset();
		}
	}

	m_bResetting = false;
}

void CAnimSequence::Pause()
{
	if (GetFlags() & eSeqFlags_LightAnimationSet || m_bPaused)
	{
		return;
	}

	m_bPaused = true;

	// Detach animation block from all nodes in this sequence.
	for (AnimNodes::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		IAnimNode* pAnimNode = *it;
		static_cast<CAnimNode*>(pAnimNode)->OnPause();
	}

	ExecuteAudioTrigger(m_audioTrigger.m_onPauseTrigger);
}

void CAnimSequence::Resume()
{
	if (GetFlags() & eSeqFlags_LightAnimationSet)
	{
		return;
	}

	if (m_bPaused)
	{
		m_bPaused = false;

		for (AnimNodes::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
		{
			IAnimNode* pAnimNode = *it;
			static_cast<CAnimNode*>(pAnimNode)->OnResume();
		}

		ExecuteAudioTrigger(m_audioTrigger.m_onResumeTrigger);
	}
}

bool CAnimSequence::IsPaused() const
{
	return m_bPaused;
}

void CAnimSequence::OnStart()
{
	for (AnimNodes::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		IAnimNode* pAnimNode = *it;
		static_cast<CAnimNode*>(pAnimNode)->OnStart();
	}
}

void CAnimSequence::OnStop()
{
	for (AnimNodes::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		IAnimNode* pAnimNode = *it;
		static_cast<CAnimNode*>(pAnimNode)->OnStop();
	}

	ExecuteAudioTrigger(m_audioTrigger.m_onStopTrigger);
}

void CAnimSequence::StillUpdate()
{
	for (AnimNodes::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		IAnimNode* pAnimNode = *it;
		pAnimNode->StillUpdate();
	}
}

void CAnimSequence::Animate(const SAnimContext& animContext)
{
	assert(m_bActive);

	SAnimContext animContextCopy = animContext;
	animContextCopy.pSequence = this;
	m_time = animContextCopy.time;

	// Evaluate all animation nodes in sequence.
	// The director first.
	if (m_pActiveDirector)
	{
		m_pActiveDirector->Animate(animContextCopy);
	}

	for (AnimNodes::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		// Make sure correct animation block is binded to node.
		IAnimNode* pAnimNode = *it;

		// All other (inactive) director nodes are skipped.
		if (pAnimNode->GetType() == eAnimNodeType_Director)
		{
			continue;
		}

		// If this is a descendant of a director node and that director is currently not active, skip this one.
		IAnimNode* pParentDirector = pAnimNode->HasDirectorAsParent();

		if (pParentDirector && pParentDirector != m_pActiveDirector)
		{
			continue;
		}

		if (pAnimNode->GetFlags() & eAnimNodeFlags_Disabled)
		{
			continue;
		}

		// Animate node.
		pAnimNode->Animate(animContextCopy);
	}
}

void CAnimSequence::Render()
{
	for (AnimNodes::iterator it = m_nodesNeedToRender.begin(); it != m_nodesNeedToRender.end(); ++it)
	{
		IAnimNode* pAnimNode = *it;
		pAnimNode->Render();
	}
}

void CAnimSequence::Activate()
{
	if (m_bActive)
	{
		return;
	}

	m_bActive = true;

	// Assign animation block to all nodes in this sequence.
	for (AnimNodes::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		IAnimNode* pAnimNode = *it;
		static_cast<CAnimNode*>(pAnimNode)->Activate(true);
	}
}

void CAnimSequence::Deactivate()
{
	if (!m_bActive)
	{
		return;
	}

	// Detach animation block from all nodes in this sequence.
	for (AnimNodes::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		IAnimNode* pAnimNode = *it;
		static_cast<CAnimNode*>(pAnimNode)->Activate(false);
		static_cast<CAnimNode*>(pAnimNode)->OnReset();
	}

	// Remove a possibly cached game hint associated with this anim sequence.
	stack_string sTemp("anim_sequence_");
	sTemp += m_name;
	REINST(Release precached sound)
	//gEnv->pAudioSystem->RemoveCachedAudioFile(sTemp.c_str(), false);

	m_bActive = false;
	m_precached = false;
}

void CAnimSequence::PrecacheData(SAnimTime startTime)
{
	PrecacheStatic(startTime);
}

void CAnimSequence::PrecacheStatic(const SAnimTime startTime)
{
	// pre-cache animation keys
	for (AnimNodes::const_iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		IAnimNode* pAnimNode = *it;
		static_cast<CAnimNode*>(pAnimNode)->PrecacheStatic(startTime);
	}

	m_precachedEntitiesSet.clear();

	PrecacheDynamic(startTime);

	if (m_precached)
	{
		return;
	}

	// Try to cache this sequence's game hint if one exists.
	stack_string sTemp("anim_sequence_");
	sTemp += m_name;

	if (gEnv->pAudioSystem)
	{
		// Make sure to use the non-serializable game hint type as trackview sequences get properly reactivated after load

		REINST(Precache sound)
		//gEnv->pAudioSystem->CacheAudioFile(sTemp.c_str(), eAFCT_GAME_HINT);
	}

	gEnv->pLog->Log("=== Precaching render data for cutscene: %s ===", GetName());

	m_precached = true;
}

void CAnimSequence::PrecacheDynamic(SAnimTime time)
{
	// pre-cache animation keys
	for (AnimNodes::const_iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		IAnimNode* pAnimNode = *it;
		static_cast<CAnimNode*>(pAnimNode)->PrecacheDynamic(time);
	}
}

void CAnimSequence::PrecacheEntity(IEntity* pEntity)
{
	if (m_precachedEntitiesSet.find(pEntity) != m_precachedEntitiesSet.end())
	{
		if (IEntityRender* pIEntityRender = pEntity->GetRenderInterface())
		{
			if (IRenderNode* pRenderNode = pIEntityRender->GetRenderNode())
			{
				gEnv->p3DEngine->PrecacheRenderNode(pRenderNode, 4.f);
			}
		}

		m_precachedEntitiesSet.insert(pEntity);
	}
}

void CAnimSequence::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks, uint32 overrideId, bool bResetLightAnimSet)
{
	if (bLoading)
	{
		// Load.
		RemoveAll();

		int sequenceVersion = 0;
		xmlNode->getAttr("SequenceVersion", sequenceVersion);

		if (sequenceVersion > IAnimSequence::kSequenceVersion)
		{
			// Don't load sequences from a newer version of CryEngine
			return;
		}

		TRange<SAnimTime> timeRange;
		m_name = xmlNode->getAttr("Name");
		xmlNode->getAttr("Flags", m_flags);
		timeRange.start.Serialize(xmlNode, bLoading, "startTimeTicks", "StartTime");
		timeRange.end.Serialize(xmlNode, bLoading, "endTimeTicks", "EndTime");
		xmlNode->getAttr("GUIDLo", m_guid.lopart);
		xmlNode->getAttr("GUIDHi", m_guid.hipart);
		xmlNode->getAttr("ID", m_id);
		m_audioTrigger.Serialize(xmlNode, bLoading);

		if (overrideId != 0)
		{
			m_id = overrideId;
		}

		if (bResetLightAnimSet && (GetFlags() & IAnimSequence::eSeqFlags_LightAnimationSet))
		{
			CLightAnimWrapper::InvalidateAllNodes();
			CLightAnimWrapper::SetLightAnimSet(0);
		}

		INDENT_LOG_DURING_SCOPE(true, "Loading sequence '%s' (start time = %.2f, end time = %.2f) %s ID #%u",
		                        m_name.c_str(), timeRange.start, timeRange.end, overrideId ? "override" : "default", m_id);

		// Loading.
		XmlNodeRef nodes = xmlNode->findChild("Nodes");

		if (nodes)
		{
			uint32 id;
			EAnimNodeType nodeType;

			for (int i = 0; i < nodes->getChildCount(); ++i)
			{
				XmlNodeRef childNode = nodes->getChild(i);
				childNode->getAttr("Id", id);

				static_cast<CMovieSystem*>(gEnv->pMovieSystem)->SerializeNodeType(nodeType, childNode, bLoading, sequenceVersion, m_flags);

				if (nodeType == eAnimNodeType_Invalid)
				{
					continue;
				}

				IAnimNode* pAnimNode = CreateNodeInternal((EAnimNodeType)nodeType, id);

				if (!pAnimNode)
				{
					continue;
				}

				pAnimNode->Serialize(childNode, bLoading, bLoadEmptyTracks);
			}

			// When all nodes loaded restore group hierarchy
			for (AnimNodes::const_iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
			{
				CAnimNode* pAnimNode = static_cast<CAnimNode*>((*it).get());
				pAnimNode->PostLoad();

				// And properly adjust the 'm_lastGenId' to prevent the id clash.
				if (pAnimNode->GetId() >= (int)m_lastGenId)
				{
					m_lastGenId = pAnimNode->GetId() + 1;
				}
			}
		}

		XmlNodeRef events = xmlNode->findChild("Events");

		if (events)
		{
			string eventName;

			for (int i = 0; i < events->getChildCount(); i++)
			{
				XmlNodeRef xn = events->getChild(i);
				eventName = xn->getAttr("Name");
				m_events.push_back(eventName);

				// Added
				for (TTrackEventListeners::iterator j = m_listeners.begin(); j != m_listeners.end(); ++j)
				{
					(*j)->OnTrackEvent(this, ITrackEventListener::eTrackEventReason_Added, eventName, NULL);
				}
			}
		}

		// Setting the time range must be done after the loading of all nodes
		// since it sets the time range of tracks, also.
		SetTimeRange(timeRange);
		Deactivate();
		//ComputeTimeRange();

		if (GetOwner())
		{
			GetOwner()->OnModified();
		}

		if (GetFlags() & IAnimSequence::eSeqFlags_LightAnimationSet)
		{
			if (CLightAnimWrapper::GetLightAnimSet())
			{
				CRY_ASSERT_MESSAGE(0, "Track Sequence flagged as LightAnimationSet have LightAnimationSet pointer already set");
			}
			CLightAnimWrapper::SetLightAnimSet(this);
		}
	}
	else
	{
		xmlNode->setAttr("SequenceVersion", IAnimSequence::kSequenceVersion);

		const char* fullname = GetName();
		xmlNode->setAttr("Name", fullname); // Save the full path as a name.
		xmlNode->setAttr("Flags", m_flags);
		m_timeRange.start.Serialize(xmlNode, bLoading, "startTimeTicks", "StartTime");
		m_timeRange.end.Serialize(xmlNode, bLoading, "endTimeTicks", "EndTime");
		xmlNode->setAttr("GUIDLo", m_guid.lopart);
		xmlNode->setAttr("GUIDHi", m_guid.hipart);
		xmlNode->setAttr("ID", m_id);
		m_audioTrigger.Serialize(xmlNode, bLoading);

		// Save.
		XmlNodeRef nodes = xmlNode->newChild("Nodes");
		int num = GetNodeCount();

		for (int i = 0; i < num; i++)
		{
			IAnimNode* pAnimNode = GetNode(i);

			if (pAnimNode)
			{
				XmlNodeRef xn = nodes->newChild("Node");
				pAnimNode->Serialize(xn, bLoading, true);
			}
		}

		XmlNodeRef events = xmlNode->newChild("Events");

		for (TrackEvents::iterator iter = m_events.begin(); iter != m_events.end(); ++iter)
		{
			XmlNodeRef xn = events->newChild("Event");
			xn->setAttr("Name", *iter);
		}
	}
}

void CAnimSequence::SetTimeRange(TRange<SAnimTime> timeRange)
{
	m_timeRange = timeRange;

	// Set this time range for every track in animation.
	// Set time range to be in range of largest animation track.
	for (AnimNodes::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		IAnimNode* anode = *it;
		anode->SetTimeRange(timeRange);
	}
}

void CAnimSequence::ComputeTimeRange()
{
	TRange<SAnimTime> timeRange = m_timeRange;

	// Set time range to be in range of largest animation track.
	for (AnimNodes::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		IAnimNode* pAnimNode = *it;

		int trackCount = pAnimNode->GetTrackCount();

		for (int paramIndex = 0; paramIndex < trackCount; paramIndex++)
		{
			IAnimTrack* pTrack = pAnimNode->GetTrackByIndex(paramIndex);
			int nkey = pTrack->GetNumKeys();

			if (nkey > 0)
			{
				timeRange.start = std::min(timeRange.start, pTrack->GetKeyTime(0));
				timeRange.end = std::max(timeRange.end, pTrack->GetKeyTime(nkey - 1));
			}
		}
	}

	m_timeRange = timeRange;
}

bool CAnimSequence::AddTrackEvent(const char* szEvent)
{
	CRY_ASSERT(szEvent && szEvent[0]);

	if (stl::push_back_unique(m_events, szEvent))
	{
		NotifyTrackEvent(ITrackEventListener::eTrackEventReason_Added, szEvent);
		return true;
	}

	return false;
}

bool CAnimSequence::RemoveTrackEvent(const char* szEvent)
{
	CRY_ASSERT(szEvent && szEvent[0]);

	if (stl::find_and_erase(m_events, szEvent))
	{
		NotifyTrackEvent(ITrackEventListener::eTrackEventReason_Removed, szEvent);
		return true;
	}

	return false;
}

bool CAnimSequence::RenameTrackEvent(const char* szEvent, const char* szNewEvent)
{
	CRY_ASSERT(szEvent && szEvent[0]);
	CRY_ASSERT(szNewEvent && szNewEvent[0]);

	for (size_t i = 0; i < m_events.size(); ++i)
	{
		if (m_events[i] == szEvent)
		{
			m_events[i] = szNewEvent;
			NotifyTrackEvent(ITrackEventListener::eTrackEventReason_Renamed, szEvent, szNewEvent);
			return true;
		}
	}

	return false;
}

bool CAnimSequence::MoveUpTrackEvent(const char* szEvent)
{
	CRY_ASSERT(szEvent && szEvent[0]);

	for (size_t i = 0; i < m_events.size(); ++i)
	{
		if (m_events[i] == szEvent)
		{
			assert(i > 0);

			if (i > 0)
			{
				std::swap(m_events[i - 1], m_events[i]);
				NotifyTrackEvent(ITrackEventListener::eTrackEventReason_MovedUp, szEvent);
			}

			return true;
		}
	}

	return false;
}

bool CAnimSequence::MoveDownTrackEvent(const char* szEvent)
{
	CRY_ASSERT(szEvent && szEvent[0]);

	for (size_t i = 0; i < m_events.size(); ++i)
	{
		if (m_events[i] == szEvent)
		{
			assert(i < m_events.size() - 1);

			if (i < m_events.size() - 1)
			{
				std::swap(m_events[i], m_events[i + 1]);
				NotifyTrackEvent(ITrackEventListener::eTrackEventReason_MovedDown, szEvent);
			}

			return true;
		}
	}

	return false;
}

void CAnimSequence::ClearTrackEvents()
{
	m_events.clear();
}

int CAnimSequence::GetTrackEventsCount() const
{
	return (int)m_events.size();
}

char const* CAnimSequence::GetTrackEvent(int iIndex) const
{
	char const* szResult = NULL;
	const bool bValid = (iIndex >= 0 && iIndex < GetTrackEventsCount());
	CRY_ASSERT(bValid);

	if (bValid)
	{
		szResult = m_events[iIndex];
	}

	return szResult;
}

void CAnimSequence::NotifyTrackEvent(ITrackEventListener::ETrackEventReason reason,
                                     const char* event, const char* param)
{
	// Notify listeners
	for (TTrackEventListeners::iterator j = m_listeners.begin(); j != m_listeners.end(); ++j)
	{
		(*j)->OnTrackEvent(this, reason, event, (void*)param);
	}
}

void CAnimSequence::TriggerTrackEvent(const char* event, const char* param)
{
	NotifyTrackEvent(ITrackEventListener::eTrackEventReason_Triggered, event, param);
}

void CAnimSequence::AddTrackEventListener(ITrackEventListener* pListener)
{
	if (std::find(m_listeners.begin(), m_listeners.end(), pListener) == m_listeners.end())
	{
		m_listeners.push_back(pListener);
	}
}

void CAnimSequence::RemoveTrackEventListener(ITrackEventListener* pListener)
{
	TTrackEventListeners::iterator it = std::find(m_listeners.begin(), m_listeners.end(), pListener);

	if (it != m_listeners.end())
	{
		m_listeners.erase(it);
	}
}

IAnimNode* CAnimSequence::FindNodeById(int nNodeId)
{
	for (AnimNodes::const_iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		IAnimNode* pAnimNode = *it;

		if (static_cast<CAnimNode*>(pAnimNode)->GetId() == nNodeId)
		{
			return pAnimNode;
		}
	}

	return 0;
}

IAnimNode* CAnimSequence::FindNodeByName(const char* sNodeName, const IAnimNode* pParentDirector)
{
	for (AnimNodes::const_iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		IAnimNode* pAnimNode = *it;

		// Case insensitive name comparison.
		if (stricmp(((CAnimNode*)pAnimNode)->GetNameFast(), sNodeName) == 0)
		{
			bool bParentDirectorCheck = pAnimNode->HasDirectorAsParent() == pParentDirector;

			if (bParentDirectorCheck)
			{
				return pAnimNode;
			}
		}
	}

	return 0;
}

bool CAnimSequence::AddNodeNeedToRender(IAnimNode* pNode)
{
	assert(pNode != 0);
	return stl::push_back_unique(m_nodesNeedToRender, pNode);
}

void CAnimSequence::RemoveNodeNeedToRender(IAnimNode* pNode)
{
	assert(pNode != 0);
	stl::find_and_erase(m_nodesNeedToRender, pNode);
}

void CAnimSequence::SetActiveDirector(IAnimNode* pDirectorNode)
{
	assert(pDirectorNode->GetType() == eAnimNodeType_Director);

	if (pDirectorNode->GetType() != eAnimNodeType_Director)
	{
		return;   // It's not a director node.
	}

	if (pDirectorNode->GetSequence() != this)
	{
		return;   // It's not a node belong to this sequence.
	}

	m_pActiveDirector = pDirectorNode;
}

IAnimNode* CAnimSequence::GetActiveDirector() const
{
	return m_pActiveDirector;
}

bool CAnimSequence::IsAncestorOf(const IAnimSequence* pSequence) const
{
	assert(this != pSequence);

	if (this == pSequence)
	{
		return true;
	}

	for (AnimNodes::const_iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		IAnimNode* pNode = *it;

		if (pNode->GetType() == eAnimNodeType_Director)
		{
			IAnimTrack* pSequenceTrack = pNode->GetTrackForParameter(eAnimParamType_Sequence);

			if (pSequenceTrack)
			{
				PREFAST_ASSUME(pSequence);

				for (int i = 0; i < pSequenceTrack->GetNumKeys(); ++i)
				{
					SSequenceKey key;
					pSequenceTrack->GetKey(i, &key);
					if (key.m_sequenceGUID == pSequence->GetGUID())
					{
						return true;
					}

					IAnimSequence* pChild = GetMovieSystem()->FindSequenceByGUID(key.m_sequenceGUID);
					if (pChild && pChild->IsAncestorOf(pSequence))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

void CAnimSequence::ExecuteAudioTrigger(const CryAudio::ControlId audioTriggerId)
{
	if (audioTriggerId != CryAudio::InvalidControlId)
	{
		gEnv->pAudioSystem->ExecuteTrigger(audioTriggerId);
	}
}
