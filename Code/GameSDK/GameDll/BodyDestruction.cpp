// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Body destructibility for actors (ported from original lua version)

-------------------------------------------------------------------------
History:
- 27:07:2010   Created by Benito Gangoso Rodriguez

*************************************************************************/

#include "StdAfx.h"
#include "BodyDestruction.h"
#include "GameRules.h"
#include "Utility/CryHash.h"
#include "BodyManagerCVars.h"

#include <CryAnimation/ICryAnimation.h>

#include "EntityUtility/EntityScriptCalls.h"
#include "EntityUtility/EntityEffects.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CBodyDestructibilityProfile::Reload( SBodyDestructibilityDef& bodyDestructionDef )
{
	m_replacementMaterials.clear();
	m_cachedCharacterInstaces.clear();

	m_attachments.clear();
	m_bones.clear();
	m_destructionEvents.clear();
	m_healthRatioEvents.clear();

	m_nonGibDeath = SExplosionDeathEvent();
	m_gibDeath = SExplosionDeathEvent();
	m_mikeDeath = SMikeDeath();

	m_destructibleAttachmentCount = 0;
	m_initialized = false;
	m_arelevelResourcesCached = false;

	LoadXmlInfo(bodyDestructionDef);

}

void CBodyDestructibilityProfile::LoadXmlInfo( SBodyDestructibilityDef& bodyDestructionDef )
{
	XmlNodeRef rootNode = GetISystem()->LoadXmlFromFile(bodyDestructionDef.bodyDestructibilityFileName.c_str());
	if (rootNode)
	{
		CryLog("Loading body destruction file '%s'", bodyDestructionDef.bodyDestructibilityFileName.c_str());

		SParsingHelper parsingHelper;

		PreparePartsAndEventsBeforeLoad(rootNode, parsingHelper);

		LoadDestructibleParts(rootNode->findChild("DestructibleParts"), parsingHelper);
		LoadEvents(rootNode->findChild("Events"), parsingHelper);
		LoadHealthRatioEvents(rootNode->findChild("HealthRatioEvents"), parsingHelper);
		LoadExplosionDeaths(rootNode->findChild("ExplosionDeaths"), parsingHelper);
		LoadMikeDeath(rootNode->findChild("MikeDeath"), parsingHelper);

		CryLog("======== Loading body destruction done ==================");

	}
	else
	{
		GameWarning("BodyDestructibility: Failed to load '%s'", bodyDestructionDef.bodyDestructibilityFileName.c_str());
	}

	m_initialized = true;
}

void CBodyDestructibilityProfile::PreparePartsAndEventsBeforeLoad( const XmlNodeRef& rootNode, SParsingHelper& parsingHelper )
{
	CRY_ASSERT(rootNode != NULL);

	typedef std::vector<string> TPartNames;

	TPartNames	boneList;
	TPartNames attachmentList;

	//Destructibles
	XmlNodeRef destructiblesNode = rootNode->findChild("DestructibleParts");
	if (destructiblesNode)
	{
		const int destructibleCount = destructiblesNode->getChildCount();
		for (int i = 0; i < destructibleCount; ++i)
		{
			XmlNodeRef partNode = destructiblesNode->getChild(i);
			CRY_ASSERT(partNode != NULL);

			const char* partName = partNode->getAttr("name");
			if (partName)
			{
				if (strcmp(partNode->getTag(), "Attachment") == 0)
				{
					stl::push_back_unique(attachmentList, string(partName));
				}
				else if (strcmp(partNode->getTag(), "Bone") == 0)
				{
					stl::push_back_unique(boneList, string(partName));
				}
			}
			else
			{
				GameWarning("Destructible part has no 'name' at line %d", partNode->getLine());
			}
		}
	}

	//Events
	XmlNodeRef eventsNode = rootNode->findChild("Events");
	if (eventsNode)
	{
		SDestructionEvent destructionEvent;

		const int eventCount = eventsNode->getChildCount();
		m_destructionEvents.reserve(eventCount);	

		for (int eventIndex = 0; eventIndex < eventCount; ++eventIndex)
		{
			XmlNodeRef eventNode = eventsNode->getChild(eventIndex);
			CRY_ASSERT(eventNode != NULL);

			// Insert events with names
			const char* eventName = eventNode->getAttr("name");
			if (eventName)
			{
				destructionEvent.eventId = CryStringUtils::HashString(eventName);
				destructionEvent.name = eventName;

				if (!stl::find(m_destructionEvents, destructionEvent))
				{
					m_destructionEvents.push_back(destructionEvent);
					parsingHelper.eventIndices.insert(TNameHashToIdxMap::value_type(destructionEvent.eventId, (int)(m_destructionEvents.size() - 1)));
				}
				else
				{
					GameWarning("Event %s defined already defined (Line %d)", eventName, eventNode->getLine());
				}

				// Gather new attachments from event list
				XmlNodeRef attachmentsToHide = eventNode->findChild("AttachmentsToHide");
				if (attachmentsToHide)
				{
					const int numOfAttachments = attachmentsToHide->getChildCount();
					for (int i = 0; i < numOfAttachments; ++i)
					{
						XmlNodeRef attachmentNode = attachmentsToHide->getChild(i);
						CRY_ASSERT(attachmentNode != NULL);

						const char* attachmentName = attachmentNode->getAttr("name");
						if (attachmentName)
						{
							stl::push_back_unique(attachmentList, string(attachmentName));
						}
						else
						{
							GameWarning("Event '%s' referencing attachment to hide with no 'name' at line %d", eventName, attachmentNode->getLine());
						}
					}
				}

				XmlNodeRef attachmentsToUnhide = eventNode->findChild("AttachmentToUnhide");
				if (attachmentsToUnhide)
				{
					const int numOfAttachments = attachmentsToUnhide->getChildCount();
					for (int i = 0; i < numOfAttachments; ++i)
					{
						XmlNodeRef attachmentNode = attachmentsToUnhide->getChild(i);
						CRY_ASSERT(attachmentNode != NULL);

						const char* attachmentName = attachmentNode->getAttr("name");
						if (attachmentName)
						{
							stl::push_back_unique(attachmentList, string(attachmentName));
						}
						else
						{
							GameWarning("Event '%s' referencing attachment to unhide with no 'name' at line %d", eventName, attachmentNode->getLine());
						}
					}
				}

			}
			else
			{
				GameWarning("Destruction event has no 'name' at line %d", eventNode->getLine());
			}
		}
	}

	//Mike
	XmlNodeRef mikeDeathNode = rootNode->findChild("MikeDeath");
	if (mikeDeathNode)
	{
		XmlNodeRef mikeAttachmentNode = mikeDeathNode->findChild("Attachment");
		if (mikeAttachmentNode && mikeAttachmentNode->haveAttr("name"))
		{
			stl::push_back_unique(attachmentList, string(mikeAttachmentNode->getAttr("name")));
		}
		else
		{
			GameWarning("Mike Death does not define an 'attachment' to use");
		}
	}

	// Prepare actual bone/attachment arrays
	SDestructibleBodyPart bodyPart;

	m_attachments.reserve(attachmentList.size());
	for (TPartNames::const_iterator cit = attachmentList.begin(); cit != attachmentList.end(); ++cit)
	{
		bodyPart.name = *cit;
		m_attachments.push_back(bodyPart);
		parsingHelper.attachmentIndices.insert(TNameHashToIdxMap::value_type(CryStringUtils::HashString(bodyPart.name.c_str()), (int)(m_attachments.size() - 1)));
	}

	m_bones.reserve(boneList.size());
	for (TPartNames::const_iterator cit = boneList.begin(); cit != boneList.end(); ++cit)
	{
		bodyPart.name = *cit;
		m_bones.push_back(bodyPart);
		parsingHelper.boneIndices.insert(TNameHashToIdxMap::value_type(CryStringUtils::HashString(bodyPart.name.c_str()), (int)(m_bones.size() - 1)));
	}
}

void CBodyDestructibilityProfile::LoadDestructibleParts( const XmlNodeRef& destructiblesNode, SParsingHelper& parsingHelper )
{
	if (destructiblesNode)
	{
		const int destructibleCount = destructiblesNode->getChildCount();
		for (int i = 0; i < destructibleCount; ++i)
		{
			XmlNodeRef partNode = destructiblesNode->getChild(i);
			CRY_ASSERT(partNode != NULL);

			const char* partName = partNode->getAttr("name");
			if (partName)
			{
				//Gather previously allocated body part
				SDestructibleBodyPart* pCurrentBodyPart = NULL;
				if (strcmp(partNode->getTag(), "Attachment") == 0)
				{
					const int attachmentIdx = (int)parsingHelper.GetAttachmentIndex(partName);

					pCurrentBodyPart = (attachmentIdx != -1) ? &m_attachments[attachmentIdx] : NULL;	

					if (pCurrentBodyPart)
					{
						m_destructibleAttachmentCount++; //Not all attachments are destructible, only from 0 to this 'count'
					}
				}
				else if (strcmp(partNode->getTag(), "Bone") == 0)
				{
					const int boneIdx = parsingHelper.GetBoneIndex(partName);

					pCurrentBodyPart = (boneIdx != -1) ? &m_bones[boneIdx] : NULL;
				}

				if (pCurrentBodyPart)
				{
					pCurrentBodyPart->hashId = CryStringUtils::HashString(partName);

					LoadPart(partNode, *pCurrentBodyPart, parsingHelper);
				}
				else
				{
					GameWarning("Failed to load body part data, '%s' undefined", partName);
				}

			}
			else
			{
				GameWarning("Destructible part with no 'name' at line %d", partNode->getLine());

			}
		}
	}
}

