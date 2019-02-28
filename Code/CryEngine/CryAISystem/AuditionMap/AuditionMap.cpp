// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "AuditionMap.h"

namespace Perception
{

CAuditionMap::CAuditionMap()
	: m_rayCastManager(*this)
	, m_listenerInstanceIdGenerator(SListenerInstanceId::s_invalidId)
{
}

SListenerInstanceId CAuditionMap::CreateListenerInstanceId(const char* szName)
{
	++m_listenerInstanceIdGenerator;
	while (m_listenerInstanceIdGenerator == SListenerInstanceId::s_invalidId)
		++m_listenerInstanceIdGenerator;

	const uint32 generatedId = m_listenerInstanceIdGenerator;

#if AUDITION_MAP_STORE_DEBUG_NAMES_FOR_LISTENER_INSTANCE_ID
	SListenerInstanceInfo instanceInfo;
	instanceInfo.debugName = szName;
	m_registeredListenerInstanceInfos.insert(RegisteredListenerInstanceInfos::value_type(generatedId, instanceInfo));
#endif

	return SListenerInstanceId(generatedId);
}

void CAuditionMap::ReleaseListenerInstanceId(const SListenerInstanceId& listenerInstanceId)
{
#if AUDITION_MAP_STORE_DEBUG_NAMES_FOR_LISTENER_INSTANCE_ID
	m_registeredListenerInstanceInfos.erase(listenerInstanceId);
#endif
}

bool CAuditionMap::RegisterListener(EntityId entityId, const SListenerParams& listenerParams)
{
	CRY_ASSERT(entityId);

	std::pair<Listeners::iterator, bool> result = m_listeners.insert(Listeners::value_type(entityId, SListener(listenerParams)));
	SListener& listener = result.first->second;
	if (!result.second)
	{
		listener = SListener(listenerParams);
	}
	if (const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId))
	{
		listener.name = pEntity->GetName();
	}
	ReconstructBoundingSphereAroundEars(listener);
	return result.second;
}

void CAuditionMap::UnregisterListner(EntityId entityId)
{
	CRY_ASSERT(entityId);

	m_rayCastManager.CancelAllPendingStimuliDirectedAtListener(entityId);
	m_listeners.erase(entityId);
}

void CAuditionMap::ListenerChanged(EntityId entityId, const SListenerParams& newListenerParams, const uint32 changesMask)
{
	Listeners::iterator it = m_listeners.find(entityId);
	CRY_ASSERT(it != m_listeners.end());
	if (it == m_listeners.end())
		return;

	SListener& currentListener = it->second;
	SListenerParams& currentListenerParams = currentListener.params;

	if (changesMask & ListenerParamsChangeOptions::ChangedListeningDistanceScale)
		currentListenerParams.listeningDistanceScale = newListenerParams.listeningDistanceScale;

	if (changesMask & ListenerParamsChangeOptions::ChangedFactionsToListenMask)
		currentListenerParams.factionsToListenMask = newListenerParams.factionsToListenMask;

	if (changesMask & ListenerParamsChangeOptions::ChangedOnSoundHeardCallback)
		currentListenerParams.onSoundHeardCallback = newListenerParams.onSoundHeardCallback;

	if (changesMask & ListenerParamsChangeOptions::ChangedEars)
	{
		if (currentListenerParams.ears.size() != newListenerParams.ears.size())
		{
			// When the number of ears has changed we should cancel any pending rays because
			// we may otherwise try to deliver a stimulus to an ear that no longer exists.
			// This is not a nice solution but there is no other clean way to solve it,
			// unless we start registering ears with unique IDs (which is a bit of overkill,
			// to be honest).
			m_rayCastManager.CancelAllPendingStimuliDirectedAtListener(entityId);
		}

		// Note: we are going to quietly assume here that ears do not move 'too much'.
		// It is however possible that a ray was casted at an ear that is currently
		// no longer near where it was. This should however not be a serious error because
		// when a pending ray is resolved it will report the stimulus to the
		// ear at the moment the stimulus was actually generated in the past. If this
		// does cause issues then we should perhaps do some distance checks here,
		// and cancel pending rays for ears that have moved too much.
		currentListenerParams.ears = newListenerParams.ears;
		ReconstructBoundingSphereAroundEars(currentListener);
	}
}

