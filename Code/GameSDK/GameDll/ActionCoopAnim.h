// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:
	Implements Coop Anim.

-------------------------------------------------------------------------
History:
- 21:11:2011: Shamelessly stolen from TomBs StealthKill Action.

*************************************************************************/

#pragma once

#ifndef __ActionCoopAnim_h__
#define __ActionCoopAnim_h__

#include "ICryMannequin.h"
#include "PlayerAnimation.h"

class CPlayer;
class CActor;

class CActionCoopAnimation : public TPlayerAction, public IEntityEventListener
{
public:

	DEFINE_ACTION( "CoopAnim" )

	struct SActionCoopParams
	{
		SActionCoopParams( CPlayer &player, CActor &target, FragmentID fragID, TagState tagState, TagID targetTagID, const IAnimationDatabase* piOptionalDatabase )
			:
		m_player(player),
		m_target(target),
		m_fragID(fragID),
		m_tagState(tagState),
		m_targetTagID(targetTagID),
		m_piOptionalTargetDatabase(piOptionalDatabase)
		{
		}

		CPlayer& m_player;
		CActor& m_target;
		FragmentID m_fragID;
		TagState m_tagState;
		TagID m_targetTagID;
		const IAnimationDatabase* m_piOptionalTargetDatabase;
	};

	explicit CActionCoopAnimation( const SActionCoopParams& params );
	~CActionCoopAnimation();


protected:

	virtual void Install() override;
	virtual void Enter() override;
	virtual void Exit() override;

	// IEntityEventListener
	virtual void OnEntityEvent( IEntity *pEntity, const SEntityEvent& event ) override;
	// ~IEntityEventListener

	void RemoveTargetFromSlaveContext();
	void SendStateEventCoopAnim();

	CPlayer &m_player;
	CActor  &m_target;
	EntityId m_targetEntityID;
	TagID m_targetTagID;
	const IAnimationDatabase* m_piOptionalTargetDatabase;

private:
	CActionCoopAnimation(); // DO NOT IMPLEMENT!!!
};


#endif// __ActionCoopAnim_h__
