// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 'Corpse Manager' used to control number of corpses in a level

-------------------------------------------------------------------------
History:
- 08:04:2010   12:00 : Created by Claire Allan

*************************************************************************/
#include "StdAfx.h"
#include "CorpseManager.h"
#include "GameCVars.h"
#include "GameRules.h"
#include "RecordingSystem.h"

static const float k_awakeDecayTimeConstant = 1.5f;    // Decay the awake timer to zero, which this constant (smaller the value the quicker the decay)
static const float k_awakeForceSleepTime = 1.5f;       // After this time try a forced sleep
static const float k_awakeForceDeleteTime = 4.0f;      // After this time just delete the corpse
static const float k_distUpdateCorpsePos = 0.2f;       // If the corpse moves by this distance, then update the registered pos, and dont inc the awake timer
static const float k_deleteAgeIfOverlap = 10.f;        // If the corpse is awake and overlapping another corpse then delete it if its an old corpse
static const float k_deleteTimeIfOverlap = 2.5f;       // If the corpse has been awake for this time and is overlapping another corpse then delete it


CCorpseManager::CCorpseManager() 
: m_bThermalVisionOn(false) 
{
	if(CGameRules* pGameRules = g_pGame->GetGameRules())
	{
		pGameRules->RegisterRoundsListener(this);
	}
};

CCorpseManager::~CCorpseManager()
{
	if(CGameRules* pGameRules = g_pGame->GetGameRules())
	{
		pGameRules->UnRegisterRoundsListener(this);
	}
}

void CCorpseManager::Update(float frameTime)
{
	int numberCorpses = m_activeCorpses.size();

	while(numberCorpses > 0 && numberCorpses > g_pGameCVars->g_corpseManager_maxNum)
	{
		RemoveACorpse();
		numberCorpses--;
	}

	UpdateCorpses(frameTime);

	// remove timed out corpses
	if(g_pGameCVars->g_corpseManager_timeoutInSeconds > 0.0f)
	{
		for(unsigned int iCorpse = 0; iCorpse < m_activeCorpses.size(); /* nothing */)
		{
			if(m_activeCorpses[iCorpse].age >= g_pGameCVars->g_corpseManager_timeoutInSeconds)
			{
				const EntityId corpseId = m_activeCorpses[iCorpse].corpseId;
				gEnv->pEntitySystem->RemoveEntity(m_activeCorpses[iCorpse].corpseId);
				m_activeCorpses.removeAt(iCorpse);
				OnRemovedCorpse(corpseId);
			}
			else
			{
				++iCorpse;
			}
		}
	}
};

void CCorpseManager::RegisterCorpse(EntityId corpseId, Vec3 corpsePos, float thermalVisionHeat)
{
	int numberCorpses = m_activeCorpses.size();

	if(numberCorpses > 0 && numberCorpses > g_pGameCVars->g_corpseManager_maxNum)
	{
		RemoveACorpse();
	}

	if(IEntity* pEntity = gEnv->pEntitySystem->GetEntity(corpseId))
	{
		if(IPhysicalEntity* pPhysEnt = pEntity->GetPhysics())
		{
			// Log collisions for moving platform support!
			pe_params_flags flags;
			flags.flagsOR = pef_log_collisions;
			pPhysEnt->SetParams(&flags);
		}
	}

	m_activeCorpses.push_back(SCorpseInfo(corpseId, corpsePos, thermalVisionHeat));
}