SListenerInstanceId CAuditionMap::RegisterGlobalListener(const SGlobalListenerParams& globalListenerParams, const char* name)
{
	SListenerInstanceId listenerInstanceId = CreateListenerInstanceId(name);
	m_globalListeners.insert(GlobalListeners::value_type(listenerInstanceId, SGlobalListener(globalListenerParams)));
	return listenerInstanceId;
}

void CAuditionMap::UnregisterGlobalListener(const SListenerInstanceId& listenerInstanceId)
{
	CRY_ASSERT(listenerInstanceId != SListenerInstanceId());

	ReleaseListenerInstanceId(listenerInstanceId);

	m_globalListeners.erase(listenerInstanceId);
}

void CAuditionMap::Update(float deltaTime)
{
/*
#if (AUDITION_MAP_GENERIC_DEBUGGING != 0)
	m_debugRecentlyCreatedStimuli.Update();
	m_debugRecentlyStimulatedEars.Update();

	IF_UNLIKELY (ai_debugAuditionMap_showCreatedStimuli != 0)
	{
		m_debugRecentlyCreatedStimuli.DebugDraw();
	}
	IF_UNLIKELY (ai_debugAuditionMap_showStimulatedEars != 0)
	{
		m_debugRecentlyStimulatedEars.DebugDraw();
	}

	IF_UNLIKELY (ai_debugAuditionMap_showEars != 0)
	{
		DebugDrawEars();
	}
	IF_UNLIKELY (ai_debugAuditionMap_showBoundingSphereAroundEars != 0)
	{
		DebugDrawBoundingSpheresAroundEars();
	}
	IF_UNLIKELY (ai_debugAuditionMap_showGlobalListenersInfo != 0)
	{
		DebugDrawGlobalListenersInfo();
	}
#endif*/

	m_rayCastManager.Update();
}

void CAuditionMap::DeliverStimulusToListener(const SSoundStimulusParams& soundParams, const SListener& listener, const size_t receivingEarIndex)
{
	const SListenerParams& listenerParams = listener.params;

/*
#if (AUDITION_MAP_GENERIC_DEBUGGING != 0)
	IF_UNLIKELY (ai_debugAuditionMap_showStimulatedEars != 0)
	{
		stack_string debugText;
		if (ai_debugAuditionMap_showAlsoStimuliTags != 0)
		{
			debugText = "Heard by '" + listener.name + "':\n";
			Utility::CMultiTags::TBitNames tagNames;
			if (soundParams.pSoundTags != nullptr)
			{
				tagNames = soundParams.pSoundTags->GetTagNames();
			}
			for (const string& tagName : tagNames)
			{
				debugText += tagName;
				debugText += "\n";
			}
		}

		m_debugRecentlyStimulatedEars.ReportStimulatedEar(listener.params.ears[receivingEarIndex].worldPos, debugText.c_str());
	}
#endif*/

	CRY_ASSERT(listenerParams.onSoundHeardCallback);
	listenerParams.onSoundHeardCallback(soundParams);
}

void CAuditionMap::DeliverStimulusToListener(const SSoundStimulusParams& soundParams, const EntityId listenerEntityId, const size_t receivingEarIndex)
{
	Listeners::iterator foundListenerIter = m_listeners.find(listenerEntityId);
	IF_LIKELY (foundListenerIter != m_listeners.end())
	{
		const SListener& listener = foundListenerIter->second;

		DeliverStimulusToListener(soundParams, listener, receivingEarIndex);
		return;
	}

	CRY_ASSERT(false);   // Trying to deliver stimulus to listener that (no longer) exists?
}

