// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "AnimationGraph/AnimatedCharacter.h"
#include "AnimActionTriState.h"

// ----------------------------------------------------------------------------
const char* ANIMEVENT_ANIMEXIT = "animexit";
const EMovementControlMethod DEFAULT_MCM_HORIZONTAL = eMCM_Entity;
const EMovementControlMethod DEFAULT_MCM_VERTICAL = eMCM_Entity;

#define SendTriStateEvent(event)                        \
  {                                                     \
    On ## event();                                      \
                                                        \
    if (m_pListener)                                    \
    {                                                   \
      m_pListener->OnAnimActionTriState ## event(this); \
    }                                                   \
  }

// ----------------------------------------------------------------------------
CAnimActionTriState::CAnimActionTriState(int priority, FragmentID fragmentID, IAnimatedCharacter& animChar, bool oneShot, float maxMiddleDuration, bool skipIntro, IAnimActionTriStateListener* pListener)
	: TBase(priority, fragmentID, TAG_STATE_EMPTY, oneShot ? 0 : NoAutoBlendOut)
	, m_animChar(animChar)
	, m_pListener(pListener)
	, m_subState(eSS_None)
#ifndef _RELEASE
	, m_entered(false)
#endif
	, m_triStateFlags((oneShot ? eTSF_OneShot : 0) | (skipIntro ? eTSF_SkippedIntro : 0))
	, m_mcmHorizontal(DEFAULT_MCM_HORIZONTAL)
	, m_mcmVertical(DEFAULT_MCM_VERTICAL)
	, m_mcmHorizontalOld(eMCM_Undefined)
	, m_mcmVerticalOld(eMCM_Undefined)
	, m_maxMiddleDuration(maxMiddleDuration)
{
	CRY_ASSERT(&m_animChar);
}

// ----------------------------------------------------------------------------
void CAnimActionTriState::OnInitialise()
{
	if ((m_triStateFlags & eTSF_SkippedIntro) == 0)
	{
		static uint32 introCRC = CCrc32::ComputeLowercase("intro");
		const CTagDefinition* pFragTagDef = GetContext().controllerDef.GetFragmentTagDef(m_fragmentID);
		if (pFragTagDef)
		{
			const TagID fragTagID = pFragTagDef->Find(introCRC);
			if (fragTagID != TAG_ID_INVALID)
			{
				pFragTagDef->Set(m_fragTags, fragTagID, true);
			}
		}
	}
}

// ----------------------------------------------------------------------------
void CAnimActionTriState::Install()
{
	CRY_ASSERT(m_fragmentID != FRAGMENT_ID_INVALID);

	static uint32 introCRC = CCrc32::ComputeLowercase("intro");
	if ((m_triStateFlags & eTSF_SkippedIntro) || !TrySetTransitionFragment(introCRC))
	{
		m_triStateFlags |= eTSF_SkippedIntro;
		if (!TrySetTransitionFragment(0))
		{
			m_triStateFlags |= eTSF_SkipMiddle;
			// Note we could exit here in some way, but we still want all events to be sent so I'll just let it go through the regular Enter()->Exit() sequence
		}
	}

	TBase::Install();
}

// ----------------------------------------------------------------------------

void CAnimActionTriState::OnRequestBlendOut(EPriorityComparison priorityComp)
{
	/*	if (priorityComp == Equal)
	   {
	    CRY_ASSERT(m_eStatus == Installed);
	    CRY_ASSERT(m_subState != eSS_None);
	    CRY_ASSERT(m_subState != eSS_Exit);

	    Stop();
	   }*/
}

// ----------------------------------------------------------------------------
void CAnimActionTriState::OnAnimationEvent(ICharacterInstance* pCharacter, const AnimEventInstance& event)
{
	CRY_ASSERT(m_eStatus == Installed);

	// Are we 100% sure that the animevent we receive here is part of the sequence of the fragment we set last?

	static uint32 animExitCRC32 = CCrc32::ComputeLowercase(ANIMEVENT_ANIMEXIT);
	if (event.m_EventNameLowercaseCRC32 == animExitCRC32)
	{
		TransitionToNextSubState();
	}
}

