// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Actor manager implementation

-------------------------------------------------------------------------
History:
- 2010/05/31 : Created by Richard Semmens

*************************************************************************/
#ifndef __ACTOR_MANAGER_H__
#define __ACTOR_MANAGER_H__

#include "Actor.h"

typedef		int8												TTeamNum;
typedef		Vec3												TActorPos;
typedef		float												TActorHealth;
typedef		CActor::EActorSpectatorMode	TActorSpectatorMode;

struct SActorData
{
	EntityId							entityId;
	TTeamNum							teamId;
	TActorHealth					health;
	TActorPos							position;
	TActorSpectatorMode		spectatorMode;
};

#define USE_ACTOR_PTR_LOOKUP 0
#define USE_ENTITY_PTR_LOOKUP 0

struct IActor;

class CActorManager
{
public:
	CActorManager();
	virtual ~CActorManager();

	void Update(float dt);
	void Reset(bool bReallocate = true);

	void ActorRemoved(IActor * pActor);
	void ActorRevived(IActor * pActor);

	float GetLocalPlayerDistanceSqClosestHostileActor(const Vec3& posn, int teamId) const;
	bool	CanSeeAnyEnemyActor(EntityId actorId) const;
	float GetDistSqToClosestActor(const Vec3& position) const;
	bool	AnyActorWithinAABB(const AABB& bbox) const;

	static CActorManager * GetActorManager();

	ILINE void PrepareForIteration() const
	{
		PrefetchLine(m_actorTeamNums, 0);
		PrefetchLine(m_actorHealth, 0);
		PrefetchLine(m_actorEntityIds, 0);
		PrefetchLine(m_actorPositions, 0);
	}

	ILINE void	GetNthActorData(int iActorIndex, SActorData& actorData) const
	{
		PrefetchLine(&m_actorPositions[iActorIndex], 128);

		actorData.entityId				= m_actorEntityIds[iActorIndex];
		actorData.health					= m_actorHealth[iActorIndex];
		actorData.teamId					= m_actorTeamNums[iActorIndex];
		actorData.position				= m_actorPositions[iActorIndex];
		actorData.spectatorMode	= (CActor::EActorSpectatorMode)m_actorSpectatorModes[iActorIndex];
	}

#if USE_ACTOR_PTR_LOOKUP
	void GetActorDataFromActor(IActor * pActor, SActorData& actorData);
#endif
#if USE_ENTITY_PTR_LOOKUP
	void GetActorDataFromEntity(IEntity *pEntity, SActorData& actorData);
#endif

	//Does not include the local player
	ILINE int GetNumActors()											const	{		return m_iNumActorsTracked;	}	

	//Does include the local player
	ILINE int GetNumActorsIncludingLocalPlayer()	const	{		return m_iNumActorsTrackedIncLocalPlayer;	}	


private:
	void CacheDataFromActor(const IActor* pActor, IEntity* pEntity, const IAIObject* pAIObject, 
		int kActorIndexMultiplier, uint8 playerFactionID, int iActorIndex);
	void WriteCachedActorData(int from, int to);

	size_t	GetMemoryRequiredForNActors(int iNumActors);
	void		ReallocateMemoryForNActors(int iNumActors);



	typedef std::map<IEntity*, int> TEntityPtrIndexMap;
	typedef std::map<IActor*, int>	TActorPtrIndexMap;

	static const int kMaxActorsTrackedInit = 1;

#if USE_ACTOR_PTR_LOOKUP
	TActorPtrIndexMap		m_actorPtrToIndex;
#endif
#if USE_ENTITY_PTR_LOOKUP
	TEntityPtrIndexMap	m_entityPtrToIndex;
#endif	

	char			* m_startMemoryPtr;
	void			* m_startMemoryPtrAlign;
	
	EntityId						* m_actorEntityIds;
	TActorPos						* m_actorPositions;
	TTeamNum						* m_actorTeamNums;
	TActorSpectatorMode	* m_actorSpectatorModes;
	TActorHealth				* m_actorHealth;

	int				m_iNumActorsTracked;
	int				m_iNumActorsTrackedIncLocalPlayer;
	int				m_iMaxTrackedActors;
};

#endif //__ACTOR_MANAGER_H__
