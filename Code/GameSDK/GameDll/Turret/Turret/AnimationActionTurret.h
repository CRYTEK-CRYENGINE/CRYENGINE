// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ANIMATION_ACTION_TURRET__H__
#define __ANIMATION_ACTION_TURRET__H__

#include <ICryMannequin.h>

class CAnimationActionTurret
	: public TAction< SAnimationContext >
{
public:
	typedef TAction< SAnimationContext > BaseClass;

	DEFINE_ACTION( "AnimationActionTurret" );

	CAnimationActionTurret( FragmentID fragmentId )
	 : TAction< SAnimationContext >( 0, fragmentId, TAG_STATE_EMPTY, IAction::NoAutoBlendOut )
	{
	}

	~CAnimationActionTurret()
	{
	}

	virtual EPriorityComparison ComparePriority( const IAction& currentAction ) const override
	{
		assert( &currentAction != this );
		return (IAction::Installed == currentAction.GetStatus() && IAction::Installing & ~currentAction.GetFlags()) ? Higher : BaseClass::ComparePriority(currentAction);
	}

	virtual EStatus Update( float elapsedSeconds ) override
	{
		const IScope& rootScope = GetRootScope();
		const bool foundBetterMatchingFragment = rootScope.IsDifferent( m_fragmentID, m_fragTags );
		if ( foundBetterMatchingFragment )
		{
			SetFragment( m_fragmentID, m_fragTags );
		}

		return BaseClass::Update( elapsedSeconds );
	}
};

#endif