void CCorpseManager::RemoveACorpse()
{
	float farthestDistSq = 0.f;
	int farthestCorpse = 0;		// default to removing corpse zero
	bool foundOffScreen = false;
	Vec3 clientPos(0.f,0.f,0.f);

	if (m_activeCorpses.size() == 0)
	{
		return;
	}

	IActor* clientActor = g_pGame->GetIGameFramework()->GetClientActor();

	if(clientActor)
	{
		clientPos = clientActor->GetEntity()->GetWorldPos();
	}

	int numCorpses = m_activeCorpses.size();
	for(int i = 0; i < numCorpses; i++)
	{
		Sphere corpseSphere(m_activeCorpses[i].corpsePos, 2.0f);
		bool visible = gEnv->p3DEngine->GetRenderingCamera().IsSphereVisible_F(corpseSphere);

		if(!foundOffScreen || !visible)
		{
			float distSq = m_activeCorpses[i].corpsePos.GetSquaredDistance(clientPos);

			if((!foundOffScreen && !visible) || (distSq > farthestDistSq))
			{
				foundOffScreen = !visible;
				farthestDistSq = distSq;
				farthestCorpse = i;
			}
		}
	}

	const EntityId corpseId = m_activeCorpses[farthestCorpse].corpseId;
	gEnv->pEntitySystem->RemoveEntity(corpseId);
	m_activeCorpses.removeAt(farthestCorpse);
	OnRemovedCorpse(corpseId);
}

void CCorpseManager::KeepAwake( const EntityId corpseId, IPhysicalEntity* pCorpsePhys )
{
	const int numCorpses = m_activeCorpses.size();
	for(int i = 0; i < numCorpses; i++)
	{
		if(m_activeCorpses[i].corpseId == corpseId)
		{
			if((m_activeCorpses[i].flags&eCF_NeverSleep)==0)
			{
				m_activeCorpses[i].flags |= eCF_NeverSleep;
				if(pCorpsePhys)
				{
					pe_action_awake paa;
					paa.bAwake = 1;
					paa.minAwakeTime = k_awakeForceDeleteTime - m_activeCorpses[i].awakeTime;;
					pCorpsePhys->Action(&paa);
				}
			}
			break;
		}
	}
}

void CCorpseManager::ClearCorpses()
{
	const int numCorpses = m_activeCorpses.size();
	for(int i = 0; i < numCorpses; i++)
	{
		gEnv->pEntitySystem->RemoveEntity(m_activeCorpses[i].corpseId);
		OnRemovedCorpse(m_activeCorpses[i].corpseId);
	}

	m_activeCorpses.clear();
}