void CAuditionMap::NotifyGlobalListeners_StimulusProcessedAndReachedOneOrMoreEars(const SSoundStimulusParams& soundParams) const
{
	GlobalListeners::const_iterator globalListenersEndIter = m_globalListeners.end();
	for (GlobalListeners::const_iterator globalListenersIter = m_globalListeners.begin();
	     globalListenersIter != globalListenersEndIter;
	     ++globalListenersIter)
	{
		const SGlobalListener& globalListener = globalListenersIter->second;

		globalListener.params.onSoundCompletelyProcessedAndReachedOneOrMoreEars(soundParams);
	}
}

float CAuditionMap::ComputeStimulusRadius(const SListener& listener, const SSoundStimulusParams& soundParams) const
{
	return soundParams.radius * listener.params.listeningDistanceScale;
}

// This function is used as an early-out decider: it merely checks if the listener
// is interested in the sound and if potentially near enough.
bool CAuditionMap::ShouldListenerAcknowledgeSound(const SListener& listener, const SSoundStimulusParams& soundParams) const
{
	if(soundParams.faction == IFactionMap::InvalidFactionID)
		return false;

	const SListenerParams& listenerParams = listener.params;
	const bool matchesFaction = (listenerParams.factionsToListenMask & (1 << soundParams.faction)) != 0;
	if (!matchesFaction)
		return false;

	const float stimulusRadius = ComputeStimulusRadius(listener, soundParams);
	const float distSqr = (listener.boundingSphereAroundEars.center - soundParams.position).GetLengthSquared();
	if (distSqr > sqr(stimulusRadius - listener.boundingSphereAroundEars.radius))
		return false;

	if (listener.params.userConditionCallback)
	{
		if (!listener.params.userConditionCallback(soundParams))
			return false;
	}
	return true;
}

bool CAuditionMap::IsAnyEarWithinStimulusRange(const SListener& listener, const SSoundStimulusParams& soundParams, size_t* pAnyReceivingEarIndex /* = nullptr */) const
{
	const size_t earsAmount = listener.params.ears.size();
	IF_UNLIKELY (earsAmount == (size_t)0)
	{
		return false;
	}

	const float stimulusRadius = ComputeStimulusRadius(listener, soundParams);
	const float stimulusRadiusSq = stimulusRadius * stimulusRadius;

	const Vec3& stimulusCenterPos = soundParams.position;

	for (size_t earIndex = (size_t)0; earIndex < earsAmount; earIndex++)
	{
		const SListenerEarParams& earParams = listener.params.ears[earIndex];

		const float distanceToEarSq = (earParams.worldPos - stimulusCenterPos).GetLengthSquared();
		if (distanceToEarSq <= stimulusRadiusSq)
		{
			if (pAnyReceivingEarIndex != nullptr)
			{
				*pAnyReceivingEarIndex = earIndex;
			}
			return true;
		}
	}
	return false;
}

void CAuditionMap::SetSoundObstructionOnHitCallback(const IAuditionMap::SoundObstructionOnHitCallback& callback)
{
	m_soundObstructionOnHitCallback = callback;
}

void CAuditionMap::OnSoundEvent(const SSoundStimulusParams& soundParams, const char* szDebugText)
{
	/*
	#if (AUDITION_MAP_GENERIC_DEBUGGING != 0)
	{
	stack_string debugText;
	IF_UNLIKELY (ai_debugAuditionMap_showAlsoStimuliTags != 0)
	{
	debugText += "-- Stimulus Created --\n";
	IF_UNLIKELY (pDebugInfo)
	{
	if (pDebugInfo->szReason)
	{
	debugText += "Reason: ";
	debugText += pDebugInfo->szReason;
	debugText += '\n';
	}
	if (pDebugInfo->pSelectorInfo)
	{
	debugText += stack_string().Format("Selector: '%s' - '%s'\n", pDebugInfo->pSelectorInfo->szSelectorLibrary, pDebugInfo->pSelectorInfo->szSelectorLabel);
	}
	}
	Utility::CMultiTags::TBitNames tagNames;
	if (soundParams.pSoundTags != nullptr)
	{
	tagNames = soundParams.pSoundTags->GetTagNames();
	}
	for (const string& tagName : tagNames)
	{
	debugText += tagName;
	debugText += "\n";
	}
	}
	m_debugRecentlyCreatedStimuli.ReportCreatedStimulus(Sphere(soundParams.position, soundParams.radius), debugText.c_str());
	}
	#endif*/

	switch (soundParams.obstructionHandling)
	{
	case EStimulusObstructionHandling::IgnoreAllObstructions:
		OnSoundEvent_IgnoreAllObstructionsHandling(soundParams);
		return;

	case EStimulusObstructionHandling::RayCastWithLinearFallOff:
		OnSoundEvent_RayCastWithLinearFallOff(soundParams);
		return;
	default:
		break;
	}
	static_assert((int)EStimulusObstructionHandling::Count == 2, "Unexpected enum count");
}

