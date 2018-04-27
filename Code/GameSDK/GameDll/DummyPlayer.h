// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id: DummyPlayer.h$
$DateTime$
Description: A dummy player used to simulate a client player for profiling purposes

-------------------------------------------------------------------------
History:
- 01/07/2010 11:15:00: Created by Martin Sherburn

*************************************************************************/

#ifndef __DUMMYPLAYER_H__
#define __DUMMYPLAYER_H__

#if (USE_DEDICATED_INPUT)

#include "Player.h"
#include "AIDemoInput.h"

class CDummyPlayer : public CPlayer
{
public:
	CDummyPlayer();
	virtual ~CDummyPlayer();

	virtual bool IsPlayer() const { return true; }
	
	virtual bool Init( IGameObject * pGameObject );
	virtual void Update(SEntityUpdateContext& ctx, int updateSlot);

	EDefaultableBool GetFire();
	void SetFire(EDefaultableBool value);
	EDefaultableBool GetMove();
	void SetMove(EDefaultableBool value);

	void GetMemoryUsage(ICrySizer *pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		CPlayer::GetInternalMemoryUsage(pSizer); // collect memory of parent class
	}

protected:
	virtual void OnChangeTeam();

private:
};

#endif //USE_DEDICATED_INPUT

#endif //!__DUMMYPLAYER_H__