void CBodyDestructibilityProfile::LoadPart( const XmlNodeRef& partNode, SDestructibleBodyPart& partData, SParsingHelper& parsingHelper)
{
	CRY_ASSERT(partNode != NULL);

	//Parse health and default events
	XmlNodeRef healthNode = partNode->findChild("Health");
	if (healthNode)
	{
		healthNode->getAttr("ratio", partData.healthRatio);
		if (healthNode->haveAttr("ratioToDestroyOnDeath"))
		{
			healthNode->getAttr("ratioToDestroyOnDeath", partData.minHealthToDestroyOnDeathRatio);
		}
		else
		{
			partData.minHealthToDestroyOnDeathRatio = partData.healthRatio;
		}
	
		const char* eventOnDestruction = healthNode->getAttr("eventOnDestruction");
		if (eventOnDestruction)
		{
			partData.destructionEvents[0] = (TDestructionEventId)parsingHelper.GetEventIndex(eventOnDestruction);
		}

		const char* eventOnActorDeath = healthNode->getAttr("eventOnActorDeath");
		if (eventOnActorDeath)
		{
			partData.destructionEvents[1] = (TDestructionEventId)parsingHelper.GetEventIndex(eventOnActorDeath);
		}
		else
		{
			partData.destructionEvents[1] = partData.destructionEvents[0];
		}
	}

	//Parse specific hit types
	CGameRules* pGameRules = g_pGame->GetGameRules();
	CRY_ASSERT(pGameRules);

	XmlNodeRef hitTypesNode = partNode->findChild("HitTypes");
	if (hitTypesNode)
	{
		const int hitTypeCount = hitTypesNode->getChildCount();
		partData.hitTypes.reserve(hitTypeCount);

		for (int i = 0; i < hitTypeCount; ++i)
		{
			XmlNodeRef typeNode = hitTypesNode->getChild(i);
			CRY_ASSERT(typeNode != NULL);

			const char* hitTypeName = typeNode->getAttr("name");
			if (hitTypeName)
			{
				SBodyHitType hitTypeInfo;
				hitTypeInfo.hitTypeId = pGameRules ? pGameRules->GetHitTypeId(hitTypeName) : 0;
				typeNode->getAttr("damageMultiplier", hitTypeInfo.damageMultiplier);

				const char* eventOnDestruction = typeNode->getAttr("eventOnDestruction");
				if (eventOnDestruction)
				{
					hitTypeInfo.destructionEvents[0] = (TDestructionEventId)parsingHelper.GetEventIndex(eventOnDestruction);
				}

				const char* eventOnActorDeath = typeNode->getAttr("eventOnActorDeath");
				if (eventOnActorDeath)
				{
					hitTypeInfo.destructionEvents[1] = (TDestructionEventId)parsingHelper.GetEventIndex(eventOnActorDeath);
				}

				partData.hitTypes.push_back(hitTypeInfo);
			}
			else
			{
				GameWarning("HitType at line '%d' has no 'name'", typeNode->getLine());
			}
		}
	}
}

void CBodyDestructibilityProfile::LoadEvents( const XmlNodeRef& eventsNode, SParsingHelper& parsingHelper )
{
	if (eventsNode)
	{
		const int eventsCount = eventsNode->getChildCount();
		for (int i = 0; i < eventsCount; ++i)
		{
			XmlNodeRef eventNode = eventsNode->getChild(i);
			CRY_ASSERT(eventNode != NULL);

			const char* eventName = eventNode->getAttr("name");
			if (eventName)
			{
				//Gather previously allocated event
				const int eventIndex = parsingHelper.GetEventIndex(eventName);

				SDestructionEvent* pCurrentEvent = (eventIndex != -1) ? &m_destructionEvents[eventIndex] : NULL;

				if (pCurrentEvent)
				{
					LoadEvent(eventNode, *pCurrentEvent, parsingHelper);
				}
				else
				{
					GameWarning("Failed to load event data, '%s' undefined", eventName);

				}
			}
			else
			{
				GameWarning("Destruction event with no 'name' at line %d", eventNode->getLine());
			}
		}
	}
}

void CBodyDestructibilityProfile::LoadEvent( const XmlNodeRef& eventNode, SDestructionEvent& eventData, SParsingHelper& parsingHelper )
{
	CRY_ASSERT(eventNode != NULL);

	eventData.scriptCondition = eventNode->haveAttr("scriptCondition") ? eventNode->getAttr("scriptCondition") : "";

	//Attachments to hide
	XmlNodeRef attachmentsToHideNode = eventNode->findChild("AttachmentsToHide");
	if (attachmentsToHideNode)
	{
		const int attachmentCount = attachmentsToHideNode->getChildCount();

		eventData.attachmentsToHide.reserve(attachmentCount);

		for (int i = 0; i < attachmentCount; ++i)
		{
			const char* attachmentName = attachmentsToHideNode->getChild(i)->getAttr("name");
			if (attachmentName)
			{
				const TDestructibleBodyPartId attachmentIdx = (TDestructibleBodyPartId)parsingHelper.GetAttachmentIndex(attachmentName);
				CRY_ASSERT_MESSAGE(attachmentIdx != -1, "Something went wrong with the initial attachment pre-parsing, this could crash during run-time");
				eventData.attachmentsToHide.push_back(attachmentIdx);
			}
			else
			{
				GameWarning("Attachment to hide with no 'name' at line %d", attachmentsToHideNode->getLine() + i);
			}
		}
	}

	//Attachments to unhide
	XmlNodeRef attachmentsToUnhideNode = eventNode->findChild("AttachmentToUnhide");
	if (attachmentsToUnhideNode)
	{
		const int attachmentCount = attachmentsToUnhideNode->getChildCount();

		eventData.attachmentsToUnhide.reserve(attachmentCount);

		for (int i = 0; i < attachmentCount; ++i)
		{
			const char* attachmentName = attachmentsToUnhideNode->getChild(i)->getAttr("name");
			if (attachmentName)
			{
				const TDestructibleBodyPartId attachmentIdx = (TDestructibleBodyPartId)parsingHelper.GetAttachmentIndex(attachmentName);
				CRY_ASSERT_MESSAGE(attachmentIdx != -1, "Something went wrong with the initial attachment pre-parsing, this could crash during run-time");
				eventData.attachmentsToUnhide.push_back(attachmentIdx);
			}
			else
			{
				GameWarning("Attachment to unhide with no 'name' at line %d", attachmentsToUnhideNode->getLine() + i);
			}
		}
	}

	//Particle effect
	XmlNodeRef effectNode = eventNode->findChild("Effect");
	if (effectNode && effectNode->haveAttr("name"))
	{
		eventData.particleFX = effectNode->getAttr("name");
	}

	//Disabled events
	XmlNodeRef disabledEventsNode = eventNode->findChild("DisableEvents");
	if (disabledEventsNode)
	{
		const int eventCount = disabledEventsNode->getChildCount();

		eventData.eventsToDisable.reserve(eventCount);

		for (int i = 0; i < eventCount; ++i)
		{
			const char* eventName = disabledEventsNode->getChild(i)->getAttr("name");
			if (eventName)
			{
				const TDestructionEventId eventIdx = (TDestructionEventId)parsingHelper.GetEventIndex(eventName);
				if (eventIdx != -1)
				{
					eventData.eventsToDisable.push_back(eventIdx);
				}
				else
				{
					GameWarning("Disable event '%s' does not exist, at line %d", eventName, disabledEventsNode->getLine() + i);
				}
			}
			else
			{
				GameWarning("Disabled event with no 'name' at line %d", disabledEventsNode->getLine() + i);
			}
		}
	}

	//Events to stop
	XmlNodeRef stopEventsNode = eventNode->findChild("StopEvents");
	if (stopEventsNode)
	{
		const int eventCount = stopEventsNode->getChildCount();

		eventData.eventsToStop.reserve(eventCount);

		for (int i = 0; i < eventCount; ++i)
		{
			const char* eventName = stopEventsNode->getChild(i)->getAttr("name");
			if (eventName)
			{
				const TDestructionEventId eventIdx = (TDestructionEventId)parsingHelper.GetEventIndex(eventName);
				if (eventIdx != -1)
				{
					eventData.eventsToStop.push_back(eventIdx);
				}
				else
				{
					GameWarning("Stop event '%s' does not exist, at line %d", eventName, stopEventsNode->getLine() + i);
				}
			}
			else
			{
				GameWarning("Stop event with no 'name' at line %d", stopEventsNode->getLine() + i);
			}
		}
	}

	//Explosion node?
	XmlNodeRef explosionNode = eventNode->findChild("Explosion");
	if (explosionNode)
	{
		CGameRules* pGameRules = g_pGame->GetGameRules();
		CRY_ASSERT(pGameRules);

		SAFE_DELETE(eventData.pExplosion); //Should not be needed, but better not rely on data

		eventData.pExplosion = new SBodyPartExplosion();
		CRY_ASSERT(eventData.pExplosion);
		
		explosionNode->getAttr("damage", eventData.pExplosion->damage);
		explosionNode->getAttr("minRadius", eventData.pExplosion->minRadius);
		explosionNode->getAttr("maxRadius", eventData.pExplosion->maxRadius);
		explosionNode->getAttr("pressure", eventData.pExplosion->pressure);
		
		eventData.pExplosion->effectName = explosionNode->haveAttr("effect") ? explosionNode->getAttr("effect") : "";
		eventData.pExplosion->hitTypeId = CGameRules::EHitType::Frag;
	}

	//Script callback
	XmlNodeRef scriptCallbackNode = eventNode->findChild("ScriptCallback");
	if (scriptCallbackNode && scriptCallbackNode->haveAttr("functionName"))
	{
		eventData.scriptCallback = scriptCallbackNode->getAttr("functionName");
	}
}