// ----------------------------------------------------------------------------
IAction::EStatus CAnimActionTriState::Update(float timePassed)
{
	TBase::Update(timePassed);

	if (m_subState == eSS_Middle)
	{
		const bool hasMaxMiddleDuration = m_maxMiddleDuration >= 0.0f;
		if (hasMaxMiddleDuration)
		{
			if (gEnv->pTimer->GetFrameStartTime() >= m_middleEndTime)
			{
				Stop();
			}
		}
	}
	return m_eStatus;
}

// ----------------------------------------------------------------------------
void CAnimActionTriState::Enter()
{
	TBase::Enter();

	// --------------------------------------------------------------------------
	// Movement Control Method

	const bool runningOnLayerZero = (m_rootScope->GetBaseLayer() == 0);
	if (runningOnLayerZero)
	{
		m_mcmHorizontalOld = m_animChar.GetMCMH();
		CRY_ASSERT(m_mcmHorizontalOld != eMCM_Undefined);
		m_mcmVerticalOld = m_animChar.GetMCMV();
		CRY_ASSERT(m_mcmVerticalOld != eMCM_Undefined);

		m_animChar.SetMovementControlMethods(m_mcmHorizontal, m_mcmVertical);
	}

	// --------------------------------------------------------------------------
	// Collider Mode

	// TODO

	// --------------------------------------------------------------------------
	// Target Location

	if (m_triStateFlags & eTSF_TargetLocation)
	{
		SetParam("TargetPos", m_targetLocation);
	}

	// --------------------------------------------------------------------------
	// Send Events

	SendTriStateEvent(Enter);

	// --------------------------------------------------------------------------
	// Send Events

	CRY_ASSERT(m_subState == eSS_None);

	m_subState = eSS_Intro;
	SendTriStateEvent(Intro);

	if (m_triStateFlags & eTSF_SkippedIntro)
	{
		TransitionToNextSubState();
	}

#ifndef _RELEASE
	m_entered = true;
#endif
}

// ----------------------------------------------------------------------------
void CAnimActionTriState::Exit()
{
#ifndef _RELEASE
	if (!m_entered)
		CryFatalError("CAnimActionTriState::Exit: Exit without Enter call");
#endif

	TBase::Exit();

	// --------------------------------------------------------------------------
	// Movement Control Method

	const bool runningOnLayerZero = (m_rootScope->GetBaseLayer() == 0);
	if (runningOnLayerZero)
	{
		CRY_ASSERT(m_mcmHorizontalOld != eMCM_Undefined);
		CRY_ASSERT(m_mcmVerticalOld != eMCM_Undefined);
		m_animChar.SetMovementControlMethods(m_mcmHorizontalOld, m_mcmVerticalOld);
	}

	// --------------------------------------------------------------------------
	// Collider Mode

	// TODO

	m_subState = eSS_Exit;
	SendTriStateEvent(Exit);
}

// ----------------------------------------------------------------------------
void CAnimActionTriState::UnregisterListener()
{
	CRY_ASSERT(m_pListener);
	m_pListener = NULL;
}

// ----------------------------------------------------------------------------
void CAnimActionTriState::InitTargetLocation(const QuatT& targetLocation)
{
#ifndef _RELEASE
	CRY_ASSERT(!m_entered);
#endif

	m_targetLocation = targetLocation;
	m_triStateFlags |= eTSF_TargetLocation;
}

// ----------------------------------------------------------------------------
void CAnimActionTriState::ClearTargetLocation()
{
#ifndef _RELEASE
	CRY_ASSERT(!m_entered);
#endif

	m_triStateFlags &= ~eTSF_TargetLocation;
}

// ----------------------------------------------------------------------------
void CAnimActionTriState::InitMovementControlMethods(EMovementControlMethod mcmHorizontal, EMovementControlMethod mcmVertical)
{
#ifndef _RELEASE
	CRY_ASSERT(!m_entered);
#endif

	m_mcmHorizontal = (mcmHorizontal == eMCM_Undefined) ? DEFAULT_MCM_HORIZONTAL : mcmHorizontal;
	m_mcmVertical = (mcmVertical == eMCM_Undefined) ? DEFAULT_MCM_VERTICAL : mcmVertical;
}

