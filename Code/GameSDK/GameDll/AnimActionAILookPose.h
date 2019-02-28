// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#ifndef __ANIM_ACTION_AI_LOOK_POSE_H__
#define __ANIM_ACTION_AI_LOOK_POSE_H__

#include "ICryMannequin.h"


class CAnimActionAILookPose
	: public TAction< SAnimationContext >
{
public:
	typedef TAction< SAnimationContext > TBase;

	DEFINE_ACTION( "AnimActionAILookPose" );

	CAnimActionAILookPose();

	// IAction
	virtual void OnInitialise() override;
	virtual EStatus Update( float timePassed ) override;
	virtual void Install() override;
	// ~IAction

	static bool IsSupported( const SAnimationContext& context );

private:
	void InitialiseLookPoseBlender();
};


#endif