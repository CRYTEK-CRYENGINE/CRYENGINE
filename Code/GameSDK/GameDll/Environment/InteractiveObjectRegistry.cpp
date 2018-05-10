// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Interactive objects are registered here with required params

-------------------------------------------------------------------------
History:
- 10:12:2009: Created by Benito G.R.

*************************************************************************/

#include "StdAfx.h"
#include "InteractiveObjectRegistry.h"
#include <IItemSystem.h>

#include <ICryMannequin.h>
#include <Mannequin/ProceduralClipsPositioning.h>
#include "PlayerAnimation.h"
#include "RecordingSystem.h"

static const char *MANN_CONTROLLERDEF_FILENAME			 = "Animations/Mannequin/ADB/PlayerControllerDefs.xml";
static const char *MANN_INTERACTION_FILENAME_PLAYER  = "Animations/Mannequin/ADB/PlayerAnims1P.adb";
static const char *MANN_INTERACTION_FILENAME_OBJECTS = "Animations/Mannequin/ADB/interactiveObjects.adb";

void CInteractiveObjectRegistry::Init()
{
	m_clipLocatorHash = IProceduralClipFactory::THash("PositionAdjustTargetLocator");

	IMannequin &mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	m_pDatabasePlayer = mannequinSys.GetAnimationDatabaseManager().Load(MANN_INTERACTION_FILENAME_PLAYER);
	m_pDatabaseObject = mannequinSys.GetAnimationDatabaseManager().Load(MANN_INTERACTION_FILENAME_OBJECTS);
	m_pControllerDef  = mannequinSys.GetAnimationDatabaseManager().LoadControllerDef(MANN_CONTROLLERDEF_FILENAME);

	if (m_pDatabasePlayer)
	{
		m_interactFragID = m_pDatabasePlayer->GetFragmentID("Interact");
	}
}

void CInteractiveObjectRegistry::Shutdown()
{
	//anim databases are managed by the manager so just NULL our pointers
	m_pDatabasePlayer = NULL;
	m_pDatabaseObject = NULL;
	m_pControllerDef = NULL;

	m_entityToTag.clear();
}

void CInteractiveObjectRegistry::OnInteractiveObjectSutdown( const EntityId entityId )
{
	m_entityToTag.erase(entityId);
}

TagID CInteractiveObjectRegistry::GetInteractionTag(const char* interactionName) const
{
	if (m_pDatabasePlayer)
	{
		if (m_interactFragID != TAG_ID_INVALID)
		{
			const CTagDefinition *tagDef = m_pDatabasePlayer->GetFragmentDefs().GetSubTagDefinition(m_interactFragID);
			return tagDef->Find(interactionName);
		}
	}

	return TAG_ID_INVALID;
}

const CInteractiveObjectRegistry::SMannequinParams CInteractiveObjectRegistry::GetInteractionTagForEntity(const EntityId entityId, const int interactionIndex) const
{
	TEntityToTagMap::const_iterator entityCit = m_entityToTag.find(entityId);

	return (entityCit != m_entityToTag.end()) ? entityCit->second : SMannequinParams();
}

void CInteractiveObjectRegistry::QueryInteractiveActionOptions( const EntityId entityId, const TagID interactionTag, const TagID stateTagId, std::vector<SInteraction> &options)
{
	if (m_pDatabasePlayer)
	{
		SFragTagState fragTagState;

		if ((m_interactFragID != TAG_ID_INVALID) && (interactionTag != TAG_ID_INVALID))
		{
			const CTagDefinition *tagDef = m_pDatabasePlayer->GetFragmentDefs().GetSubTagDefinition(m_interactFragID);
			const bool hasState = (stateTagId != TAG_ID_INVALID);

			m_entityToTag[entityId].interactionTagID						= interactionTag;
			m_entityToTag[entityId].stateTagID = stateTagId;

			SFragTagState fragTagStateMatch;
			tagDef->Set(fragTagState.fragmentTags, interactionTag, true);
			if (hasState)
			{
				tagDef->Set(fragTagState.fragmentTags, stateTagId, true);
			}
			
			uint32 tagSetIdx;
			SFragmentQuery fragQuery(m_interactFragID, fragTagState);
			int32 numOptions = m_pDatabasePlayer->FindBestMatchingTag(fragQuery, &fragTagStateMatch, &tagSetIdx);

			if (hasState && !tagDef->IsSet(fragTagStateMatch.fragmentTags, stateTagId))
			{
				numOptions = 0;
			}

			for (int32 i=0; i<numOptions; i++)
			{
				const CFragment *fragment = m_pDatabasePlayer->GetEntry(m_interactFragID, tagSetIdx, i);

				bool foundLocator = false;
				const uint32 numProcLayers = fragment->m_procLayers.size();
				for (uint32 k=0; (k<numProcLayers) && !foundLocator; k++)
				{
					const uint32 numClips = fragment->m_procLayers[k].size();
					for (uint32 c=0; (c<numClips) && !foundLocator; c++)
					{
						const SProceduralEntry &entry = fragment->m_procLayers[k][c];
						if (entry.typeNameHash == m_clipLocatorHash)
						{
							const SProceduralClipPosAdjustTargetLocatorParams* const pParams = static_cast<const SProceduralClipPosAdjustTargetLocatorParams*>(entry.pProceduralParams.get());
							CRY_ASSERT(pParams);

							foundLocator = true;
							SInteraction interaction;
							interaction.helper      = pParams->targetJointName.c_str();
							interaction.targetStateTagID = tagDef->Find(pParams->targetStateName.crc);

							options.push_back(interaction);
						}
					}
				}
			}
		}
	}
}

void CInteractiveObjectRegistry::ApplyInteractionToObject(IEntity *pEntity, const TagID interactionTypeTag, const int interactionIndex) const
{
	if (m_pDatabaseObject && m_pControllerDef)
	{
		IMannequin &mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
		SAnimationContext animContext(*m_pControllerDef);

		IActionController *pActionController = mannequinSys.CreateActionController(pEntity, animContext);

		int scopeContextID = m_pControllerDef->m_scopeContexts.Find("SlaveObject");
		pActionController->SetScopeContext(scopeContextID, *pEntity, pEntity->GetCharacter(0), m_pDatabaseObject);
		TagState fragTags = TAG_STATE_EMPTY;
		const CTagDefinition *pTagDef = m_pControllerDef->GetFragmentTagDef(m_interactFragID);
		if (pTagDef)
		{
			pTagDef->Set(fragTags, interactionTypeTag, true);
		}
		IAction *pAction = new TAction<SAnimationContext>(0, m_interactFragID, fragTags);
		pAction->SetOptionIdx(interactionIndex);
		pActionController->Queue(*pAction);

		// Set the time increments to non-zero to force the ActionController::Update() to drive the animation to the end.
		// When time increment is zero, animation position will not update. This will be changed to a simpler process by Tom Berry at some point.
		const uint32 totalScopes = pActionController->GetTotalScopes();
		for(uint32 s=0; s<totalScopes; ++s)
		{
			pActionController->GetScope(s)->IncrementTime(0.001f);
		}
		pActionController->Update(1000.0f);
		// "false" here leaves the anim on the transition queue in the animation system so it isn't cleared on pActionController->Release().
		pActionController->ClearScopeContext(scopeContextID, false);

		pActionController->Release();

		CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
		if (pRecordingSystem)
		{
			pRecordingSystem->OnInteractiveObjectFinishedUse(pEntity->GetId(), interactionTypeTag, interactionIndex);
		}

	}
}







