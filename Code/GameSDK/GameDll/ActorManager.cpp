// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ActorManager.h"

#include "GameRules.h"
#include "PlayerVisTable.h"

#include "IActorSystem.h"


CActorManager * CActorManager::GetActorManager()
{
	static CActorManager s_actorManager;
	return &s_actorManager;
}

CActorManager::CActorManager()
{
	m_startMemoryPtr			= NULL;
	m_startMemoryPtrAlign = NULL;

	Reset(true);
}

CActorManager::~CActorManager()
{
	Reset(false);
}

void CActorManager::Reset(bool bReallocate)
{
	m_iNumActorsTracked								= 0;
	m_iNumActorsTrackedIncLocalPlayer = 0;
	m_iMaxTrackedActors								= 0;

#if USE_ACTOR_PTR_LOOKUP
	m_actorPtrToIndex.clear();
#endif
#if USE_ENTITY_PTR_LOOKUP
	m_entityPtrToIndex.clear();
#endif

	//Re-allocate
	if(bReallocate)
	{
		ReallocateMemoryForNActors(kMaxActorsTrackedInit);
	}
	else
	{
		if(m_startMemoryPtr)
		{
			delete [] m_startMemoryPtr;
			m_startMemoryPtr			= NULL;
			m_startMemoryPtrAlign = NULL;
			m_actorEntityIds			= NULL;
			m_actorPositions			= NULL;
			m_actorTeamNums				= NULL;
			m_actorSpectatorModes	= NULL;
			m_actorHealth					= NULL;
		}
	}
}

size_t CActorManager::GetMemoryRequiredForNActors(int iNumActors)
{
	size_t memoryForOneActor = sizeof(EntityId)								//m_actorEntityIds
															+ sizeof(TActorPos)						//m_actorPositions
															+ sizeof(TTeamNum)						//m_actorTeamNums
															+ sizeof(TActorHealth)				//m_actorHealth
															+ sizeof(TActorSpectatorMode);//m_actorSpectatorModes
	
	return (memoryForOneActor * iNumActors)	+ 128;	//over-allocate to avoid prefetch issues
}

void CActorManager::ReallocateMemoryForNActors(int iNumActorsRequired)
{
	int iNumActors = (iNumActorsRequired + 15) & ~15;

	if(m_startMemoryPtr)
	{
		delete [] m_startMemoryPtr;
		m_startMemoryPtr			= NULL;
		m_startMemoryPtrAlign = NULL;
	}

	size_t memoryNeeded = GetMemoryRequiredForNActors(iNumActors);

	char * pMemory = new char[memoryNeeded + 128];	//Additional 128 for alignment 'cause we're using new :(
	memset(pMemory, 0, memoryNeeded+128);
	m_startMemoryPtr			= pMemory;
	
	pMemory								= (char*)(((INT_PTR)pMemory-1&~127)+128);

	assert((((INT_PTR)pMemory) & 127) == 0);	//Double check 128 byte align

	m_startMemoryPtrAlign = pMemory;

	//Fix up sub-pointers
	size_t offset = 0;
	m_actorEntityIds			= reinterpret_cast<EntityId*>			(pMemory + offset);
	offset += (sizeof(EntityId) * iNumActors); 
	
	m_actorPositions			= reinterpret_cast<TActorPos*>		(pMemory + offset);
	offset += (sizeof(TActorPos) * iNumActors);

	m_actorHealth					= reinterpret_cast<TActorHealth*>	(pMemory + offset);
	offset += (sizeof(TActorHealth) * iNumActors);

 	m_actorTeamNums				= reinterpret_cast<TTeamNum*>			(pMemory + offset);
	offset += (sizeof(TTeamNum) * iNumActors);

	m_actorSpectatorModes	= reinterpret_cast<TActorSpectatorMode*> (pMemory + offset);
	offset += (sizeof(TActorSpectatorMode) * iNumActors);

	m_iMaxTrackedActors = iNumActors;
}