void CBodyDestructibilityProfile::LoadHealthRatioEvents( const XmlNodeRef& hitRatiosNode, SParsingHelper& parsingHelper )
{
	if (hitRatiosNode)
	{
		const int hitRatioCount = hitRatiosNode->getChildCount();
		m_healthRatioEvents.reserve(hitRatioCount);

		for (int i = 0; i < hitRatioCount; ++i)
		{
			XmlNodeRef ratioNode = hitRatiosNode->getChild(i);
			CRY_ASSERT(ratioNode != NULL);

			SHealthRatioEvent ratioEvent;

			ratioEvent.bone = ratioNode->haveAttr("bone") ? ratioNode->getAttr("bone") : "";
			ratioEvent.material = ratioNode->haveAttr("material") ? ratioNode->getAttr("material") : "";

			ratioEvent.destructionEvent = ratioNode->haveAttr("event") ? (TDestructionEventId)parsingHelper.GetEventIndex(ratioNode->getAttr("event")) : -1;

			ratioNode->getAttr("ratio", ratioEvent.healthRatio);

			m_healthRatioEvents.push_back(ratioEvent);
		}

		std::sort(m_healthRatioEvents.begin(), m_healthRatioEvents.end(), compareHealthRatios());
	}
}

void CBodyDestructibilityProfile::LoadExplosionDeaths( const XmlNodeRef& explosionDeathsNode, SParsingHelper& parsingHelper )
{
	if (explosionDeathsNode)
	{
		XmlNodeRef gibDeathNode = explosionDeathsNode->findChild("GibEvent");
		if (gibDeathNode)
		{
			m_gibDeath.bone = gibDeathNode->haveAttr("bone") ? gibDeathNode->getAttr("bone") : "";
			m_gibDeath.material = gibDeathNode->haveAttr("material") ? gibDeathNode->getAttr("material") : "";

			m_gibDeath.destructionEvent = gibDeathNode->haveAttr("event") ? (TDestructionEventId)parsingHelper.GetEventIndex(gibDeathNode->getAttr("event")) : -1;

			gibDeathNode->getAttr("probability", m_gibDeath.gibProbability);
			gibDeathNode->getAttr("minExplosionDamage", m_gibDeath.minExplosionDamage);
		}

		XmlNodeRef nonGibDeathNode = explosionDeathsNode->findChild("NonGibEvent");
		if (nonGibDeathNode)
		{
			m_nonGibDeath.bone = nonGibDeathNode->haveAttr("bone") ? nonGibDeathNode->getAttr("bone") : "";
			m_nonGibDeath.material = nonGibDeathNode->haveAttr("material") ? nonGibDeathNode->getAttr("material") : "";

			m_nonGibDeath.destructionEvent = nonGibDeathNode->haveAttr("event") ? (TDestructionEventId)parsingHelper.GetEventIndex(nonGibDeathNode->getAttr("event")) : -1;
		}
	}
}

void CBodyDestructibilityProfile::LoadMikeDeath( const XmlNodeRef& mikeDeathNode, SParsingHelper& parsingHelper )
{
	if (mikeDeathNode)
	{
		XmlNodeRef eventNode = mikeDeathNode->findChild("Destruction");
		if (eventNode)
		{
			m_mikeDeath.destructionEvent = eventNode->haveAttr("event") ? (TDestructionEventId)parsingHelper.GetEventIndex(eventNode->getAttr("event")) : -1;
			m_mikeDeath.bone = eventNode->haveAttr("bone") ? eventNode->getAttr("bone") : "";
		}

		XmlNodeRef attachmentNode = mikeDeathNode->findChild("Attachment");
		if (attachmentNode)
		{
			m_mikeDeath.attachmentId = attachmentNode->haveAttr("name") ? (TDestructibleBodyPartId)parsingHelper.GetAttachmentIndex(attachmentNode->getAttr("name")) : -1;
			m_mikeDeath.animation = attachmentNode->haveAttr("animation") ? attachmentNode->getAttr("animation") : "";
			m_mikeDeath.object = attachmentNode->haveAttr("object") ? attachmentNode->getAttr("object") : "";
			attachmentNode->getAttr("alphaTestFadeOutDelay", m_mikeDeath.alphaTestFadeOutDelay);
			attachmentNode->getAttr("alphaTestFadeOutTimeOut", m_mikeDeath.alphaTestFadeOutTimeOut);
			attachmentNode->getAttr("alphaTestFadeOutMaxAlpha", m_mikeDeath.alphaTestFadeOutMaxAlpha);
		}
	}
}

void CBodyDestructibilityProfile::PrepareInstance( CBodyDestrutibilityInstance& instance, float instanceBaseHealth, const SBodyCharacterInfo& bodyInfo )
{
	CRY_ASSERT(bodyInfo.pAttachmentManager);

	instance.ReserveSlots((int)bodyInfo.pAttachmentManager->GetAttachmentCount(), (int)m_attachments.size(), (int)m_bones.size(), (int)m_destructionEvents.size());
	instance.SetInstanceHealth(instanceBaseHealth);

	for (TDestructibleBodyParts::const_iterator attachmentCit = m_attachments.begin(); attachmentCit != m_attachments.end(); ++attachmentCit)
	{
		const SDestructibleBodyPart& bodyPart = *attachmentCit;

		IAttachment* pAttachment = bodyInfo.pAttachmentManager->GetInterfaceByName(bodyPart.name.c_str());
		const bool visibleAtStart = pAttachment ? (pAttachment->IsAttachmentHidden() == 0) : false;

		instance.AddAttachment(instanceBaseHealth * bodyPart.healthRatio, instanceBaseHealth * bodyPart.minHealthToDestroyOnDeathRatio, visibleAtStart);
	}

	for (TDestructibleBodyParts::const_iterator boneCit = m_bones.begin(); boneCit != m_bones.end(); ++boneCit)
	{
		const SDestructibleBodyPart& bodyPart = *boneCit;

		instance.AddBone(instanceBaseHealth * bodyPart.healthRatio, instanceBaseHealth * bodyPart.minHealthToDestroyOnDeathRatio);
	}

	// We have one user, cache resources
	CacheLevelResources();
}

void CBodyDestructibilityProfile::ResetInstance( IEntity& characterEntity, CBodyDestrutibilityInstance& instance )
{
	ICharacterInstance* pCharacterInstance = characterEntity.GetCharacter(0);
	if (pCharacterInstance)
	{
		IAttachmentManager* pAttachmentManager = pCharacterInstance->GetIAttachmentManager();
		CRY_ASSERT(pAttachmentManager);

		// Only if something changed...
		if (instance.AreInstanceDestructiblesModified())
		{
			//Restore attachment visibility
			const size_t attachmentCount = m_attachments.size();
			for (size_t attachmentIdx = 0; attachmentIdx < attachmentCount; ++attachmentIdx)
			{
				IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName(m_attachments[attachmentIdx].name.c_str());
				CBodyDestrutibilityInstance::SBodyDestructiblePartStatus* pPartStatus = instance.GetAttachmentStatus(attachmentIdx);
				if ((pAttachment != NULL) && (pPartStatus != NULL))
				{
					pAttachment->HideAttachment(pPartStatus->WasInitialyVisible() ? 0 : 1);
				}

				IAttachmentObject* pAttachmentObject = pAttachment ? pAttachment->GetIAttachmentObject() : NULL;
				if ((pAttachmentObject != NULL) && (pAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_Entity))
				{
					CEntityAttachment* pAttachmentEntity = static_cast<CEntityAttachment*>(pAttachmentObject);
					if (pAttachmentEntity->GetEntityId() == instance.GetMikeAttachmentEntityId())
					{
						pAttachmentEntity->SetEntityId(0);
					}
				}
			}

			//Destroy event generated attachments
			const size_t eventCount = m_destructionEvents.size();
			for (size_t eventIdx = 0; eventIdx < eventCount; ++eventIdx)
			{
				pAttachmentManager->RemoveAttachmentByName(m_destructionEvents[eventIdx].name.c_str());
			}
		}

		ResetMaterials(characterEntity, *pCharacterInstance, instance);
	}

	//Health and event status reset
	instance.Reset();
}

