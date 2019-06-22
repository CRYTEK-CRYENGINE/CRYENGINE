// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DialogScript.h"
#include "DialogCommon.h"

namespace
{
static const CDialogScript::TActorID MASK_ALL = ~CDialogScript::TActorID(0);
static const CDialogScript::TActorID MASK_01010101 = MASK_ALL / 3;
static const CDialogScript::TActorID MASK_00110011 = MASK_ALL / 5;
static const CDialogScript::TActorID MASK_00001111 = MASK_ALL / 17;

ILINE void bit_set(CDialogScript::TActorID& n, int which)
{
	n |= (0x01 << which);
}

ILINE void bit_clear(CDialogScript::TActorID& n, int which)
{
	n &= ~(0x01 << which);
}

ILINE bool is_bit_set(CDialogScript::TActorID& n, int which)
{
	return (n & (0x01 << which)) != 0;
}

ILINE int bit_count(CDialogScript::TActorID n)
{
	n = (n & MASK_01010101) + ((n >> 1) & MASK_01010101);
	n = (n & MASK_00110011) + ((n >> 2) & MASK_00110011);
	n = (n & MASK_00001111) + ((n >> 4) & MASK_00001111);
	return n % 255;
}
}

void CDialogScript::SActorSet::SetActor(TActorID id)
{
	bit_set(m_actorBits, id);
}

void CDialogScript::SActorSet::ResetActor(TActorID id)
{
	bit_clear(m_actorBits, id);
}

bool CDialogScript::SActorSet::HasActor(TActorID id)
{
	return is_bit_set(m_actorBits, id);
}

int CDialogScript::SActorSet::NumActors() const
{
	return bit_count(m_actorBits);
}

bool CDialogScript::SActorSet::Matches(const CDialogScript::SActorSet& other) const
{
	return m_actorBits == other.m_actorBits;
}

bool CDialogScript::SActorSet::Satisfies(const CDialogScript::SActorSet& other) const
{
	return (m_actorBits & other.m_actorBits) == other.m_actorBits;
}

////////////////////////////////////////////////////////////////////////////
CDialogScript::CDialogScript(const string& dialogID) : m_id(dialogID), m_reqActorSet(0), m_bComplete(false), m_versionFlags(0)
{
}

////////////////////////////////////////////////////////////////////////////
CDialogScript::~CDialogScript()
{
}

////////////////////////////////////////////////////////////////////////////
// Add one line after another
bool CDialogScript::AddLine(TActorID actorID, CryAudio::ControlId audioID, const char* anim, const char* facial, TActorID lookAtTargetID, float delay, float facialWeight, float facialFadeTime, bool bLookAtSticky, bool bResetFacial, bool bResetLookAt, bool bSoundStopsAnim, bool bUseAGSignal, bool bUseAGEP)
{
	SScriptLine line;
	line.m_actor = actorID;
	line.m_lookatActor = lookAtTargetID;
	line.m_audioID = audioID;
	line.m_anim = anim;
	line.m_facial = facial;
	line.m_delay = delay;
	line.m_facialWeight = facialWeight;
	line.m_facialFadeTime = facialFadeTime;
	line.m_flagLookAtSticky = bLookAtSticky;
	line.m_flagResetFacial = bResetFacial;
	line.m_flagResetLookAt = bResetLookAt;
	line.m_flagSoundStopsAnim = bSoundStopsAnim;
	line.m_flagAGSignal = bUseAGSignal;
	line.m_flagAGEP = bUseAGEP;
	line.m_flagUnused = 0;
	return AddLine(line);
}

////////////////////////////////////////////////////////////////////////////
// Add one line after another
bool CDialogScript::AddLine(const SScriptLine& line)
{
	if (line.m_actor >= MAX_ACTORS)
	{
		GameWarning("CDialogScript::AddLine: Script='%s' Cannot add line because actorID %d is out of range [0..%d]", m_id.c_str(), line.m_actor, MAX_ACTORS - 1);
		return false;
	}

	if (line.m_lookatActor != NO_ACTOR_ID && (line.m_lookatActor >= MAX_ACTORS))
	{
		GameWarning("CDialogScript::AddLine: Script='%s' Cannot add line because lookAtTargetID %d is out of range [0..%d]", m_id.c_str(), line.m_lookatActor, MAX_ACTORS - 1);
		return false;
	}

	m_lines.push_back(line);
	m_reqActorSet.SetActor(line.m_actor);
	if (line.m_lookatActor != NO_ACTOR_ID)
		m_reqActorSet.SetActor(line.m_lookatActor);

	m_bComplete = false;
	return true;
}

////////////////////////////////////////////////////////////////////////////
// Call this after all lines have been added
bool CDialogScript::Complete()
{
	m_bComplete = true;
	return true;
}

////////////////////////////////////////////////////////////////////////////
// Is the dialogscript completed
bool CDialogScript::IsCompleted() const
{
	return m_bComplete;
}

////////////////////////////////////////////////////////////////////////////
// Retrieves number of required actors
int CDialogScript::GetNumRequiredActors() const
{
	if (m_bComplete == false)
		return 0;
	return m_reqActorSet.NumActors();
}

////////////////////////////////////////////////////////////////////////////
// Get number of dialog lines
int CDialogScript::GetNumLines() const
{
	if (m_bComplete == false)
		return 0;
	return m_lines.size();
}

#if 0
////////////////////////////////////////////////////////////////////////////
// Get a certain line
const CDialogScript::SScriptLine& CDialogScript::GetLine(int index) const
{
	assert(index >= 0 && index < m_lines.size());
	if (m_bComplete && index >= 0 && index < m_lines.size())
		return m_lines[index];
	else return theEmptyScriptLine;
}
#endif

////////////////////////////////////////////////////////////////////////////
// Get a certain line
const CDialogScript::SScriptLine* CDialogScript::GetLine(int index) const
{
	assert(index >= 0 && index < m_lines.size());
	if (m_bComplete && index >= 0 && index < m_lines.size())
		return &m_lines[index];
	else return 0;
}

void CDialogScript::SetVersionFlags(uint32 which, bool bSet)
{
	if (bSet)
		m_versionFlags |= which;
	else
		m_versionFlags &= ~which;
}

void CDialogScript::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_id);
	pSizer->AddObject(m_desc);
	pSizer->AddObject(m_lines);
}