void CCorpseManager::UpdateCorpses(float frameTime)
{
	// Early out if no corpses
	if(m_activeCorpses.size() == 0)
	{
		return;
	}

	float minHeatValue = g_pGameCVars->g_corpseManager_thermalHeatMinValue;
	float heatFadeDuration = g_pGameCVars->g_corpseManager_thermalHeatFadeDuration;
	
	int deleteIdx = -1;
	IEntity* pCorpseEntities[MAX_CORPSES];
	AABB corpseBBox[MAX_CORPSES];

	int numCorpses = m_activeCorpses.size();
	for(int i = 0; i < numCorpses; i++)
	{
		if (pCorpseEntities[i] = gEnv->pEntitySystem->GetEntity(m_activeCorpses[i].corpseId))
		{
			pCorpseEntities[i]->GetWorldBounds(corpseBBox[i]);
		}
	}

	for(int i = 0; i < numCorpses; i++)
	{
		// Update age counting up
		float corpseAgePrevFrame = m_activeCorpses[i].age;
		m_activeCorpses[i].age += frameTime;

		IEntity* pCorpseEntity = pCorpseEntities[i];

		if(pCorpseEntity)
		{
			// Update thermal vision heat for corpse
			if(m_bThermalVisionOn && (corpseAgePrevFrame < heatFadeDuration))
			{
				IEntityRender *pCorpseRenderProxy = (pCorpseEntity->GetRenderInterface());
				if(pCorpseRenderProxy)
				{
					if(m_activeCorpses[i].age < heatFadeDuration)
					{
						float heatFadeRatio = m_activeCorpses[i].age / heatFadeDuration;
						float heatValue = LERP(m_activeCorpses[i].thermalVisionHeat,minHeatValue,heatFadeRatio);
						//pCorpseRenderProxy->SetVisionParams(heatValue, heatValue, heatValue, heatValue);
					}
					else
					{
						//pCorpseRenderProxy->SetVisionParams(minHeatValue, minHeatValue, minHeatValue, minHeatValue);
					}
				}
			}

			//  Sleep checking
			IPhysicalEntity* pPhysEnt = pCorpseEntity->GetPhysics();
			if (pPhysEnt)
			{
				pe_status_dynamics psd;
				if (pPhysEnt->GetStatus(&psd))
				{
					pe_status_awake psa;
					if (pPhysEnt->GetStatus(&psa))
					{
						// Force a skeleton update to make sure it isn't left out of sync.
						if(ICharacterInstance* pCharInst = pCorpseEntity->GetCharacter(0))
						{
							if(ISkeletonPose* pSkelPose = pCharInst->GetISkeletonPose())
							{
								pSkelPose->SetForceSkeletonUpdate(1);
							}
						}

						// Ragdoll is Awake
						if(m_activeCorpses[i].flags&eCF_NeverSleep)
						{
							m_activeCorpses[i].awakeTime += frameTime;
							if (m_activeCorpses[i].awakeTime > k_awakeForceDeleteTime)
							{
								deleteIdx = i;
							}
						}
						else
						{
							if((m_activeCorpses[i].corpsePos-psd.centerOfMass).GetLengthSquared() > sqr(k_distUpdateCorpsePos))
							{
								// Valid movement that is likely not jitter
								m_activeCorpses[i].corpsePos = psd.centerOfMass;
								m_activeCorpses[i].awakeTime = 0.f;
							}
							else
							{
								m_activeCorpses[i].awakeTime += frameTime;
								if (m_activeCorpses[i].awakeTime > k_awakeForceSleepTime)
								{
									// Force to sleep 
									m_activeCorpses[i].corpsePos = psd.centerOfMass;
									pe_action_awake paa;
									paa.bAwake = 0;
									pPhysEnt->Action(&paa);
								}
								if (m_activeCorpses[i].awakeTime > k_awakeForceDeleteTime)
								{
									deleteIdx = i;
								}
							}
						}
					}
					else
					{
						// Wake up again if marked to never sleep.
						if(m_activeCorpses[i].flags&eCF_NeverSleep)
						{
							m_activeCorpses[i].corpsePos = psd.centerOfMass;
							pe_action_awake paa;
							paa.bAwake = 1;
							paa.minAwakeTime = k_awakeForceDeleteTime - m_activeCorpses[i].awakeTime;
							pPhysEnt->Action(&paa);
						}
						// Keep slept.
						else if (m_activeCorpses[i].awakeTime<0.1f)
						{
							m_activeCorpses[i].awakeTime = 0.f;
							m_activeCorpses[i].corpsePos = psd.centerOfMass;
						}
						else
						{
							// Exponentially decay the time
							m_activeCorpses[i].awakeTime *= k_awakeDecayTimeConstant/(frameTime+k_awakeDecayTimeConstant);
						}
					}
				}
			}
		}
		
		// Kill overlapping, awake corpses
		if (deleteIdx==-1)
		{
			for (int j=i+1; j<numCorpses; j++)
			{
				if (pCorpseEntities[j] && Overlap::AABB_AABB(corpseBBox[i], corpseBBox[j]))
				{
					if ((m_activeCorpses[i].age > k_deleteAgeIfOverlap) || (m_activeCorpses[i].awakeTime>k_deleteTimeIfOverlap) || (m_activeCorpses[j].awakeTime>k_deleteTimeIfOverlap))
					{
						deleteIdx = i;
						break;
					}
				}
			}
		}
	}

	if (deleteIdx>=0)
	{
		const EntityId corpseId = m_activeCorpses[deleteIdx].corpseId;
		gEnv->pEntitySystem->RemoveEntity(m_activeCorpses[deleteIdx].corpseId);
		m_activeCorpses.removeAt(deleteIdx);
		OnRemovedCorpse(corpseId);
	}
}

void CCorpseManager::OnRemovedCorpse( const EntityId corpseId )
{
	if(CRecordingSystem *pRecordingSystem = g_pGame->GetRecordingSystem())
	{
		pRecordingSystem->OnCorpseRemoved(corpseId);
	}
}