void CAuditionMap::OnSoundEvent_IgnoreAllObstructionsHandling(const SSoundStimulusParams& soundParams)
{
	bool bReachedOneOrMoreEars = false;

	Listeners::const_iterator listenerEndIter = m_listeners.end();
	for (Listeners::const_iterator listenerIter = m_listeners.begin(); listenerIter != listenerEndIter; ++listenerIter)
	{
		const SListener& listener = listenerIter->second;
		if (ShouldListenerAcknowledgeSound(listener, soundParams))
		{
			size_t anyReceivingEarIndex;
			if (IsAnyEarWithinStimulusRange(listener, soundParams, &anyReceivingEarIndex))
			{
				DeliverStimulusToListener(soundParams, listener, anyReceivingEarIndex);

				bReachedOneOrMoreEars = true;
			}
		}
	}

	if (bReachedOneOrMoreEars)
	{
		NotifyGlobalListeners_StimulusProcessedAndReachedOneOrMoreEars(soundParams);
	}
}

void CAuditionMap::OnSoundEvent_RayCastWithLinearFallOff(const SSoundStimulusParams& soundParams)
{
	const float soundLinearFallOffFactor = AuditionHelpers::CAuditionMapRayCastManager::ComputeSoundLinearFallOffFactor(soundParams.radius);
	AuditionHelpers::SPendingStimulusParamsHelper pendingStimulusParamsHelper(&m_rayCastManager, soundParams);

	Listeners::const_iterator listenerEndIter = m_listeners.end();
	for (Listeners::const_iterator listenerIter = m_listeners.begin(); listenerIter != listenerEndIter; ++listenerIter)
	{
		const EntityId listenerEntityId = listenerIter->first;
		const SListener& listener = listenerIter->second;

		if (ShouldListenerAcknowledgeSound(listener, soundParams))
		{
			if (IsAnyEarWithinStimulusRange(listener, soundParams))
			{
				m_rayCastManager.QueueRaysBetweenStimulusAndListener(soundParams.position, pendingStimulusParamsHelper.GetStimulusIndex(), soundLinearFallOffFactor, listenerEntityId, listener.params);
			}
		}
	}
}

void CAuditionMap::ReconstructBoundingSphereAroundEars(SListener& listener)
{
	if (listener.params.ears.empty())
	{
		listener.boundingSphereAroundEars = Sphere(Vec3(ZERO), 0.0f);
		return;
	}

	// Find the center of the 'point cloud' and use that as the center of the bounding sphere.
	AABB aabb(AABB::RESET);
	for (const SListenerEarParams& earParams : listener.params.ears)
	{
		aabb.Add(earParams.worldPos);
	}
	const Vec3 sphereCenterPos = aabb.GetCenter();

	float sphereRadiusSq = 0.0f;
	for (const SListenerEarParams& earParams : listener.params.ears)
	{
		const float distanceToCenterSq = (earParams.worldPos - sphereCenterPos).GetLengthSquared();
		if (distanceToCenterSq > sphereRadiusSq)
		{
			sphereRadiusSq = distanceToCenterSq;
		}
	}

	const float numericalNoiseBufferDistance = 0.05f;

	listener.boundingSphereAroundEars.center = sphereCenterPos;
	listener.boundingSphereAroundEars.radius = sqrtf(sphereRadiusSq) + numericalNoiseBufferDistance;
}

