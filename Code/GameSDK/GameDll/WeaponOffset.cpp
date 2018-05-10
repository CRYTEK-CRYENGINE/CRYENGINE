// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WeaponOffset.h"
#include "Utility/Hermite.h"
#include <Mannequin/Serialization.h>


void SWeaponOffset::Serialize(Serialization::IArchive& ar)
{
	ar(m_position, "Position", "Position");
	ar(m_rotation, "Rotation", "Rotation");
}

CWeaponOffsetState::CWeaponOffsetState()
	:	m_current(ZERO)
	,	m_last(ZERO)
	,	m_transitiontime(0.0f)
	,	m_time(0.0f)
{
}



void CWeaponOffsetState::SetState(const SWeaponOffset& offset, float transitionTime)
{
	m_last = ComputeCurrentOffset();
	m_current = offset;
	m_time = 0.0f;
	m_transitiontime = transitionTime;
}



SWeaponOffset CWeaponOffsetState::Blend(float deltaTime)
{
	SWeaponOffset result = ComputeCurrentOffset();
	m_time += deltaTime;
	return result;
}



SWeaponOffset CWeaponOffsetState::ComputeCurrentOffset()
{
	SWeaponOffset result;

	if (m_transitiontime > 0.0f)
	{
		float t = Hermite2(SATURATE(m_time / m_transitiontime));
		result.m_position = LERP(m_last.m_position, m_current.m_position, t);
		result.m_rotation = Lerp(m_last.m_rotation, m_current.m_rotation, t);
	}
	else
	{
		result = m_current;
	}

	return result;
}




CWeaponOffsetStack::CWeaponOffsetStack()
	:	m_nextId(0)
{
	m_weaponOffsetLayers.reserve(4);
}



SWeaponOffset CWeaponOffsetStack::Blend(float deltaTime)
{
	return m_state.Blend(deltaTime);
}



void CWeaponOffsetStack::SetOffset(const SWeaponOffset& offset, float blendTime)
{
	m_state.SetState(offset, blendTime);
}



CWeaponOffsetStack::TOffsetId CWeaponOffsetStack::PushOffset(const SWeaponOffset& offset, uint32 layer, float blendTime)
{
	TOffsetId lastActiveId = m_weaponOffsetLayers.empty() ? -1 : m_weaponOffsetLayers.begin()->m_id;

	TWeaponOffsetLayers::iterator it = m_weaponOffsetLayers.begin();
	for (; it != m_weaponOffsetLayers.end(); ++it)
	{
		if (it->m_layer <= layer)
			break;
	}

	SWeaponOffsetLayer layerOffset;
	layerOffset.m_offset = offset;
	layerOffset.m_layer = layer;
	layerOffset.m_id = m_nextId++;
	m_weaponOffsetLayers.insert(it, layerOffset);

	if (m_weaponOffsetLayers.begin()->m_id != lastActiveId)
		m_state.SetState(m_weaponOffsetLayers[0].m_offset, blendTime);

	return layerOffset.m_id;
}



void CWeaponOffsetStack::PopOffset(TOffsetId offsetId, float blendTime)
{
	TOffsetId lastActiveId = m_weaponOffsetLayers.empty() ? -1 : m_weaponOffsetLayers.begin()->m_id;

	TWeaponOffsetLayers::iterator it = m_weaponOffsetLayers.begin();
	for (; it != m_weaponOffsetLayers.end(); ++it)
	{
		if (it->m_id == offsetId)
			break;
	}
	if (it != m_weaponOffsetLayers.end())
		m_weaponOffsetLayers.erase(it);

	if (m_weaponOffsetLayers.empty())
		m_state.SetState(SWeaponOffset(ZERO), blendTime);
	else if(m_weaponOffsetLayers.begin()->m_id != lastActiveId)
		m_state.SetState(m_weaponOffsetLayers[0].m_offset, blendTime);
}
