// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//-------------------------------------------------------------------------
//
// Description:
//  AnimAction that can handle a typical three-fragment Intro/Middle (sometimes looping)/Outro sequence.
//
////////////////////////////////////////////////////////////////////////////
#ifndef __ANIM_ACTION_TRI_STATE_H__
#define __ANIM_ACTION_TRI_STATE_H__
#pragma once

//---------------------------------------------------------------------------------------------------

struct IAnimActionTriStateListener
{
	virtual ~IAnimActionTriStateListener() {}

	// Called when the action is entered (called before Intro)
	virtual void OnAnimActionTriStateEnter(class CAnimActionTriState* pAction) {}

	// Called when the Intro substate starts (also when there is no animation for it)
	virtual void OnAnimActionTriStateIntro(class CAnimActionTriState* pAction) {}

	// Called when the Middle substate starts (also when there is no animation for
	// it or when a call to Abort() will cause the middle part to be skipped)
	// Will NOT get called when the action gets trumped by a higher prio action
	// during the Intro substate, or when it gets a call to ForceFinsh()
	virtual void OnAnimActionTriStateMiddle(class CAnimActionTriState* pAction) {}

	// Called when the Outro substate starts (also when there is no animation for it)
	// Will NOT get called when the action gets trumped by a higher prio action
	// during the Middle substate, or when it gets a call to ForceFinsh()
	virtual void OnAnimActionTriStateOutro(class CAnimActionTriState* pAction) {}

	// Called when the Outro substate ends (also when there is no animation for it)
	// Will NOT get called when the action gets trumped by a higher prio action
	// during the Outro substate, or when it gets a call to ForceFinsh()
	virtual void OnAnimActionTriStateOutroEnd(class CAnimActionTriState* pAction) {}

	// Called when the action is exited (guaranteed to be called when Enter was
	// called before -- except in error cases)
	virtual void OnAnimActionTriStateExit(class CAnimActionTriState* pAction) {}
};

//---------------------------------------------------------------------------------------------------

class CAnimActionTriState : public TAction<SAnimationContext>
{
public:
	typedef TAction<SAnimationContext> TBase;

	DEFINE_ACTION("TriState");

	enum ESubState
	{
		eSS_None,
		eSS_Intro,
		eSS_Middle,
		eSS_Outro,
		eSS_Exit,
	};

	// --------------------------------------------------------------------------

	CAnimActionTriState(int priority, FragmentID fragmentID, IAnimatedCharacter& animChar, bool oneShot, float maxMiddleDuration = -1, bool skipIntro = false, IAnimActionTriStateListener* pListener = NULL);

	// --------------------------------------------------------------------------

	// *Must* be called when the action's lifetime can be bigger than the listener's
	void UnregisterListener();

	// Initialize the location that is passed as parameter "TargetPos" to CryMannequin
	void InitTargetLocation(const QuatT& targetLocation);

	// Clear the location that is passed as parameter "TargetPos" to CryMannequin
	void ClearTargetLocation();

	// Initialize the MovementControlMethods that will be set when this action enters
	// Note: This only happens when the fragment runs on layer 0, otherwise this
	// setting is ignored
	void InitMovementControlMethods(EMovementControlMethod mcmHorizontal, EMovementControlMethod mcmVertical);

	// Stop the action gracefully. Intro & Outro will still play, but the middle
	// part will be skipped or stopped. When the Middle part is already playing,
	// it will still play fully when the action is marked as OneShot.
	// Note: When in an Intro, the Middle event will still be sent and immediately
	// followed by the Outro event.
	// Contrast this behavior to calling ForceFinish() or when this action is
	// pushed away by another, higher priority action. In that case only the Exit
	// event will be sent.
	void             Stop();

	inline ESubState GetSubState() const { return m_subState; }

	inline bool      IsOneShot() const   { return (m_triStateFlags & eTSF_OneShot) != 0; }

protected:
	// --------------------------------------------------------------------------

	// Called when the action is entered (called before Intro)
	virtual void OnEnter() {}

	// Called when the Intro substate starts (also when there is no animation for it)
	virtual void OnIntro() {}

	// Called when the Middle substate starts (also when there is no animation for
	// it or when a call to Abort() will cause the middle part to be skipped)
	// Will NOT get called when the action gets trumped by a higher prio action
	// during the Intro substate, or when it gets a call to ForceFinsh()
	virtual void OnMiddle() {}

	// Called when the Outro substate starts (also when there is no animation for it)
	// Will NOT get called when the action gets trumped by a higher prio action
	// during the Middle substate, or when it gets a call to ForceFinsh()
	virtual void OnOutro() {}

	// Called when the Outro substate ends (also when there is no animation for it)
	// Will NOT get called when the action gets trumped by a higher prio action
	// during the Outro substate, or when it gets a call to ForceFinsh()
	virtual void OnOutroEnd() {}

	// Called when the action is exited (guaranteed to be called when Enter was
	// called before -- except in error cases)
	virtual void OnExit() {}

	// --------------------------------------------------------------------------

	// overrides ----------------------------------------------------------------
	virtual void    OnAnimationEvent(ICharacterInstance* pCharacter, const AnimEventInstance& event) override;
	virtual void    OnSequenceFinished(int layer, uint32 scopeID) override;
	virtual EStatus Update(float timePassed) override;
	virtual void    Enter() override;
	virtual void    Exit() override;
	virtual void    Install() override;
	virtual void    OnRequestBlendOut(EPriorityComparison priorityComp) override;
	virtual void    OnInitialise() override;
	// ~overrides ----------------------------------------------------------------

protected:

	IAnimatedCharacter&          m_animChar;
	IAnimActionTriStateListener* m_pListener;
	ESubState                    m_subState;
#ifndef _RELEASE
	bool                         m_entered;
#endif

private:
	bool TrySetTransitionFragment(uint32 fragTagCRC);
	void TransitionToNextSubState();

	enum ETriStateFlag
	{
		eTSF_TargetLocation = BIT(1), // has target location
		eTSF_SkippedIntro   = BIT(2),
		eTSF_SkipMiddle     = BIT(3),
		eTSF_OneShot        = BIT(4),
	};

	uint32                 m_triStateFlags;
	EMovementControlMethod m_mcmHorizontal;
	EMovementControlMethod m_mcmVertical;
	EMovementControlMethod m_mcmHorizontalOld;
	EMovementControlMethod m_mcmVerticalOld;
	QuatT                  m_targetLocation;
	float                  m_maxMiddleDuration;
	CTimeValue             m_middleEndTime;
};

#endif // __ANIM_ACTION_TRI_STATE_H__
