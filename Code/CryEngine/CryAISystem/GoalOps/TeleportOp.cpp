// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TeleportOp.h"

#include <CrySystem/XML/XMLAttrReader.h>
#include "PipeUser.h"

TeleportOp::TeleportOp()
	: m_destinationPosition(ZERO)
	, m_destinationDirection(ZERO)
	, m_waitUntilAgentIsNotMoving(false)
{
}

TeleportOp::TeleportOp(const XmlNodeRef& node)
	: m_destinationPosition(ZERO)
	, m_destinationDirection(ZERO)
	, m_waitUntilAgentIsNotMoving(false)
{
}

TeleportOp::TeleportOp(const TeleportOp& rhs)
	: m_destinationPosition(rhs.m_destinationPosition)
	, m_destinationDirection(rhs.m_destinationDirection)
	, m_waitUntilAgentIsNotMoving(rhs.m_waitUntilAgentIsNotMoving)
{
	m_destinationDirection.Normalize();
}

void TeleportOp::Update(CPipeUser& pipeUser)
{
	if (m_waitUntilAgentIsNotMoving && IsAgentMoving(pipeUser))
	{
		return;
	}
	Teleport(pipeUser);

	GoalOpSucceeded();
}

void TeleportOp::Serialize(TSerialize serializer)
{
	serializer.BeginGroup("TeleportOp");

	serializer.Value("position", m_destinationPosition);
	serializer.Value("direction", m_destinationDirection);
	serializer.Value("waitUntilAgentIsNotMoving", m_waitUntilAgentIsNotMoving);

	serializer.EndGroup();
}

void TeleportOp::Teleport(CPipeUser& pipeUser)
{
	if (IEntity* entity = pipeUser.GetEntity())
	{
		Matrix34 transform = entity->GetWorldTM();

		const float rotationAngleInRadians = atan2f(-m_destinationDirection.x, m_destinationDirection.y);
		transform.SetRotationZ(rotationAngleInRadians, m_destinationPosition);

		entity->SetWorldTM(transform);
	}
}

bool TeleportOp::IsAgentMoving(CPipeUser& pipeUser)
{
	const SAIBodyInfo& bodyInfo = pipeUser.QueryBodyInfo();

	return bodyInfo.isMoving;
}

void TeleportOp::SetDestination(const Vec3& position, const Vec3& direction)
{
	m_destinationPosition = position;
	m_destinationDirection = direction;
	m_destinationDirection.Normalize();
}

void TeleportOp::SetWaitUntilAgentIsNotMovingBeforeTeleport()
{
	m_waitUntilAgentIsNotMoving = true;
}