// ----------------------------------------------------------------------------
void CAnimActionTriState::OnSequenceFinished(int layer, uint32 scopeID)
{
#ifndef _RELEASE
	CRY_ASSERT(m_entered);
#endif

	if (layer != 0 || GetRootScope().GetID() != scopeID)
	{
		return;
	}

	TransitionToNextSubState();
}

// ----------------------------------------------------------------------------
bool CAnimActionTriState::TrySetTransitionFragment(uint32 fragTagCRC)
{
	const IScope& rootScope = GetRootScope();
	const IAnimationDatabase& database = rootScope.GetDatabase();
	const TagState globalTagState = GetContext().state.GetMask();

	TagState fragTags = TAG_STATE_EMPTY;
	if (fragTagCRC)
	{
		const CTagDefinition* pFragTagDef = GetContext().controllerDef.GetFragmentTagDef(m_fragmentID);
		if (!pFragTagDef)
		{
			return false;
		}

		const TagID fragTagID = pFragTagDef->Find(fragTagCRC);
		if (fragTagID == TAG_ID_INVALID)
		{
			return false;
		}

		pFragTagDef->Set(fragTags, fragTagID, true);
	}

	const TagState scopeAdditionalTags = rootScope.GetAdditionalTags();
	const TagState combinedGlobalTagState = GetContext().controllerDef.m_tags.GetUnion(globalTagState, scopeAdditionalTags);
	const SFragTagState fragTagState(combinedGlobalTagState, fragTags);

	SFragTagState matchingTagState;
	const uint32 optionCount = database.FindBestMatchingTag(SFragmentQuery(m_fragmentID, fragTagState), &matchingTagState);
	if (optionCount == 0 || matchingTagState.fragmentTags != fragTagState.fragmentTags)
	{
		return false;
	}

	if (m_fragTags != fragTags)
	{
		SetFragment(m_fragmentID, fragTags);
	}

	return true;
}

// ----------------------------------------------------------------------------
void CAnimActionTriState::Stop()
{
	if ((GetStatus() == IAction::None) || (GetStatus() == IAction::Pending))
	{
		assert(m_subState == eSS_None);
		TBase::Stop();
	}
	else
	{
		if (m_subState < eSS_Middle)
		{
			m_triStateFlags |= eTSF_SkipMiddle;
		}
		else if (m_subState == eSS_Middle)
		{
			if (!IsOneShot())
				TransitionToNextSubState();
		}
	}
}

// ----------------------------------------------------------------------------
void CAnimActionTriState::TransitionToNextSubState()
{
	switch (m_subState)
	{
	case eSS_Intro:
		{
			if (!(m_triStateFlags & eTSF_SkipMiddle))
			{
				if (!TrySetTransitionFragment(0))
				{
					m_triStateFlags |= eTSF_SkipMiddle;
				}
			}

			m_subState = eSS_Middle;
			m_middleEndTime = gEnv->pTimer->GetFrameStartTime() + CTimeValue(m_maxMiddleDuration);
			SendTriStateEvent(Middle);

			if (!(m_triStateFlags & eTSF_SkipMiddle))
			{
				break;
			}

			// fall-through to Middle sequence finished
		}
	case eSS_Middle:
		{
			static uint32 outroCRC = CCrc32::ComputeLowercase("outro");
			const bool hasOutro = TrySetTransitionFragment(outroCRC);
			m_flags &= ~IAction::NoAutoBlendOut;

			m_subState = eSS_Outro;
			SendTriStateEvent(Outro);

			if (hasOutro)
			{
				break;
			}

			// fall-through to Outro sequence finished
		}
	case eSS_Outro:
		{
			SendTriStateEvent(OutroEnd);
			TBase::Stop();
			break;
		}
	default:
		{
			CRY_ASSERT(false);
		}
	}
}