void CBodyDestructibilityProfile::CacheLevelResources()
{
	if (AreLevelResourcesCached())
		return;

	for (TDestructionEvents::iterator eventCit = m_destructionEvents.begin(); eventCit != m_destructionEvents.end(); ++eventCit)
	{
		SDestructionEvent& destructionEvent = *eventCit;

		if (!destructionEvent.particleFX.empty())
		{
			if( gEnv->pParticleManager->FindEffect(destructionEvent.particleFX.c_str(), "CBodyDestructibilityProfile::CacheLevelResources") == NULL )
			{
				destructionEvent.particleFX.clear();
			}
		}

		if ((destructionEvent.pExplosion != NULL) && !destructionEvent.pExplosion->effectName.empty())
		{
			if( gEnv->pParticleManager->FindEffect(destructionEvent.pExplosion->effectName.c_str(), "CBodyDestructibilityProfile::CacheLevelResources") == NULL )
			{
				destructionEvent.pExplosion->effectName.clear();
			}
		}
	}

	for (THealthRatioEvents::const_iterator eventCit = m_healthRatioEvents.begin(); eventCit != m_healthRatioEvents.end(); ++eventCit)
	{
		const SHealthRatioEvent& healthRatioEvent = *eventCit;

		CacheMaterial(healthRatioEvent.material.c_str());
	}

	CacheMaterial(m_gibDeath.material.c_str());
	CacheMaterial(m_nonGibDeath.material.c_str());

	CacheCharacter(m_mikeDeath.object.c_str());

	m_arelevelResourcesCached = true;
}

void CBodyDestructibilityProfile::CacheMaterial( const char* materialName )
{
	const bool isValidName = (materialName != NULL) && (materialName[0]);
	if (isValidName)
	{
		IMaterial *pReplacement = gEnv->p3DEngine->GetMaterialManager()->FindMaterial(materialName);
		if (!pReplacement)
		{
			pReplacement = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(materialName, false);
		}

		if (pReplacement)
		{
			const CryHash materialHash = CryStringUtils::HashString(materialName);
			if (m_replacementMaterials.find(materialHash) == m_replacementMaterials.end())
			{
				m_replacementMaterials.insert(TReplacementMaterials::value_type(materialHash, IMaterialSmartPtr(pReplacement)));
			}
		}
		else
		{
			GameWarning("Body destruction could not find and cache material '%s'", materialName);
		}
	}
}

void CBodyDestructibilityProfile::CacheCharacter( const char* characterName )
{
	const bool isValidName = (characterName != NULL) && (characterName[0]);
	if (isValidName)
	{
		const CryHash characterHash = CryStringUtils::HashString(characterName);
		if (m_cachedCharacterInstaces.find(characterHash) == m_cachedCharacterInstaces.end())
		{
			ICharacterInstance *pCharacterInstance = gEnv->pCharacterManager->CreateInstance(characterName);
			if (pCharacterInstance)
			{
				m_cachedCharacterInstaces.insert(TCachedCharacterInstances::value_type(characterHash, ICharacterInstancePtr(pCharacterInstance)));
			}
			else
			{
				GameWarning("Body destruction could not find and cache character '%s'", characterName);
			}
		}
	}
}

void CBodyDestructibilityProfile::FlushLevelResourceCache()
{
	m_replacementMaterials.clear();
	m_cachedCharacterInstaces.clear();

	m_arelevelResourcesCached = false;
}

void CBodyDestructibilityProfile::ProcessDestructiblesHit( IEntity& characterEntity, CBodyDestrutibilityInstance& instance, const HitInfo& hitInfo, const float previousHealth, const float newHealth )
{
	instance.SetDestructibleLastEventForHitReactions("");

	//Allow for special events even when it is a punish death, so it can trigger gibs on death bodies, etc..
	const bool processDeathByPunishHit = ((previousHealth <= 0.0f) || (newHealth <= 0.0f)) && (hitInfo.type == CGameRules::EHitType::Punish);
	if (processDeathByPunishHit)
	{
		ProcessDeathByPunishHit(characterEntity, instance, hitInfo);
		return;
	}

	ICharacterInstance* pCharacterInstance = characterEntity.GetCharacter(0);
	if (pCharacterInstance)
	{
		IDefaultSkeleton& rIDefaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();
		ISkeletonPose* pSkeletonPose = pCharacterInstance->GetISkeletonPose();
		CRY_ASSERT(pSkeletonPose);

		IAttachmentManager* pAttachmentManager = pCharacterInstance->GetIAttachmentManager();
		CRY_ASSERT(pAttachmentManager);

		SBodyPartQueryResult bodyPart;
		CBodyDestrutibilityInstance::SBodyDestructiblePartStatus* pPartStatus = NULL;
		const char* effectAttachmentBone = NULL;
		const char* hitPartName = NULL;

		if (IAttachment* pAttachment = pAttachmentManager->GetInterfaceByPhysId(hitInfo.partId))
		{
			if (GetDestructibleAttachment(CryStringUtils::HashString(pAttachment->GetName()), bodyPart))
			{
				pPartStatus = instance.GetAttachmentStatus(bodyPart.index);
				effectAttachmentBone = rIDefaultSkeleton.GetJointNameByID(pAttachment->GetJointID());
				hitPartName = pAttachment->GetName();
			}
		}
		else if (hitInfo.partId >= 0)
		{
			const char* boneName = rIDefaultSkeleton.GetJointNameByID(pSkeletonPose->getBonePhysParentOrSelfIndex(hitInfo.partId));
			if ((boneName != NULL)  && GetDestructibleBone(CryStringUtils::HashString(boneName), bodyPart))
			{
				pPartStatus = instance.GetBoneStatus(bodyPart.index);
				effectAttachmentBone = boneName;
				hitPartName = boneName;
			}
		}

		const bool diedByHit = ((previousHealth > 0.0f) && (newHealth <= 0.0f));

		//Do we hit something destroyable?
		if ((pPartStatus != NULL) && (!pPartStatus->IsDestroyed()))
		{
			float inflictedDamage = (float)__fsel( -newHealth, hitInfo.damage, previousHealth - newHealth );
			CRY_ASSERT(inflictedDamage >= 0.0f);

			const int eventIdx = diedByHit ? 1 : 0 ;
			TDestructionEventId destructionEventId = bodyPart.pPart->destructionEvents[eventIdx];
			float hitDamageMultiplier = 1.0f;

			if (const SBodyHitType* pHitTypeModifier = bodyPart.pPart->GetHitTypeModifiers(hitInfo.type))
			{
				hitDamageMultiplier = pHitTypeModifier->damageMultiplier;
				destructionEventId = (pHitTypeModifier->destructionEvents[eventIdx] != -1) ? pHitTypeModifier->destructionEvents[eventIdx] : destructionEventId;
			}

			pPartStatus->SetHealth(pPartStatus->GetHealth() - (inflictedDamage * hitDamageMultiplier));

			//If destroyed, do the rest....
			if (pPartStatus->IsDestroyed() && instance.CanTriggerEvent(destructionEventId) && CheckAgainstScriptCondition(characterEntity, destructionEventId))
			{
				ProcessDestructionEvent(characterEntity, instance, hitInfo, destructionEventId, pAttachmentManager, effectAttachmentBone);
			}

			LogDamage(characterEntity, "Hit", hitPartName, inflictedDamage*hitDamageMultiplier, hitDamageMultiplier, pPartStatus->IsDestroyed());
		}

		if (previousHealth > 0.0f)
		{
			ProcessHealthRatioEvents(characterEntity, *pCharacterInstance, pAttachmentManager, instance, hitInfo, newHealth);	
		}
		
		if (diedByHit)
		{
			//Process on death health ratios (attachment auto-destruction)
			ProcessDeathByHit(characterEntity, *pCharacterInstance, instance, hitInfo);
		}
	}
}

bool CBodyDestructibilityProfile::CheckAgainstScriptCondition( IEntity& characterEntity, TDestructionEventId destructionEventId )
{
	CRY_ASSERT((destructionEventId >= 0) && (destructionEventId < (int)m_destructionEvents.size()));

	const SDestructionEvent& destructionEvent = m_destructionEvents[destructionEventId];
	if (destructionEvent.scriptCondition.empty())
	{
		return true;
	}
	else
	{
		return EntityScripts::CallScriptFunction(&characterEntity, characterEntity.GetScriptTable(), destructionEvent.scriptCondition.c_str());
	}
}