/*
#if (AUDITION_MAP_GENERIC_DEBUGGING != 0)

void CAuditionMap::DebugDrawEars() const
{
	Listeners::const_iterator listenerEndIter = m_listeners.end();
	for (Listeners::const_iterator listenerIter = m_listeners.begin(); listenerIter != listenerEndIter; ++listenerIter)
	{
		const EntityId listenerEntityId = listenerIter->first;
		const SListener& listener = listenerIter->second;
		DebugDrawEarsForListener(listenerEntityId, listener);
	}
}

void CAuditionMap::DebugDrawEarsForListener(const EntityId listenerEntityId, const SListener& listener) const
{
	IRenderer* pRenderer = gEnv->pRenderer;
	if (pRenderer == nullptr)
	{
		return;
	}
	IRenderAuxGeom* pRenderAuxGeom = pRenderer->GetIRenderAuxGeom();
	if (pRenderAuxGeom == nullptr)
	{
		return;
	}

	Vec3 entityNamePos;

	const IEntity* pListenerEntity = gEnv->pEntitySystem->GetEntity(listenerEntityId);
	if (pListenerEntity != nullptr)
	{
		entityNamePos = DebugDrawEntityNameAndGetDrawPos(pRenderAuxGeom, pListenerEntity);
	}

	const float earMarkerRadius = 0.15f;
	const ColorF earMarkerColor = Col_Pink;
	const bool bDrawShaded = false;

	for (const SListenerEarParams& earParams : listener.params.ears)
	{
		const Vec3& earWorldPos = earParams.worldPos;

		pRenderAuxGeom->DrawSphere(earWorldPos, earMarkerRadius, earMarkerColor, bDrawShaded);

		if (pListenerEntity != nullptr)
		{
			pRenderAuxGeom->DrawLine(entityNamePos, earMarkerColor, earWorldPos, earMarkerColor);
		}
	}
}

void CAuditionMap::DebugDrawBoundingSpheresAroundEars() const
{
	Listeners::const_iterator listenerEndIter = m_listeners.end();
	for (Listeners::const_iterator listenerIter = m_listeners.begin(); listenerIter != listenerEndIter; ++listenerIter)
	{
		const EntityId listenerEntityId = listenerIter->first;
		const SListener& listener = listenerIter->second;
		DebugDrawBoundingSpheresAroundEarsForListener(listenerEntityId, listener);
	}
}

void CAuditionMap::DebugDrawBoundingSpheresAroundEarsForListener(const EntityId listenerEntityId, const SListener& listener) const
{
	IRenderer* pRenderer = gEnv->pRenderer;
	if (pRenderer == nullptr)
	{
		return;
	}
	IRenderAuxGeom* pRenderAuxGeom = pRenderer->GetIRenderAuxGeom();
	if (pRenderAuxGeom == nullptr)
	{
		return;
	}

	Vec3 entityNamePos;

	const IEntity* pListenerEntity = gEnv->pEntitySystem->GetEntity(listenerEntityId);
	if (pListenerEntity != nullptr)
	{
		entityNamePos = DebugDrawEntityNameAndGetDrawPos(pRenderAuxGeom, pListenerEntity);
	}

	const ColorF boundingSphereColor = Col_SeaGreen;
	const bool bDrawShaded = false;

	pRenderAuxGeom->DrawSphere(listener.boundingSphereAroundEars.center, listener.boundingSphereAroundEars.radius, boundingSphereColor, bDrawShaded);
	if (pListenerEntity != nullptr)
	{
		pRenderAuxGeom->DrawLine(entityNamePos, boundingSphereColor, listener.boundingSphereAroundEars.center, boundingSphereColor);
	}
}

Vec3 CAuditionMap::DebugDrawEntityNameAndGetDrawPos(IRenderAuxGeom* pRenderAuxGeom, const IEntity* pEntity)
{
	SDrawTextInfo drawTextInfo;
	drawTextInfo.flags |= eDrawText_Center | eDrawText_CenterV | eDrawText_800x600;
	drawTextInfo.color[0] = 1.0f;
	drawTextInfo.color[1] = 1.0f;
	drawTextInfo.color[2] = 1.0f;
	drawTextInfo.color[3] = 1.0f;
	drawTextInfo.scale = Vec2(1.5f);

	const Vec3 nameOffset(0.0f, 0.0f, 0.5f);

	const Vec3 entityNamePos = pEntity->GetWorldPos() + nameOffset;
	pRenderAuxGeom->RenderTextQueued(entityNamePos, drawTextInfo, pEntity->GetName());

	return entityNamePos;
}

void CAuditionMap::DebugDrawGlobalListenersInfo() const
{
	IRenderer* pRenderer = gEnv->pRenderer;
	if (pRenderer == nullptr)
	{
		return;
	}

	stack_string text;

	Vec2 topleftScreenPos(0.1f, 0.1f);

	DebugDrawTextLineFeed(&topleftScreenPos, "Audition Map - Global Listeners", Col_Yellow);

	if (m_globalListeners.empty())
	{
		DebugDrawTextLineFeed(&topleftScreenPos, "- None -");
	}
	else
	{
		GlobalListeners::const_iterator globalListenersEndIter = m_globalListeners.end();
		for (GlobalListeners::const_iterator globalListenersIter = m_globalListeners.begin();
		     globalListenersIter != globalListenersEndIter;
		     ++globalListenersIter)
		{
			const SListenerInstanceId& listenerInstanceId = globalListenersIter->first;

			stack_string listenerDescText;
			RetrieveRegisteredListenerDebugDescText(listenerInstanceId, &listenerDescText);

			text.Format("name = '%s'   instanceID = %u", listenerDescText.c_str(), (unsigned int)listenerInstanceId);

			DebugDrawTextLineFeed(&topleftScreenPos, text.c_str());
		}
	}
}

// Screen space: [0.0f .. 1.0f].
void CAuditionMap::DebugDrawText2D(IRenderer* pRenderer, const Vec2& screenSpacePos, const float fontSize, const ColorF color, const char* pText)
{
	IF_UNLIKELY (pRenderer == NULL)
	{
		return;
	}

	const int flags = eDrawText_2D | eDrawText_800x600 | eDrawText_FixedSize;

	const Vec3 textPos(screenSpacePos.x * pRenderer->GetWidth(), screenSpacePos.y * pRenderer->GetHeight(), 0.5f); // (The debug text drawing is broken? Seems to take the actual screen dimensions instead of the 800x600 we requested)?
	IRenderAuxText::DrawText(textPos, fontSize, color, flags, pText);
}

void CAuditionMap::DebugDrawTextLineFeed(Vec2* pTopLeftScreenPos, const char* szText, const ColorF textColor / * = Col_White * /)
{
	const float fontSize = 1.5f;
	const float textLineHeight = 0.02f;

	DebugDrawText2D(gEnv->pRenderer, *pTopLeftScreenPos, fontSize, textColor, szText);

	pTopLeftScreenPos->y += textLineHeight;
}

void CAuditionMap::RetrieveRegisteredListenerDebugDescText(const SListenerInstanceId listenerInstanceId, stack_string* pResultDescText) const
{
	assert(pResultDescText != nullptr);

	RegisteredListenerInstanceInfos::const_iterator foundIter = m_registeredListenerInstanceInfos.find(listenerInstanceId);
	IF_LIKELY (foundIter != m_registeredListenerInstanceInfos.end())
	{
		// TODO: Later on we could add special listener instance information for entities for example.
		//       The name we return here could then be just the entity name?
		const SListenerInstanceInfo& listenerInstanceInfo = foundIter->second;

	#if (AUDITION_MAP_STORE_DEBUG_NAMES_FOR_LISTENER_INSTANCE_ID != 0)
		*pResultDescText = listenerInstanceInfo.debugName.c_str();
		return;
	#endif

		*pResultDescText = "<UNKNOWN>";
		return;
	}

	*pResultDescText = "<ERROR: UNREGISTERED LISTENER!>";
}

#endif // (AUDITION_MAP_GENERIC_DEBUGGING != 0)*/

} // namespace AuditionMap