void CActorManager::Update(float dt)
{
	ResetLine128(m_actorHealth, 0);
	ResetLine128(m_actorTeamNums, 0);
	ResetLine128(m_actorEntityIds, 0);

	//Need to iterate over the position array to zero enough cache
	ResetLine128(m_actorPositions, 0);

#if USE_ACTOR_PTR_LOOKUP
	m_actorPtrToIndex.clear();
#endif
#if USE_ENTITY_PTR_LOOKUP
	m_entityPtrToIndex.clear();
#endif

	//iterate over all actors
	IActorSystem* pActorSystem = gEnv->pGameFramework->GetIActorSystem();
	const int kMaxNumActors = pActorSystem->GetActorCount();

	const int kTeamCount = g_pGame->GetGameRules()->GetTeamCount();

	const int kActorIndexMultiplier = (kTeamCount < 2) ? 1 : 0;

	if(kMaxNumActors > m_iMaxTrackedActors)
	{
		ReallocateMemoryForNActors(kMaxNumActors);
	}

	IActorIteratorPtr pActorIterator = pActorSystem->CreateActorIterator();
	IActor* pActor = static_cast<CActor*>(pActorIterator->Next());

	IActor* pLocalPlayer = g_pGame->GetIGameFramework()->GetClientActor();
	IAIObject* pPlayerAIObject = pLocalPlayer ? pLocalPlayer->GetEntity()->GetAI() : NULL;
	uint8 playerFactionID = pPlayerAIObject ? pPlayerAIObject->GetFactionID() : IFactionMap::InvalidFactionID;

	//TODO: Switch to local actor ptr array instead of iterating through actor system
	//			will require actor registration on creation as well as removal on destruction
	int iNumActorsTracked = 0;
	const bool bMultiplayer = gEnv->bMultiplayer;
	while (pActor != NULL)
	{
		if (pActor != pLocalPlayer)
		{
			IEntity* pEntity = pActor->GetEntity();
			const IAIObject* pAIObject = pEntity->GetAI();

			if(bMultiplayer || (pAIObject && pEntity->IsActivatedForUpdates()))
			{
				CacheDataFromActor(pActor, pEntity, pAIObject, kActorIndexMultiplier, playerFactionID, iNumActorsTracked);
				iNumActorsTracked++;
			}
		}

		pActor = pActorIterator->Next();
	}

	m_iNumActorsTracked								= iNumActorsTracked;

	if(pLocalPlayer)
	{
		CacheDataFromActor(pLocalPlayer, pLocalPlayer->GetEntity(), pPlayerAIObject, kActorIndexMultiplier, playerFactionID, iNumActorsTracked);
		m_iNumActorsTrackedIncLocalPlayer = iNumActorsTracked + 1;
	}
	else
	{
		m_iNumActorsTrackedIncLocalPlayer = iNumActorsTracked;
	}
}

void CActorManager::ActorRemoved(IActor * pActor)
{
#if 0 && USE_ACTOR_PTR_LOOKUP
	//Can do this cheaper with actor lookup
#else
	PrepareForIteration();

	const int kNumActors					= GetNumActors();
	const int kNumActorsIncPlayer = GetNumActorsIncludingLocalPlayer();
	const int kPlayerIndex = kNumActorsIncPlayer - 1;
	EntityId actorId = pActor->GetEntityId();
	bool bFound = false;
	
	for(int i = 0; i < kNumActors; i++)
	{
		if(m_actorEntityIds[i] == actorId)
		{
			WriteCachedActorData(kNumActors - 1, i);
			bFound = true;
			break;
		}
	}

	if(bFound)
	{
		//The actor was found in the normal list of actors
		if(kNumActorsIncPlayer != kNumActors)
		{
			//If there is a local player in the actor list, shift them up one
			WriteCachedActorData(kPlayerIndex, kNumActors-1);
			m_iNumActorsTrackedIncLocalPlayer -= 1;
			m_iNumActorsTracked								-= 1;
		}		
	}
	else
	{
		if(kNumActors && m_actorEntityIds[kPlayerIndex] == actorId)
		{
			m_iNumActorsTrackedIncLocalPlayer -= 1;
		}
	}

#endif
}

void CActorManager::ActorRevived(IActor * pActor)
{
	CRY_ASSERT_MESSAGE(gEnv->bMultiplayer || gEnv->IsEditor(), "ERROR! This should only be called in multiplayer or in the editor!");
	
	bool bFound = false;

	const EntityId actorId = pActor->GetEntityId();
	const int kNumActorsIncPlayer = m_iNumActorsTrackedIncLocalPlayer;

	int i = 0;
	for(; i < kNumActorsIncPlayer; i++)
	{
		if(m_actorEntityIds[i] == actorId)
		{
			bFound = true;
			break;
		}
	}

	//This may be the first spawn, so the actor may not be present in the list. As mentioned 
	// elsewhere, TODO: Use actor ptr list locally and add actors when revived
	if(bFound)
	{
		const int kTeamCount = g_pGame->GetGameRules()->GetTeamCount();
		const int kActorIndexMultiplier = (kTeamCount < 2) ? 1 : 0;

		// Don't need to worry about AI parameters - this is a multiplayer only function
		CacheDataFromActor(pActor, pActor->GetEntity(), NULL, kActorIndexMultiplier, IFactionMap::InvalidFactionID, i);		
	}
}