void CBodyDestructibilityProfile::ProcessDestructionEvent( IEntity& characterEntity, CBodyDestrutibilityInstance& instance, const HitInfo& hitInfo, const TDestructionEventId destructionEventId, IAttachmentManager* pAttachmentManager, const char* targetEffectBone,  const Vec3& noAttachEffectPos /*= ZERO*/ )
{
	CRY_ASSERT(pAttachmentManager);
	CRY_ASSERT(targetEffectBone);
	CRY_ASSERT((destructionEventId >= 0) && (destructionEventId < (int)m_destructionEvents.size()));

	const SDestructionEvent& destructionEvent = m_destructionEvents[destructionEventId];

	// Hide attachments
	TDestructibleAttachmentIds::const_iterator attachmentToHideCit = destructionEvent.attachmentsToHide.begin();
	TDestructibleAttachmentIds::const_iterator attachmentToHideEnd = destructionEvent.attachmentsToHide.end();
	while(attachmentToHideCit != attachmentToHideEnd)
	{
		const TDestructibleBodyPartId attachmentId = *attachmentToHideCit;
		CRY_ASSERT((attachmentId >= 0) && (attachmentId < (int)m_attachments.size()));

		HideAttachment(pAttachmentManager, m_attachments[attachmentId].name.c_str(), 1);

		++attachmentToHideCit;
	}

	// Unhide attachments
	TDestructibleAttachmentIds::const_iterator attachmentToUnhideCit = destructionEvent.attachmentsToUnhide.begin();
	TDestructibleAttachmentIds::const_iterator attachmentToUnHideEnd = destructionEvent.attachmentsToUnhide.end();
	while(attachmentToUnhideCit != attachmentToUnHideEnd)
	{
		const TDestructibleBodyPartId attachmentId = *attachmentToUnhideCit;
		CRY_ASSERT((attachmentId >= 0) && (attachmentId < (int)m_attachments.size()));

		HideAttachment(pAttachmentManager, m_attachments[attachmentId].name.c_str(), 0);

		++attachmentToUnhideCit;
	}

	// Spawn effect ?
	if (!destructionEvent.particleFX.empty())
	{
		if (noAttachEffectPos.IsZero())
		{
			IAttachment* pEffectAttachment = pAttachmentManager->CreateAttachment(destructionEvent.name.c_str(), CA_BONE, targetEffectBone, false);
			if (pEffectAttachment)
			{
				pEffectAttachment->SetAttRelativeDefault( QuatT(ZERO, IDENTITY) );

				CEffectAttachment* pEffect = new CEffectAttachment(destructionEvent.particleFX.c_str(), ZERO, FORWARD_DIRECTION, 1.0f);
				CRY_ASSERT(pEffect);

				pEffectAttachment->AddBinding(pEffect);
			}
		}
		else
		{
			//For Gib event, so it just spawns the effect
			EntityEffects::SpawnParticleFX( destructionEvent.particleFX.c_str(), EntityEffects::SEffectSpawnParams( noAttachEffectPos ) );
		}

	}

	// Disable other events
	TDestructionEventIds::const_iterator eventToDisableCit = destructionEvent.eventsToDisable.begin();
	TDestructionEventIds::const_iterator eventToDisableEnd = destructionEvent.eventsToDisable.end();
	while (eventToDisableCit != eventToDisableEnd)
	{
		instance.DisableEvent(*eventToDisableCit);
		++eventToDisableCit;
	}

	// Stop other events
	TDestructionEventIds::const_iterator eventToStopCit = destructionEvent.eventsToStop.begin();
	TDestructionEventIds::const_iterator eventToStopEnd = destructionEvent.eventsToStop.end();
	while (eventToStopCit != eventToStopEnd)
	{
		const TDestructibleBodyPartId eventId = *eventToStopCit;
		CRY_ASSERT((eventId >= 0) && (eventId < (int)m_destructionEvents.size()));

		pAttachmentManager->RemoveAttachmentByName(m_destructionEvents[eventId].name.c_str());

		++eventToStopCit;
	}

	// And finally, generate an explosion?
	if (destructionEvent.pExplosion)
	{
		ExplosionInfo explosionInfo(hitInfo.targetId, 0, 0, destructionEvent.pExplosion->damage , hitInfo.pos, hitInfo.dir,
			destructionEvent.pExplosion->minRadius, destructionEvent.pExplosion->maxRadius,
			destructionEvent.pExplosion->minRadius * 0.7f, destructionEvent.pExplosion->maxRadius * 0.7f,
			0.0f, destructionEvent.pExplosion->pressure, 0.0f, destructionEvent.pExplosion->hitTypeId);

		explosionInfo.SetEffect(destructionEvent.pExplosion->effectName.c_str(), 1.0f, 0.0f);

		CGameRules* pGameRules = g_pGame->GetGameRules();
		if (pGameRules)
		{
			pGameRules->QueueExplosion(explosionInfo);
		}
	}

	//Script callback?
	if (!destructionEvent.scriptCallback.empty())
	{
		EntityScripts::CallScriptFunction(&characterEntity, characterEntity.GetScriptTable(), destructionEvent.scriptCallback.c_str());
	}

	LogDestructionEvent(characterEntity, destructionEvent.name.c_str());

	//Disable itself after triggered
	instance.DisableEvent(destructionEventId);
	instance.SetDestructibleLastEventForHitReactions(m_destructionEvents[destructionEventId].name.c_str());
}

void CBodyDestructibilityProfile::ProcessDestructiblesOnExplosion( IEntity& characterEntity, CBodyDestrutibilityInstance& instance, const HitInfo& hitInfo, const float previousHealth, const float newHealth )
{
	instance.SetDestructibleLastEventForHitReactions("");

	//Allow for special explosion events even when it is death, so it can trigger gibs on death bodies, etc..
	const bool processDeathByExplosion = ((previousHealth <= 0.0f) || (newHealth <= 0.0f));
	if (processDeathByExplosion)
	{
		ProcessDeathByExplosion(characterEntity, instance, hitInfo);
		return;
	}

	ICharacterInstance* pCharacterInstance = characterEntity.GetCharacter(0);
	if (pCharacterInstance)
	{
		const int eventIdx = 0;
		const int maxDestructiblesByExplosion = 2;
		int destroyedBodyParts = 0;

		float inflictedDamage = (float)__fsel( -newHealth, hitInfo.damage, previousHealth - newHealth );
		CRY_ASSERT(inflictedDamage >= 0.0f);

		IDefaultSkeleton& rIDefaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();

		IAttachmentManager* pAttachmentManager = pCharacterInstance->GetIAttachmentManager();
		CRY_ASSERT(pAttachmentManager);

		//First process attachments
		size_t attachmentIdx = 0;
		const size_t destructibleAttachments = min(m_destructibleAttachmentCount, m_attachments.size());
		while((attachmentIdx < destructibleAttachments) && (destroyedBodyParts < maxDestructiblesByExplosion))
		{
			CBodyDestrutibilityInstance::SBodyDestructiblePartStatus* pPartStatus = instance.GetAttachmentStatus(attachmentIdx);
			if ((pPartStatus != NULL) && !pPartStatus->IsDestroyed())
			{
				const SDestructibleBodyPart& bodyPart = m_attachments[attachmentIdx];

				TDestructionEventId destructionEventId = bodyPart.destructionEvents[eventIdx];
				float hitDamageMultiplier = 1.0f;

				if (const SBodyHitType* pHitTypeModifier = bodyPart.GetHitTypeModifiers(hitInfo.type))
				{
					hitDamageMultiplier = pHitTypeModifier->damageMultiplier;
					destructionEventId = (pHitTypeModifier->destructionEvents[eventIdx] != -1) ? pHitTypeModifier->destructionEvents[eventIdx] : destructionEventId;
				}

				pPartStatus->SetHealth(pPartStatus->GetHealth() - (inflictedDamage*hitDamageMultiplier));

				//If destroyed, do the rest....
				if (pPartStatus->IsDestroyed() && instance.CanTriggerEvent(destructionEventId) && CheckAgainstScriptCondition(characterEntity, destructionEventId))
				{
					IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName(bodyPart.name.c_str());
					const char *effectAttachmentBone = pAttachment ? rIDefaultSkeleton.GetJointNameByID(pAttachment->GetJointID()) : "";

					ProcessDestructionEvent(characterEntity, instance, hitInfo, destructionEventId, pAttachmentManager, effectAttachmentBone);

					destroyedBodyParts++;
				}

				LogDamage(characterEntity, "Explosion", bodyPart.name.c_str(), inflictedDamage*hitDamageMultiplier, hitDamageMultiplier, pPartStatus->IsDestroyed());
			}
			++attachmentIdx;
		}

		//Second process bones
		destroyedBodyParts = 0; //Reset, so we give the same chances to destroy attachments and bones
		int boneIdx = 0;
		const int destructibleBones = (int)m_bones.size();
		while((boneIdx < destructibleBones) && (destroyedBodyParts < maxDestructiblesByExplosion))
		{
			CBodyDestrutibilityInstance::SBodyDestructiblePartStatus* pPartStatus = instance.GetBoneStatus(boneIdx);
			if ((pPartStatus != NULL) && !pPartStatus->IsDestroyed())
			{
				const SDestructibleBodyPart& bodyPart = m_bones[boneIdx];

				TDestructionEventId destructionEventId = bodyPart.destructionEvents[eventIdx];
				float hitDamageMultiplier = 1.0f;

				if (const SBodyHitType* pHitTypeModifier = bodyPart.GetHitTypeModifiers(hitInfo.type))
				{
					hitDamageMultiplier = pHitTypeModifier->damageMultiplier;
					destructionEventId = (pHitTypeModifier->destructionEvents[eventIdx] != -1) ? pHitTypeModifier->destructionEvents[eventIdx] : destructionEventId;
				}

				pPartStatus->SetHealth(pPartStatus->GetHealth() - (inflictedDamage*hitDamageMultiplier));

				//If destroyed, do the rest....
				if (pPartStatus->IsDestroyed() && instance.CanTriggerEvent(destructionEventId) && CheckAgainstScriptCondition(characterEntity, destructionEventId))
				{
					const char *effectAttachmentBone = bodyPart.name.c_str();

					ProcessDestructionEvent(characterEntity, instance, hitInfo, destructionEventId, pAttachmentManager, effectAttachmentBone);

					destroyedBodyParts++;
				}

				LogDamage(characterEntity, "Explosion", bodyPart.name.c_str(), inflictedDamage*hitDamageMultiplier, hitDamageMultiplier, pPartStatus->IsDestroyed());
			}
			++boneIdx;
		}
	
		if (previousHealth > 0.0f)
		{
			ProcessHealthRatioEvents(characterEntity, *pCharacterInstance, pAttachmentManager, instance, hitInfo, newHealth);
		}
	}
}

