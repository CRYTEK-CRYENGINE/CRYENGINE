// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CommunicationChannel.h"

CommunicationChannel::CommunicationChannel(const CommunicationChannel::Ptr& parent,
                                           const SCommunicationChannelParams& params,
                                           const CommChannelID& channelID)
	: m_parent(parent)
	, m_minSilence(params.minSilence)
	, m_silence(0.0f)
	, m_flushSilence(params.flushSilence)
	, m_actorMinSilence(params.actorMinSilence)
	, m_priority(params.priority)
	, m_id(channelID)
	, m_occupied(false)
	, m_type(params.type)
	, m_ignoreActorSilence(params.ignoreActorSilence)
{}

void CommunicationChannel::Update(float updateTime)
{
	if (!m_occupied && (m_silence > 0.0f))
		m_silence -= updateTime;
}

void CommunicationChannel::Occupy(bool occupy, float minSilence)
{
	assert((occupy && IsFree()) || (!occupy && !IsFree()));

	if (m_occupied && !occupy)
		m_silence = (minSilence >= 0.0f) ? minSilence : m_minSilence;
	m_occupied = occupy;

	if (m_parent)
		m_parent->Occupy(occupy, minSilence);

}

bool CommunicationChannel::IsFree() const
{
	if (m_parent && !m_parent->IsFree())
		return false;

	return !m_occupied && (m_silence <= 0.0f);
}

void CommunicationChannel::Clear()
{
	m_occupied = false;
	m_silence = 0.0;
}

void CommunicationChannel::ResetSilence()
{
	if (m_parent && !m_parent->IsOccupied())
		m_parent->ResetSilence();

	m_silence = m_flushSilence;
}
