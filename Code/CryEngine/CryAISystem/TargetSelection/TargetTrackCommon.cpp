// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Defines common helpers for the Target Track system

   -------------------------------------------------------------------------
   History:
   - 02:01:2010: Created by Kevin Kirst

*************************************************************************/

#include "StdAfx.h"
#include "TargetTrackCommon.h"

namespace TargetTrackHelpers
{
const float STargetTrackStimulusConfig::INVALID_VALUE = 0.0f;

//////////////////////////////////////////////////////////////////////////
SEnvelopeData::SEnvelopeData()
	: m_fCurrentValue(0.0f)
	, m_fStartTime(0.0f)
	, m_fLastInvokeTime(0.0f)
	, m_fLastRunningValue(0.0f)
	, m_fLastReleasingValue(0.0f)
	, m_bReinvoked(false)
{

}

//////////////////////////////////////////////////////////////////////////
STargetTrackStimulusEvent::STargetTrackStimulusEvent(tAIObjectID ownerId)
	: m_vTargetPos(ZERO)
	, m_ownerId(ownerId)
	, m_targetId(0)
	, m_eTargetThreat(AITHREAT_AGGRESSIVE)
	, m_eStimulusType(eEST_Generic)
{
	assert(m_ownerId > 0);
}

//////////////////////////////////////////////////////////////////////////
STargetTrackStimulusEvent::STargetTrackStimulusEvent(tAIObjectID ownerId, tAIObjectID targetId, const char* szStimulusName, const SStimulusEvent& eventInfo)
	: m_sStimulusName(szStimulusName)
	, m_vTargetPos(eventInfo.vPos)
	, m_ownerId(ownerId)
	, m_targetId(targetId)
	, m_eTargetThreat(eventInfo.eTargetThreat)
	, m_eStimulusType(eventInfo.eStimulusType)
{
	assert(m_ownerId > 0);
	assert(szStimulusName && szStimulusName[0]);
}

//////////////////////////////////////////////////////////////////////////
STargetTrackPulseConfig::STargetTrackPulseConfig()
	: m_fValue(0.0f)
	, m_fDuration(0.0f)
	, m_bInherited(false)
{

}

//////////////////////////////////////////////////////////////////////////
STargetTrackPulseConfig::STargetTrackPulseConfig(const char* szPulse, float fValue, float fDuration)
	: m_sPulse(szPulse)
	, m_fValue(fValue)
	, m_fDuration(fDuration)
	, m_bInherited(false)
{
	assert(szPulse && szPulse[0]);
}

//////////////////////////////////////////////////////////////////////////
STargetTrackPulseConfig::STargetTrackPulseConfig(const STargetTrackPulseConfig& other, bool bInherited /*= false*/)
	: m_sPulse(other.m_sPulse)
	, m_fValue(other.m_fValue)
	, m_fDuration(other.m_fDuration)
	, m_bInherited(false)
{
	m_bInherited = (bInherited || other.m_bInherited);
}

//////////////////////////////////////////////////////////////////////////
STargetTrackModifierConfig::STargetTrackModifierConfig()
	: m_uId(0)
	, m_fValue(1.0f)
	, m_fLimit(0.0f)
	, m_bInherited(false)
{

}

//////////////////////////////////////////////////////////////////////////
STargetTrackModifierConfig::STargetTrackModifierConfig(uint32 uId, float fValue, float fLimit)
	: m_uId(uId)
	, m_fValue(fValue)
	, m_fLimit(0.0f)
	, m_bInherited(false)
{
	assert(m_uId > 0);

	m_fLimit = max(fLimit, 0.0f);
}

//////////////////////////////////////////////////////////////////////////
STargetTrackModifierConfig::STargetTrackModifierConfig(const STargetTrackModifierConfig& other, bool bInherited /*= false*/)
	: m_uId(other.m_uId)
	, m_fValue(other.m_fValue)
	, m_fLimit(0.0f)
	, m_bInherited(false)
{
	m_fLimit = max(other.m_fLimit, 0.0f);
	m_bInherited = (bInherited || other.m_bInherited);
}

//////////////////////////////////////////////////////////////////////////
STargetTrackStimulusConfig::STargetTrackStimulusConfig()
	: m_fPeak(0.0f)
	, m_fAttack(INVALID_VALUE)
	, m_fDecay(INVALID_VALUE)
	, m_fSustainRatio(INVALID_VALUE)
	, m_fRelease(INVALID_VALUE)
	, m_fIgnore(INVALID_VALUE)
	, m_bHostileOnly(false)
	, m_ucInheritanceMask(0)
{

}

//////////////////////////////////////////////////////////////////////////
STargetTrackStimulusConfig::STargetTrackStimulusConfig(const char* szStimulus, bool bHostileOnly, float fPeak, float fSustainRatio, float fAttack, float fDecay, float fRelease, float fIgnore)
	: m_sStimulus(szStimulus)
	, m_bHostileOnly(bHostileOnly)
	, m_ucInheritanceMask(0)
{
	assert(szStimulus && szStimulus[0]);

	m_fPeak = (fPeak > FLT_EPSILON ? fPeak : INVALID_VALUE);
	m_fAttack = (fAttack > FLT_EPSILON ? fAttack : INVALID_VALUE);
	m_fDecay = (fDecay > FLT_EPSILON ? fDecay : INVALID_VALUE);
	m_fSustainRatio = (fSustainRatio > FLT_EPSILON ? fSustainRatio : INVALID_VALUE);
	m_fRelease = (fRelease > FLT_EPSILON ? fRelease : INVALID_VALUE);
	m_fIgnore = (fIgnore > FLT_EPSILON ? fIgnore : INVALID_VALUE);
}

//////////////////////////////////////////////////////////////////////////
STargetTrackStimulusConfig::STargetTrackStimulusConfig(const STargetTrackStimulusConfig& other, bool bInherited /*= false*/)
	: m_sStimulus(other.m_sStimulus)
	, m_fPeak(other.m_fPeak)
	, m_fAttack(other.m_fAttack)
	, m_fDecay(other.m_fDecay)
	, m_fSustainRatio(other.m_fSustainRatio)
	, m_fRelease(other.m_fRelease)
	, m_fIgnore(other.m_fIgnore)
	, m_bHostileOnly(other.m_bHostileOnly)
	, m_ucInheritanceMask(other.m_ucInheritanceMask)
{
	m_pulses.clear();
	m_pulses.reserve(other.m_pulses.size());
	m_modifiers.clear();
	m_modifiers.reserve(other.m_modifiers.size());
	m_threatLevels.clear();
	m_threatLevels.reserve(other.m_threatLevels.size());

	TPulseContainer::const_iterator itOtherPulse = other.m_pulses.begin();
	TPulseContainer::const_iterator itOtherPulseEnd = other.m_pulses.end();
	for (; itOtherPulse != itOtherPulseEnd; ++itOtherPulse)
	{
		const uint32 uOtherPulseHash = itOtherPulse->first;
		const STargetTrackPulseConfig& otherPulse = itOtherPulse->second;

		TPulseContainer::value_type pulsePair(uOtherPulseHash, TargetTrackHelpers::STargetTrackPulseConfig(otherPulse, bInherited));
		m_pulses.insert(pulsePair);
	}

	TModifierContainer::const_iterator itOtherMod = other.m_modifiers.begin();
	TModifierContainer::const_iterator itOtherModEnd = other.m_modifiers.end();
	for (; itOtherMod != itOtherModEnd; ++itOtherMod)
	{
		const STargetTrackModifierConfig& otherMod = itOtherMod->second;

		TModifierContainer::value_type modPair(otherMod.m_uId, TargetTrackHelpers::STargetTrackModifierConfig(otherMod, bInherited));
		m_modifiers.insert(modPair);
	}

	TThreatLevelContainer::const_iterator itOtherThreatLevel = other.m_threatLevels.begin();
	TThreatLevelContainer::const_iterator itOtherThreatLevelEnd = other.m_threatLevels.end();
	for (; itOtherThreatLevel != itOtherThreatLevelEnd; ++itOtherThreatLevel)
	{
		TThreatLevelContainer::value_type threatPair(itOtherThreatLevel->first, itOtherThreatLevel->second);
		m_threatLevels.insert(threatPair);
	}
}

//////////////////////////////////////////////////////////////////////////
STargetTrackConfig::STargetTrackConfig()
	: m_bTemplateApplied(false)
{

}

//////////////////////////////////////////////////////////////////////////
STargetTrackConfig::STargetTrackConfig(const char* szName)
	: m_sName(szName)
	, m_bTemplateApplied(false)
{
	assert(szName && szName[0]);
}
}
