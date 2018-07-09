// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ShootOp.h"

#include <CrySystem/XML/XMLAttrReader.h>
#include "PipeUser.h"

#include <CryAISystem/IMovementSystem.h>
#include <CryAISystem/MovementRequest.h>

namespace
{
struct ShootOpDictionary
{
	ShootOpDictionary();
	CXMLAttrReader<ShootOp::ShootAt> shootAt;
	CXMLAttrReader<EFireMode>        fireMode;
};

ShootOpDictionary::ShootOpDictionary()
{
	shootAt.Reserve(3);
	shootAt.Add("Target", ShootOp::AttentionTarget);
	shootAt.Add("RefPoint", ShootOp::ReferencePoint);
	shootAt.Add("LocalSpacePosition", ShootOp::LocalSpacePosition);

	fireMode.Add("Off", FIREMODE_OFF);
	fireMode.Add("Burst", FIREMODE_BURST);
	fireMode.Add("Continuous", FIREMODE_CONTINUOUS);
	fireMode.Add("Forced", FIREMODE_FORCED);
	fireMode.Add("Aim", FIREMODE_AIM);
	fireMode.Add("Secondary", FIREMODE_SECONDARY);
	fireMode.Add("SecondarySmoke", FIREMODE_SECONDARY_SMOKE);
	fireMode.Add("Melee", FIREMODE_MELEE);
	fireMode.Add("Kill", FIREMODE_KILL);
	fireMode.Add("BurstWhileMoving", FIREMODE_BURST_WHILE_MOVING);
	fireMode.Add("PanicSpread", FIREMODE_PANIC_SPREAD);
	fireMode.Add("BurstDrawFire", FIREMODE_BURST_DRAWFIRE);
	fireMode.Add("MeleeForced", FIREMODE_MELEE_FORCED);
	fireMode.Add("BurstSnipe", FIREMODE_BURST_SNIPE);
	fireMode.Add("AimSweep", FIREMODE_AIM_SWEEP);
	fireMode.Add("BurstOnce", FIREMODE_BURST_ONCE);
}

ShootOpDictionary g_shootOpDictionary;
}

ShootOp::ShootOp()
	: m_position(ZERO)
	, m_duration(0.0f)
	, m_shootAt(LocalSpacePosition)
	, m_fireMode(FIREMODE_OFF)
	, m_stance(STANCE_NULL)
{
}

ShootOp::ShootOp(const XmlNodeRef& node)
	: m_position(ZERO)
	, m_duration(0.0f)
	, m_shootAt(LocalSpacePosition)
	, m_fireMode(FIREMODE_OFF)
	, m_stance(STANCE_NULL)
{
	g_shootOpDictionary.shootAt.Get(node, "at", m_shootAt, true);
	g_shootOpDictionary.fireMode.Get(node, "fireMode", m_fireMode, true);
	node->getAttr("position", m_position);
	node->getAttr("duration", m_duration);
	s_xml.GetStance(node, "stance", m_stance);
}

ShootOp::ShootOp(const ShootOp& rhs)
	: m_position(rhs.m_position)
	, m_duration(rhs.m_duration)
	, m_shootAt(rhs.m_shootAt)
	, m_fireMode(rhs.m_fireMode)
	, m_stance(rhs.m_stance)
{
}

void ShootOp::Enter(CPipeUser& pipeUser)
{
	if (m_shootAt == LocalSpacePosition)
		SetupFireTargetForLocalPosition(pipeUser);
	else if (m_shootAt == ReferencePoint)
		SetupFireTargetForReferencePoint(pipeUser);

	pipeUser.SetFireMode(m_fireMode);

	if (m_stance != STANCE_NULL)
		pipeUser.m_State.bodystate = m_stance;

	MovementRequest movementRequest;
	movementRequest.entityID = pipeUser.GetEntityID();
	movementRequest.type = MovementRequest::Stop;
	gEnv->pAISystem->GetMovementSystem()->QueueRequest(movementRequest);

	m_timeWhenShootingShouldEnd = gEnv->pTimer->GetFrameStartTime() + CTimeValue(m_duration);
}

void ShootOp::Leave(CPipeUser& pipeUser)
{
	if (m_dummyTarget)
	{
		pipeUser.SetFireTarget(NILREF);
		m_dummyTarget.Release();
	}

	pipeUser.SetFireMode(FIREMODE_OFF);
}

void ShootOp::Update(CPipeUser& pipeUser)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const CTimeValue& now = gEnv->pTimer->GetFrameStartTime();

	if (now >= m_timeWhenShootingShouldEnd)
		GoalOpSucceeded();
}

void ShootOp::Serialize(TSerialize serializer)
{
	serializer.BeginGroup("ShootOp");

	serializer.Value("position", m_position);
	serializer.Value("timeWhenShootingShouldEnd", m_timeWhenShootingShouldEnd);
	serializer.Value("duration", m_duration);
	serializer.EnumValue("shootAt", m_shootAt, ShootAtSerializationHelperFirst, ShootAtSerializationHelperLast);
	serializer.EnumValue("fireMode", m_fireMode, FIREMODE_SERIALIZATION_HELPER_FIRST, FIREMODE_SERIALIZATION_HELPER_LAST);
	m_dummyTarget.Serialize(serializer, "shootTarget");

	serializer.EndGroup();
}

void ShootOp::SetupFireTargetForLocalPosition(CPipeUser& pipeUser)
{
	// The user of this op has specified that the character
	// should fire at a predefined position in local-space.
	// Transform that position into world-space and store it
	// in an AI object. The pipe user will shoot at this.

	gAIEnv.pAIObjectManager->CreateDummyObject(m_dummyTarget);

	const Vec3 fireTarget = pipeUser.GetEntity()->GetWorldTM().TransformPoint(m_position);

	m_dummyTarget->SetPos(fireTarget);

	pipeUser.SetFireTarget(m_dummyTarget);
}

void ShootOp::SetupFireTargetForReferencePoint(CPipeUser& pipeUser)
{
	gAIEnv.pAIObjectManager->CreateDummyObject(m_dummyTarget);

	if (IAIObject* referencePoint = pipeUser.GetRefPoint())
	{
		m_dummyTarget->SetPos(referencePoint->GetPos());
	}
	else
	{
		gEnv->pLog->LogWarning("Agent '%s' is trying to shoot at the reference point but it hasn't been set.", pipeUser.GetName());
		m_dummyTarget->SetPos(ZERO);
	}

	pipeUser.SetFireTarget(m_dummyTarget);
}