void CBodyDestructibilityProfile::ProcessDestructionEventByName( const char* eventName, const char* referenceBone, IEntity& characterEntity, CBodyDestrutibilityInstance& instance, const HitInfo& hitInfo )
{
	CRY_ASSERT(eventName != NULL);

	ICharacterInstance* pCharacterInstance = characterEntity.GetCharacter(0);
	if (pCharacterInstance != NULL)
	{
		TDestructionEventId eventId;
		CryHash eventNameId = CryStringUtils::HashString(eventName);

		const int eventCount = (int)m_destructionEvents.size();
		for(eventId = 0; eventId< eventCount; ++eventId)
		{
			if(m_destructionEvents[eventId].eventId == eventNameId)
				break;
		}

		IF_UNLIKELY (eventId >= eventCount)
		{
			return;
		}

		IAttachmentManager* pAttachmentManager = pCharacterInstance->GetIAttachmentManager();
		CRY_ASSERT(pAttachmentManager);

		if (instance.CanTriggerEvent(eventId) && 
			CheckAgainstScriptCondition(characterEntity, eventId))
		{
			ProcessDestructionEvent(characterEntity, instance, hitInfo, eventId, pAttachmentManager, referenceBone);
		}
	}
}

void CBodyDestructibilityProfile::ProcessHealthRatioEvents( IEntity& characterEntity, ICharacterInstance& characterInstance, IAttachmentManager* pAttachmentManager, CBodyDestrutibilityInstance& instance, const HitInfo& hitInfo, const float newHealth )
{
	CRY_ASSERT(instance.GetCurrentHealthRatioIndex() >= 0);
	CRY_ASSERT(pAttachmentManager);

	const bool diedByMikeBurn = (newHealth <= 0.0f) && (CGameRules::EHitType::Mike_Burn == hitInfo.type);
	const float healthRef = newHealth;
	const int healthRatioEventCount = (int)m_healthRatioEvents.size();
	for (int healthRatioIdx = instance.GetCurrentHealthRatioIndex(); healthRatioIdx < healthRatioEventCount; ++healthRatioIdx)
	{
		const SHealthRatioEvent& healthRatioEvent = m_healthRatioEvents[healthRatioIdx];
		const float healthThreshold = healthRatioEvent.healthRatio * instance.GetInstanceInitialHealth();
		
		if(newHealth > healthThreshold)
		{
			break;	//Health ratios are ordered, as soon as we are above the threshold, just bail out
		}
		else
		{
			if (instance.CanTriggerEvent(healthRatioEvent.destructionEvent) && 
				CheckAgainstScriptCondition(characterEntity, healthRatioEvent.destructionEvent))
			{
				ProcessDestructionEvent(characterEntity, instance, hitInfo, healthRatioEvent.destructionEvent, pAttachmentManager, healthRatioEvent.bone.c_str());

				instance.DisableEvent(healthRatioEvent.destructionEvent);
			}

			//Mike death does its own thing with the material settings...
			if (!diedByMikeBurn)
			{
				ReplaceMaterial(characterEntity, characterInstance, instance, healthRatioEvent.material.c_str());
			}

			instance.SetCurrentHealthRatioIndex(healthRatioIdx + 1);

			LogHealthRatioEvent(characterEntity, healthRatioIdx);
		}
	}
}

void CBodyDestructibilityProfile::ProcessDeathByExplosion( IEntity& characterEntity, CBodyDestrutibilityInstance& instance, const HitInfo& hitInfo )
{
	ICharacterInstance* pCharacterInstance = characterEntity.GetCharacter(0);
	if (pCharacterInstance)
	{
		IAttachmentManager* pAttachmentManager = pCharacterInstance->GetIAttachmentManager();
		CRY_ASSERT(pAttachmentManager);

		const bool shouldGib = (hitInfo.damage > m_gibDeath.minExplosionDamage) && (cry_random(0.0f, 1.0f) < m_gibDeath.gibProbability);

		if (shouldGib)
		{
			TDestructionEventId destructionEventId = m_gibDeath.destructionEvent;
			if (instance.CanTriggerEvent(destructionEventId))
			{
				//Trigger event/fx and hide the entity
				ISkeletonPose* pSkeletonPose = pCharacterInstance->GetISkeletonPose();
				IDefaultSkeleton& rIDefaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();
				CRY_ASSERT(pSkeletonPose);

				const QuatT& boneLocation = pSkeletonPose->GetAbsJointByID(rIDefaultSkeleton.GetJointIDByName(m_gibDeath.bone.c_str()));
				const Vec3 effectPosition = characterEntity.GetWorldTM().TransformPoint(boneLocation.t);

				LogMessage(characterEntity, "Death By Explosion [GIB]");

				ProcessDestructionEvent(characterEntity, instance, hitInfo, destructionEventId, pAttachmentManager, m_gibDeath.bone.c_str(), effectPosition);
				characterEntity.Hide(true);
			}
		}
		else
		{
			TDestructionEventId destructionEventId = m_nonGibDeath.destructionEvent;
			if (instance.CanTriggerEvent(destructionEventId) && CheckAgainstScriptCondition(characterEntity, destructionEventId))
			{
				LogMessage(characterEntity, "Death By Explosion");

				ProcessDestructionEvent(characterEntity, instance, hitInfo, destructionEventId, pAttachmentManager, m_nonGibDeath.bone.c_str());
			}
			ReplaceMaterial(characterEntity, *pCharacterInstance, instance, m_nonGibDeath.material.c_str());
		}

		ProcessHealthRatioEvents(characterEntity, *pCharacterInstance, pAttachmentManager, instance, hitInfo, 0.0f);
	}
}

void CBodyDestructibilityProfile::ProcessDeathByPunishHit( IEntity& characterEntity, CBodyDestrutibilityInstance& instance, const HitInfo& hitInfo )
{
	ICharacterInstance* pCharacterInstance = characterEntity.GetCharacter(0);
	if (pCharacterInstance)
	{
		IAttachmentManager* pAttachmentManager = pCharacterInstance->GetIAttachmentManager();
		CRY_ASSERT(pAttachmentManager);

		TDestructionEventId destructionEventId = m_gibDeath.destructionEvent;
		if (instance.CanTriggerEvent(destructionEventId))
		{
			//Trigger event/fx and hide the entity
			ISkeletonPose* pSkeletonPose = pCharacterInstance->GetISkeletonPose();
			IDefaultSkeleton& rIDefaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();
			CRY_ASSERT(pSkeletonPose);

			const QuatT& boneLocation = pSkeletonPose->GetAbsJointByID(rIDefaultSkeleton.GetJointIDByName(m_gibDeath.bone.c_str()));
			const Vec3 effectPosition = characterEntity.GetWorldTM().TransformPoint(boneLocation.t);

			LogMessage(characterEntity, "Death By Explosion [GIB]");

			ProcessDestructionEvent(characterEntity, instance, hitInfo, destructionEventId, pAttachmentManager, m_gibDeath.bone.c_str(), effectPosition);
			characterEntity.Hide(true);
		}

		ProcessHealthRatioEvents(characterEntity, *pCharacterInstance, pAttachmentManager, instance, hitInfo, 0.0f);
	}
}

void CBodyDestructibilityProfile::ProcessDeathByHit( IEntity& characterEntity, ICharacterInstance& characterInstance, CBodyDestrutibilityInstance& instance, const HitInfo& hitInfo )
{
	//Iterate through all attachment/bones and check against health ratios to auto destroy on death

	const int eventIdx = 0;

	IDefaultSkeleton& rIDefaultSkeleton = characterInstance.GetIDefaultSkeleton();

	IAttachmentManager* pAttachmentManager = characterInstance.GetIAttachmentManager();
	CRY_ASSERT(pAttachmentManager);

	// Add some random destruction to the mix, when death by some kind of electrical hit :)
	const uint32 randomize = (hitInfo.type == CGameRules::EHitType::Electricity) ? cry_random(10, 109) : 0;

	LogMessage(characterEntity, "Deadly Hit!");

	//First process attachments
	size_t attachmentIdx = 0;
	const size_t destructibleAttachments = min(m_destructibleAttachmentCount, m_attachments.size());
	while(attachmentIdx < destructibleAttachments)
	{
		CBodyDestrutibilityInstance::SBodyDestructiblePartStatus* pPartStatus = instance.GetAttachmentStatus(attachmentIdx);
		const TDestructionEventId destructionEventId = m_attachments[attachmentIdx].destructionEvents[eventIdx];

		if ((pPartStatus != NULL) && !pPartStatus->IsDestroyed())
		{
			const float inflictedDamage = pPartStatus->GetInitialHealth() - pPartStatus->GetHealth();

			if( ((inflictedDamage > pPartStatus->GetOnDeathHealthThreshold()) || (cry_random(0, 99) < randomize) ) && 
				instance.CanTriggerEvent(destructionEventId) && 
				CheckAgainstScriptCondition(characterEntity, destructionEventId))
			{
				const SDestructibleBodyPart& bodyPart = m_attachments[attachmentIdx];

				IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName(bodyPart.name.c_str());
				const char *effectAttachmentBone = pAttachment ? rIDefaultSkeleton.GetJointNameByID(pAttachment->GetJointID()) : "";

				ProcessDestructionEvent(characterEntity, instance, hitInfo, destructionEventId, pAttachmentManager, effectAttachmentBone);
			}
		}

		++attachmentIdx;
	}

	//Second process bones
	int boneIdx = 0;
	const int destructibleBones = (int)m_bones.size();
	while(boneIdx < destructibleBones)
	{
		CBodyDestrutibilityInstance::SBodyDestructiblePartStatus* pPartStatus = instance.GetBoneStatus(boneIdx);
		const TDestructionEventId destructionEventId = m_bones[boneIdx].destructionEvents[eventIdx];
		if ((pPartStatus != NULL) && !pPartStatus->IsDestroyed())
		{
			const float inflictedDamage = pPartStatus->GetInitialHealth() - pPartStatus->GetHealth();

			if( ((inflictedDamage > pPartStatus->GetOnDeathHealthThreshold()) || (cry_random(0, 99) < randomize) ) && 
				instance.CanTriggerEvent(destructionEventId) && 
				CheckAgainstScriptCondition(characterEntity, destructionEventId))
			{
				const SDestructibleBodyPart& bodyPart = m_bones[boneIdx];

				ProcessDestructionEvent(characterEntity, instance, hitInfo, destructionEventId, pAttachmentManager, bodyPart.name.c_str());
			}
		}
		++boneIdx;
	}

	if (hitInfo.type == CGameRules::EHitType::Mike_Burn)
	{
		ProcessMikeDeath(characterEntity, characterInstance, instance, hitInfo);
	}
}