void CActorManager::WriteCachedActorData(int from, int to)
{
	m_actorEntityIds[to]			= m_actorEntityIds[from];
	m_actorHealth[to]					= m_actorHealth[from];
	m_actorPositions[to]			= m_actorPositions[from];
	m_actorSpectatorModes[to]	= m_actorSpectatorModes[from];
	m_actorTeamNums[to]				= m_actorTeamNums[from];
}

void CActorManager::CacheDataFromActor(const IActor* pActor, IEntity* pEntity, const  IAIObject* pAIObject, 
									   int kActorIndexMultiplier, uint8 playerFactionID, int iActorIndex)
{
	EntityId actorID = pActor->GetEntityId();
	const CActor *pCActor = static_cast<const CActor*>(pActor);

	m_actorEntityIds[iActorIndex] = actorID;
	m_actorPositions[iActorIndex] = pEntity->GetWorldPos();

	int iModifiedTeamNum = 0;
	if (gEnv->bMultiplayer)
	{
		int iTeamNum = g_pGame->GetGameRules()->GetTeam(actorID);

		//If there are no teams, we set each actor to have a team identical to their actor index in the actor 
		//	manager. This avoids having to do expensive checks to see if we should check the team numbers
		iModifiedTeamNum = (iTeamNum * (1 - kActorIndexMultiplier)) + ((iActorIndex + 1) * kActorIndexMultiplier);
	}
	else if(pAIObject)
	{
		uint8 factionID = pAIObject->GetFactionID();
		IFactionMap::ReactionType reactionToPlayer = gEnv->pAISystem->GetFactionMap().GetReaction(playerFactionID, factionID);
		iModifiedTeamNum = (reactionToPlayer < IFactionMap::Neutral) ? playerFactionID + (1 + factionID) : 0;
	}

	float fHealth = pCActor->GetHealth();
	CActor::EActorSpectatorMode spectatorMode = (CActor::EActorSpectatorMode)pCActor->GetSpectatorMode();

	m_actorTeamNums[iActorIndex]		= iModifiedTeamNum;
	m_actorHealth[iActorIndex]			= fHealth;		
	m_actorSpectatorModes[iActorIndex]	= spectatorMode;

	//Make sure we haven't truncated the numbers by assigning to int16s / int8s
	CRY_ASSERT_TRACE(m_actorTeamNums[iActorIndex] == iModifiedTeamNum,    ("%s '%s' team number broken! %d != %d",    pActor->GetEntity()->GetClass()->GetName(), pActor->GetEntity()->GetName(), m_actorTeamNums[iActorIndex], iModifiedTeamNum));
	CRY_ASSERT_TRACE(m_actorSpectatorModes[iActorIndex] == spectatorMode, ("%s '%s' spectator mode broken! %d != %d", pActor->GetEntity()->GetClass()->GetName(), pActor->GetEntity()->GetName(), m_actorSpectatorModes[iActorIndex], spectatorMode));

#if USE_ACTOR_PTR_LOOKUP
	m_actorPtrToIndex.insert(TActorPtrIndexMap::value_type(pActor, iActorIndex));
#endif
#if USE_ENTITY_PTR_LOOKUP
	m_entityPtrToIndex.insert(TEntityPtrIndexMap::value_type(pEntity, iActorIndex));
#endif
}

// return FLT_MAX if no actors not on team
//		DO NOT CHANGE away from using 'min()', if branching is necessary create a new
//		function to do so - Rich S
float CActorManager::GetLocalPlayerDistanceSqClosestHostileActor(const Vec3& posn, int teamId) const
{
	PrefetchLine(m_actorTeamNums, 0);
	PrefetchLine(m_actorHealth, 0);
	PrefetchLine(m_actorPositions, 0);
	PrefetchLine(m_actorPositions, 128);

	const int kNumActors = GetNumActors();
	float fClosestDistSq = FLT_MAX;

	if(!gEnv->bMultiplayer || (teamId != 0))
	{
		for(int i = 0; i < kNumActors; i++)
		{
			PrefetchLine(&m_actorPositions[i], 128);

			if(teamId != m_actorTeamNums[i] && m_actorHealth[i] > 0)
			{
				Vec3 diff = m_actorPositions[i] - posn;

				float fDistSq = diff.len2();

				fClosestDistSq = min(fDistSq, fClosestDistSq);
			}
		}		
	}
	else
	{
		for(int i = 0; i < kNumActors; i++)
		{
			PrefetchLine(&m_actorPositions[i], 128);

			if(m_actorHealth[i] > 0)
			{
				Vec3 diff = m_actorPositions[i] - posn;

				float fDistSq = diff.len2();

				fClosestDistSq = min(fDistSq, fClosestDistSq);
			}
		}
	}

	return fClosestDistSq;
}

