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
#ifndef __CORPSEMANAGER_H__
#define __CORPSEMANAGER_H__

#if _MSC_VER > 1000
# pragma once
#endif

#define MAX_CORPSES (24)

#include <CryCore/Containers/CryFixedArray.h>
#include "GameRulesModules/IGameRulesRoundsListener.h"

class CCorpseManager : IGameRulesRoundsListener
{
	enum ECorpseFlags
	{
		eCF_NeverSleep		=BIT(0),
	};

	struct SCorpseInfo
	{
		SCorpseInfo(EntityId _id, Vec3 _pos, float _thermalVisionHeat)
			:	corpseId(_id)
			, corpsePos(_pos)
			, age(0.0f)
			, awakeTime(0.f)
			, thermalVisionHeat(_thermalVisionHeat)
			, flags(0)
		{}
		
		Vec3 corpsePos;
		float age;
		float awakeTime;
		float thermalVisionHeat;
		EntityId corpseId;
		uint8 flags;
	};

public:
	CCorpseManager(); 
	~CCorpseManager(); 

	void RegisterCorpse(EntityId corpseId, Vec3 corpsePos, float thermalVisionHeat);
	void RemoveACorpse();
	void Update(float frameTime);
	void KeepAwake(const EntityId corpseId, IPhysicalEntity* pCorpsePhys);
	void ClearCorpses();

	// IGameRulesRoundsListener
	virtual void OnRoundStart() { ClearCorpses(); }
	virtual void OnRoundEnd() {}
	virtual void OnSuddenDeath() {}
	virtual void ClRoundsNetSerializeReadState(int newState, int curState) {}
	virtual void OnRoundAboutToStart() {};
	//~IGameRulesRoundsListener

private:

	void UpdateCorpses(float frameTime);
	void OnRemovedCorpse(const EntityId corpseId);

	CryFixedArray<SCorpseInfo, MAX_CORPSES>  m_activeCorpses;

	bool m_bThermalVisionOn;
};


#endif // __CORPSEMANAGER_H__