void CBodyDestructibilityProfile::ProcessMikeDeath( IEntity& characterEntity, ICharacterInstance& characterInstance, CBodyDestrutibilityInstance& instance, const HitInfo& hitInfo )
{
	if (instance.CanTriggerEvent(m_mikeDeath.destructionEvent) && 
		CheckAgainstScriptCondition(characterEntity, m_mikeDeath.destructionEvent))
	{
		IAttachmentManager* pAttachmentManager = characterInstance.GetIAttachmentManager();
		CRY_ASSERT(pAttachmentManager);

		LogMessage(characterEntity, "Mike death");

		ProcessDestructionEvent(characterEntity, instance, hitInfo, m_mikeDeath.destructionEvent, pAttachmentManager, m_mikeDeath.bone.c_str());

		IAttachment* pAnimatedAttachment = (m_mikeDeath.attachmentId != -1) ? pAttachmentManager->GetInterfaceByName(m_attachments[m_mikeDeath.attachmentId].name.c_str()) : NULL;
		if (pAnimatedAttachment)
		{
			pAnimatedAttachment->HideAttachment(0);

			SEntitySpawnParams spawnParams;
			spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
			spawnParams.vPosition = pAnimatedAttachment->GetAttWorldAbsolute().t;
			spawnParams.nFlags = ENTITY_FLAG_NO_SAVE;
			IEntity* pJellyDestroyedEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams);

			if (pJellyDestroyedEntity)
			{
				const EntityId jellyEntityId = pJellyDestroyedEntity->GetId();
				pJellyDestroyedEntity->LoadCharacter(0, m_mikeDeath.object.c_str());

				SEntityPhysicalizeParams physParams;
				physParams.type = PE_ARTICULATED;
				physParams.nSlot = 0;
				pJellyDestroyedEntity->Physicalize(physParams);

				IAttachmentObject* pAnimatedObject = pAnimatedAttachment->GetIAttachmentObject();
				if ((pAnimatedObject != NULL) && (pAnimatedObject->GetAttachmentType() == IAttachmentObject::eAttachment_Entity))
				{
					static_cast<CEntityAttachment*>(pAnimatedObject)->SetEntityId(jellyEntityId);
				}
				else
				{
					CEntityAttachment* pJellyAttachment = new CEntityAttachment();
					CRY_ASSERT(pJellyAttachment);
					pJellyAttachment->SetEntityId(jellyEntityId);
					
					pAnimatedAttachment->AddBinding(pJellyAttachment);
				}

				instance.InitializeMikeDeath(
					jellyEntityId,
					m_mikeDeath.alphaTestFadeOutTimeOut,
					m_mikeDeath.alphaTestFadeOutDelay,
					m_mikeDeath.alphaTestFadeOutMaxAlpha);

				ICharacterInstance* pAnimatedObjectChr = pJellyDestroyedEntity->GetCharacter(0);
				if (pAnimatedObjectChr)
				{
					IMaterial* pCharacterMaterial = pAnimatedObjectChr->GetIMaterial();
					if (pCharacterMaterial)
					{
						IMaterial* pCloneMaterial = gEnv->p3DEngine->GetMaterialManager()->CloneMaterial(pCharacterMaterial);
						pAnimatedObjectChr->SetIMaterial_Instance(pCloneMaterial);
					}

					ISkeletonAnim* pAnimatedObjectSkeletonAnim = pAnimatedObjectChr->GetISkeletonAnim();
					if (pAnimatedObjectSkeletonAnim)
					{
						CryCharAnimationParams animationParams;
						animationParams.m_nLayerID = 0;
						animationParams.m_fTransTime = 0.15f;
						animationParams.m_nFlags = 0;

						pAnimatedObjectSkeletonAnim->StartAnimation(m_mikeDeath.animation.c_str(), animationParams);
					}
				}
			}
		}
	}
}

void CBodyDestructibilityProfile::HideAttachment( IAttachmentManager* pAttachmentManager, const char* attachmentName, uint32 hide )
{
	IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName(attachmentName);
	if (pAttachment)
	{
		pAttachment->HideAttachment(hide);
	}
}

void CBodyDestructibilityProfile::ReplaceMaterial( IEntity& characterEntity, ICharacterInstance& characterInstance, CBodyDestrutibilityInstance& instance, const char* material )
{
	IMaterial* pReplacementMaterial = GetMaterial(material);
	if (pReplacementMaterial)
	{
		instance.ReplaceMaterial( characterEntity, characterInstance, *pReplacementMaterial);
	}
}

void CBodyDestructibilityProfile::ResetMaterials( IEntity& characterEntity, ICharacterInstance& characterInstance, CBodyDestrutibilityInstance& instance )
{
	instance.ResetMaterials( characterEntity, characterInstance );
}

IMaterial* CBodyDestructibilityProfile::GetMaterial( const char* materialName ) const
{
	const bool isValidName = (materialName != NULL) && (materialName[0]);
	if (isValidName)
	{
		TReplacementMaterials::const_iterator cit = m_replacementMaterials.find(CryStringUtils::HashString(materialName));
		
		return (cit != m_replacementMaterials.end()) ? cit->second.get() : NULL; 
	}
	
	return NULL;
}

bool CBodyDestructibilityProfile::GetDestructibleAttachment( CryHash hashId, SBodyPartQueryResult& bodyPart ) const
{
	TDestructibleBodyParts::const_iterator attachmentIter = m_attachments.begin();
	TDestructibleBodyParts::const_iterator attachmentsEnd = m_attachments.end();
	size_t attachmentIndex = 0;

	while ((attachmentIter != attachmentsEnd) && (attachmentIter->hashId != hashId) && (attachmentIndex < m_destructibleAttachmentCount))
	{
		++attachmentIter;
		++attachmentIndex;
	}

	if ((attachmentIter != attachmentsEnd) && (attachmentIndex < m_destructibleAttachmentCount))
	{
		bodyPart.pPart = &(*attachmentIter);
		bodyPart.index = attachmentIndex;
		return true;
	}

	return false;
}

bool CBodyDestructibilityProfile::GetDestructibleBone( CryHash hashId, SBodyPartQueryResult& bodyPart ) const
{
	TDestructibleBodyParts::const_iterator boneIter = m_bones.begin();
	TDestructibleBodyParts::const_iterator bonesEnd = m_bones.end();
	int boneIndex = 0;

	while ((boneIter != bonesEnd) && (boneIter->hashId != hashId))
	{
		++boneIter;
		++boneIndex;
	}

	if (boneIter != bonesEnd)
	{
		bodyPart.pPart = &(*boneIter);
		bodyPart.index = boneIndex;
		return true;
	}

	return false;
}