float CActorManager::GetDistSqToClosestActor(const Vec3& position) const
{
	//Avoiding PrepareForIteration call as we don't need the TeamNums, EntityIds or Health
	PrefetchLine(m_actorPositions, 0);
	PrefetchLine(m_actorPositions, 128);
	PrefetchLine(m_actorHealth, 0);
	PrefetchLine(m_actorHealth, 128);

	const int		kNumActors			= m_iNumActorsTrackedIncLocalPlayer;
	float				fClosestDistSq	= FLT_MAX;

	for(int i = 0; i < kNumActors; i++)
	{
		PrefetchLine(&m_actorPositions[i], 128);

		if(m_actorHealth[i] > 0)
		{
			Vec3 diff = m_actorPositions[i] - position;

			float fDistSq = diff.len2();

			//On average, faster to iterate over all of them and get the closest without fp-branching
			//	and then branch once on the closest value
			fClosestDistSq = min(fDistSq, fClosestDistSq);
		}
	}

	return fClosestDistSq;
}

bool CActorManager::AnyActorWithinAABB( const AABB& bbox ) const
{
	//Avoiding PrepareForIteration call as we don't need the TeamNums or EntityIds
	PrefetchLine(m_actorPositions, 0);
	PrefetchLine(m_actorPositions, 128);
	PrefetchLine(m_actorHealth, 0);
	PrefetchLine(m_actorHealth, 128);

	const int		kNumActors			= m_iNumActorsTrackedIncLocalPlayer;
	for (int i = 0; i < kNumActors; i++)
	{
		PrefetchLine(&m_actorPositions[i], 128);

		if ((m_actorHealth[i] > 0) && bbox.IsContainPoint(m_actorPositions[i]))
		{
			return true;
		}
	}

	return false;
}

bool CActorManager::CanSeeAnyEnemyActor( EntityId actorId ) const
{
	//Not using PrepareForIteration as we're not using actor position or health
	PrefetchLine(m_actorTeamNums, 0);
	PrefetchLine(m_actorEntityIds, 0);

	const int kNumActors					= m_iNumActorsTracked;
	const int kLocalPlayerTeamId	= g_pGame->GetGameRules()->GetTeam(actorId);

	CPlayerVisTable * pPlayerVisTable = g_pGame->GetPlayerVisTable();

	for(int i = 0; i < kNumActors; i++)
	{
		if(m_actorTeamNums[i] != kLocalPlayerTeamId)
		{
			if(pPlayerVisTable->CanLocalPlayerSee(m_actorEntityIds[i], 20))
			{
				return true;
			}
		}
	}
	
	return false;
}

#if USE_ACTOR_PTR_LOOKUP
void CActorManager::GetActorDataFromActor( IActor * pActor, SActorData& actorData )
{
	int idx = m_iNumActorsTracked;
	
	TActorPtrIndexMap::const_iterator it = m_actorPtrToIndex.find(pActor);
	if (it != m_actorPtrToIndex.end())
	{
		 idx = it->second;
	}
	else
	{
		//We're requesting data on an actor that is not currently cached.
		//	Fill out the data, then return
		CacheDataFromActor(pActor, idx);
	}

	GetNthActorData(idx, actorData);
}
#endif

#if USE_ENTITY_PTR_LOOKUP
void CActorManager::GetActorDataFromEntity( IEntity *pEntity, SActorData& actorData )
{
	int idx = m_iNumActorsTracked;

	TEntityPtrIndexMap::const_iterator it = m_entityPtrToIndex.find(pEntity);
	if (it != m_entityPtrToIndex.end())
	{
		idx = it->second;
	}
	else
	{
		//We're requesting data on an actor that is not currently cached.
		//	Fill out the data, then return
		IActor * pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId());

		assert(pActor);

		CacheDataFromActor(pActor, idx);
	}

	GetNthActorData(idx, actorData);
}
#endif
