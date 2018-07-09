// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:
	Implements the Blend From Ragdoll AnimAction

-------------------------------------------------------------------------
History:
- 06.07.12: Created by Stephen M. North

*************************************************************************/
#pragma once

#ifndef __AnimActionBlendFromRagdoll_h__
#define __AnimActionBlendFromRagdoll_h__

#include <ICryMannequin.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAnimationPoseModifier.h>
#include <IGameRulesSystem.h>

#define USE_BLEND_FROM_RAGDOLL

class CActor;
class CAnimActionBlendFromRagdoll : public TAction< SAnimationContext >
{
	DEFINE_ACTION( "AnimActionBlendFromRagdoll" );

public:
	CAnimActionBlendFromRagdoll( int priority, CActor& actor, const FragmentID& fragID, const TagState fragTags = TAG_STATE_EMPTY );

protected:
	typedef std::vector<uint> TAnimationIds;
	typedef TAction< SAnimationContext > TBase;

	virtual void OnInitialise() override;
	virtual void Enter() override;
	virtual void Exit() override;
	virtual EStatus Update(float timePassed) override;

	virtual void OnFragmentStarted() override;

	void DispatchPoseModifier();
	void QueryPose();
	void GenerateAnimIDs();

private:
	CActor& m_actor;
	IAnimationPoseMatchingPtr m_pPoseMatching;
	TagState m_fragTagsTarget;
	TAnimationIds m_animIds;
	bool m_bSetAnimationFrag;
	uint m_animID;
};


class CAnimActionBlendFromRagdollSleep : public TAction< SAnimationContext >
{
	DEFINE_ACTION( "AnimActionBlendFromRagdollSleep" );

public:
	CAnimActionBlendFromRagdollSleep( int priority, CActor& actor, const HitInfo& hitInfo, const TagState& sleepTagState, const TagState& fragTags = TAG_STATE_EMPTY );

protected:
	typedef TAction< SAnimationContext > TBase;

	virtual void OnInitialise() override;
	virtual void Enter() override;

private:
	HitInfo m_hitInfo;
	TagState m_fragTagsTarget;

	CActor& m_actor;
};

#endif // __AnimActionBlendFromRagdoll_h__