void CBodyDestructibilityProfile::GetMemoryUsage( ICrySizer *pSizer ) const
{
	pSizer->AddObject(this, sizeof(*this));
	
	pSizer->AddContainer(m_replacementMaterials);	
	pSizer->AddContainer(m_cachedCharacterInstaces);
	
	pSizer->AddContainer(m_attachments);
	pSizer->AddContainer(m_bones);
	pSizer->AddContainer(m_destructionEvents);
	pSizer->AddContainer(m_healthRatioEvents);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if !defined(_RELEASE)

void CBodyDestructibilityProfile::DebugInstance( IEntity& characterEntity, CBodyDestrutibilityInstance& instance )
{
	const float headerHorizontalOffset = 50.0f;
	const float horizontalOffsetLv1 = 70.0f;
	const float horizontalOffsetLv2 = 85.0f;

	const float colorWhite[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	const float colorRed[4] = {1.0f, 0.0f, 0.0f, 1.0f};
	const float colorGreen[4] = {0.0f, 1.0f, 0.0f, 1.0f};

	float lineOffset = 50.0f;

	ICharacterInstance* pCharacter = characterEntity.GetCharacter(0);
	if (!pCharacter)
		return;

	IAttachmentManager* pAttachmentManager = pCharacter->GetIAttachmentManager();
	if (!pAttachmentManager)
		return;

	//////////////////////////////////////////////////////////////////////////
	/// Display destructible parts and attachments status

	IRenderAuxText::Draw2dLabel(headerHorizontalOffset, lineOffset, 1.5f, colorWhite, false, "===== Destructibility status of '%s' =====", characterEntity.GetName());
	lineOffset += 20.0f;

	IRenderAuxText::Draw2dLabel(headerHorizontalOffset, lineOffset, 1.5f, colorWhite, false, "Attachments");
	lineOffset += 20.0f;

	for (size_t attachmentIdx = 0; attachmentIdx < m_attachments.size(); ++attachmentIdx)
	{
		CBodyDestrutibilityInstance::SBodyDestructiblePartStatus* pPartStatus = instance.GetAttachmentStatus(attachmentIdx);

		if (pPartStatus)
		{
			const bool isDestroyable = attachmentIdx < m_destructibleAttachmentCount;
			if (isDestroyable)
			{
				const bool isDestroyed = (isDestroyable) && pPartStatus->IsDestroyed();
				IRenderAuxText::Draw2dLabel(horizontalOffsetLv1, lineOffset, 1.4f, isDestroyed ? colorRed : colorGreen, false, "Destroyable Attachment [%" PRISIZE_T "] : %s", attachmentIdx, m_attachments[attachmentIdx].name.c_str());
				lineOffset += 15.0f;

				IRenderAuxText::Draw2dLabel(horizontalOffsetLv2, lineOffset, 1.3f, isDestroyed ? colorRed : colorGreen, false, "Health: Initial %.2f, Current %.2f, onDeathThreshold %.2f",
										pPartStatus->GetInitialHealth(), pPartStatus->GetHealth(), pPartStatus->GetOnDeathHealthThreshold());
				lineOffset += 15.0f;
			}
			else
			{
				IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName(m_attachments[attachmentIdx].name.c_str());
				const bool isVisible = pAttachment ? (pAttachment->IsAttachmentHidden() == 0) : false;

				IRenderAuxText::Draw2dLabel(horizontalOffsetLv1, lineOffset, 1.4f, isVisible ? colorGreen : colorRed, false, "Attachment [%" PRISIZE_T "] : %s", attachmentIdx, m_attachments[attachmentIdx].name.c_str());
				lineOffset += 15.0f;
			}
		}
		else
		{
			IRenderAuxText::Draw2dLabel(horizontalOffsetLv1, lineOffset, 1.4f, colorRed, false, "Attachment [%" PRISIZE_T "] : %s Status not found!", attachmentIdx, m_attachments[attachmentIdx].name.c_str());
			lineOffset += 15.0f;
		}
	}

	IRenderAuxText::Draw2dLabel(headerHorizontalOffset, lineOffset, 1.5f, colorWhite, false, "Bones");
	lineOffset += 20.0f;

	for (size_t boneIdx = 0; boneIdx < m_bones.size(); ++boneIdx)
	{
		CBodyDestrutibilityInstance::SBodyDestructiblePartStatus* pPartStatus = instance.GetBoneStatus(boneIdx);

		if (pPartStatus)
		{
			const bool isDestroyed = pPartStatus->IsDestroyed();
			IRenderAuxText::Draw2dLabel(horizontalOffsetLv1, lineOffset, 1.4f, isDestroyed ? colorRed : colorGreen, false, "Destroyable Bone [%" PRISIZE_T "] : %s", boneIdx, m_bones[boneIdx].name.c_str());
			lineOffset += 15.0f;

			IRenderAuxText::Draw2dLabel(horizontalOffsetLv2, lineOffset, 1.3f, isDestroyed ? colorRed : colorGreen, false, "Health: Initial %.2f, Current %.2f, onDeathThreshold %.2f",
				pPartStatus->GetInitialHealth(), pPartStatus->GetHealth(), pPartStatus->GetOnDeathHealthThreshold());
			lineOffset += 15.0f;
		}
		else
		{
			IRenderAuxText::Draw2dLabel(horizontalOffsetLv1, lineOffset, 1.4f, colorRed, false, "Destroyable Bone [%" PRISIZE_T "] : %s Status not found!", boneIdx, m_bones[boneIdx].name.c_str());
			lineOffset += 15.0f;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	/// Display event info

	lineOffset = 70.0f;

	IRenderAuxText::Draw2dLabel(headerHorizontalOffset + 600.0f, lineOffset, 1.5f, colorWhite, false, "Events");
	lineOffset += 20.0f;

	for (size_t eventIdx = 0; eventIdx < m_destructionEvents.size(); ++eventIdx)
	{
		const bool isEventEabled = instance.CanTriggerEvent(eventIdx);

		IRenderAuxText::Draw2dLabel(horizontalOffsetLv1 + 600.0f, lineOffset, 1.4f, isEventEabled ? colorGreen : colorRed, false, "Event [%" PRISIZE_T "] : %s", eventIdx, m_destructionEvents[eventIdx].name.c_str());
		lineOffset += 15.0f;
	}


	IRenderAuxText::Draw2dLabel(headerHorizontalOffset + 600.0f, lineOffset, 1.5f, colorWhite, false, "Health ratio events");
	lineOffset += 20.0f;

	for (size_t eventIdx = 0; eventIdx < m_healthRatioEvents.size(); ++eventIdx)
	{
		IRenderAuxText::Draw2dLabel(horizontalOffsetLv1 + 600.0f, lineOffset, 1.4f, (static_cast<size_t>(instance.GetCurrentHealthRatioIndex()) > eventIdx) ? colorRed : colorGreen, false, "Event [%" PRISIZE_T "] : %.2f", eventIdx, m_healthRatioEvents[eventIdx].healthRatio * 100.0f);
		lineOffset += 15.0f;
	}
}

void CBodyDestructibilityProfile::LogDamage( IEntity& characterEntity, const char* customLog, const char* affectedPart, float inflictedDamage, float hitTypeScale, bool destroyed )
{
	if (!CBodyManagerCVars::IsBodyDestructionDebugEnabled())
		return;

	if (CBodyManagerCVars::IsBodyDestructionDebugFilterEnabled() && !CBodyManagerCVars::IsBodyDestructionDebugFilterFor(characterEntity.GetName()))
		return;

	static CryFixedStringT<256>	debugMessage;
	static const ColorF whiteColor (1.0f, 1.0f, 1.0f, 1.0f);

	debugMessage.Format("%s: DestructiblePart '%s' Damage '%.2f' HitTypeScale '%.2f' (%s)", customLog, affectedPart, inflictedDamage, hitTypeScale, destroyed ? "Destroyed" : "Alive");

	g_pGame->GetIGameFramework()->GetIPersistantDebug()->Add2DText(debugMessage.c_str(), 1.2f, whiteColor, 5.0f);
}

void CBodyDestructibilityProfile::LogDestructionEvent( IEntity& characterEntity, const char* eventName )
{
	if (!CBodyManagerCVars::IsBodyDestructionDebugEnabled())
		return;

	if (CBodyManagerCVars::IsBodyDestructionDebugFilterEnabled() && !CBodyManagerCVars::IsBodyDestructionDebugFilterFor(characterEntity.GetName()))
		return;

	static CryFixedStringT<64>	debugMessage;
	static const ColorF greyColor (0.5f, 0.5f, 0.5f, 1.0f);

	debugMessage.Format("Event triggered: '%s'", eventName);

	g_pGame->GetIGameFramework()->GetIPersistantDebug()->Add2DText(debugMessage.c_str(), 1.2f, greyColor, 5.0f);
}

void CBodyDestructibilityProfile::LogHealthRatioEvent( IEntity& characterEntity, int ratioEventIndex )
{
	if (!CBodyManagerCVars::IsBodyDestructionDebugEnabled())
		return;

	if (CBodyManagerCVars::IsBodyDestructionDebugFilterEnabled() && !CBodyManagerCVars::IsBodyDestructionDebugFilterFor(characterEntity.GetName()))
		return;

	static CryFixedStringT<64>	debugMessage;
	static const ColorF greenColor (0.0f, 0.1f, 0.0f, 1.0f);

	debugMessage.Format("HealthRatio triggered: [%d]", ratioEventIndex);

	g_pGame->GetIGameFramework()->GetIPersistantDebug()->Add2DText(debugMessage.c_str(), 1.2f, greenColor, 5.0f);
}

void CBodyDestructibilityProfile::LogMessage( IEntity& characterEntity, const char* message )
{
	if (!CBodyManagerCVars::IsBodyDestructionDebugEnabled())
		return;

	if (CBodyManagerCVars::IsBodyDestructionDebugFilterEnabled() && !CBodyManagerCVars::IsBodyDestructionDebugFilterFor(characterEntity.GetName()))
		return;

	static CryFixedStringT<64>	debugMessage;
	static const ColorF whiteColor (1.0f, 1.0f, 1.0f, 1.0f);

	debugMessage.Format("%s", message);

	g_pGame->GetIGameFramework()->GetIPersistantDebug()->Add2DText(debugMessage.c_str(), 1.2f, whiteColor, 5.0f);
}

#endif