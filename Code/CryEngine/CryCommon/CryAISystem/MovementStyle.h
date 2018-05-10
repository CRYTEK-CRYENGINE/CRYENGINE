// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CrySystem/XML/XMLAttrReader.h>
#include <CryAISystem/IAgent.h>

//! Defines how an agent should move to the destination.
//! Moving to cover, running, crouching and so on.
class MovementStyle
{
public:
	// When these are changed, ReadFromXml should be updated as well.
	enum Stance
	{
		Relaxed,
		Alerted,
		Stand,
		Crouch
	};

	enum Speed
	{
		Walk,
		Run,
		Sprint
	};

public:
	MovementStyle()
		: m_stance(Stand)
		, m_speed(Run)
		, m_speedLiteral(0.0f)
		, m_hasSpeedLiteral(false)
		, m_bodyOrientationMode(HalfwayTowardsAimOrLook)
		, m_movingToCover(false)
		, m_movingAlongDesignedPath(false)
		, m_turnTowardsMovementDirectionBeforeMoving(false)
		, m_strafe(false)
		, m_hasExactPositioningRequest(false)
		, m_glanceInMovementDirection(false)
	{
	}

	void ConstructDictionariesIfNotAlreadyDone();
	void ReadFromXml(const XmlNodeRef& node);

	void SetStance(const Stance stance)                                  { m_stance = stance; }
	void SetSpeed(const Speed speed)                                     { m_speed = speed; }
	void SetSpeedLiteral(float speedLiteral)                             { m_speedLiteral = speedLiteral; m_hasSpeedLiteral = true; }
	void SetMovingToCover(const bool movingToCover)                      { m_movingToCover = movingToCover; }
	void SetTurnTowardsMovementDirectionBeforeMoving(const bool enabled) { m_turnTowardsMovementDirectionBeforeMoving = enabled; }

	void SetMovingAlongDesignedPath(const bool movingAlongDesignedPath)  { m_movingAlongDesignedPath = movingAlongDesignedPath; }
	void SetExactPositioningRequest(const SAIActorTargetRequest* const pExactPositioningRequest)
	{
		if (pExactPositioningRequest != 0)
		{
			m_hasExactPositioningRequest = true;
			m_exactPositioningRequest = *pExactPositioningRequest;
		}
		else
		{
			m_hasExactPositioningRequest = false;
		}
	}

	Stance                       GetStance() const                                      { return m_stance; }
	Speed                        GetSpeed() const                                       { return m_speed; }
	bool                         HasSpeedLiteral() const                                { return m_hasSpeedLiteral; }
	float                        GetSpeedLiteral() const                                { return m_speedLiteral; }
	EBodyOrientationMode         GetBodyOrientationMode() const                         { return m_bodyOrientationMode; }
	bool                         IsMovingToCover() const                                { return m_movingToCover; }
	bool                         IsMovingAlongDesignedPath() const                      { return m_movingAlongDesignedPath; }
	bool                         ShouldTurnTowardsMovementDirectionBeforeMoving() const { return m_turnTowardsMovementDirectionBeforeMoving; }
	bool                         ShouldStrafe() const                                   { return m_strafe; }
	bool                         ShouldGlanceInMovementDirection() const                { return m_glanceInMovementDirection; }
	const SAIActorTargetRequest* GetExactPositioningRequest() const                     { return m_hasExactPositioningRequest ? &m_exactPositioningRequest : nullptr; }

private:
	Stance                m_stance;
	Speed                 m_speed;
	float                 m_speedLiteral;
	bool                  m_hasSpeedLiteral;
	EBodyOrientationMode  m_bodyOrientationMode;
	SAIActorTargetRequest m_exactPositioningRequest;
	bool                  m_movingToCover;
	bool                  m_movingAlongDesignedPath;
	bool                  m_turnTowardsMovementDirectionBeforeMoving;
	bool                  m_strafe;
	bool                  m_hasExactPositioningRequest;
	bool                  m_glanceInMovementDirection;
};

struct MovementStyleDictionaryCollection
{
	MovementStyleDictionaryCollection();

	CXMLAttrReader<MovementStyle::Speed>  speeds;
	CXMLAttrReader<MovementStyle::Stance> stances;
	CXMLAttrReader<EBodyOrientationMode>  bodyOrientations;
	CXMLAttrReader<bool>                  booleans;
};

inline MovementStyleDictionaryCollection::MovementStyleDictionaryCollection()
{
	speeds.Reserve(3);
	speeds.Add("Walk", MovementStyle::Walk);
	speeds.Add("Run", MovementStyle::Run);
	speeds.Add("Sprint", MovementStyle::Sprint);

	stances.Reserve(4);
	stances.Add("Relaxed", MovementStyle::Relaxed);
	stances.Add("Alerted", MovementStyle::Alerted);
	stances.Add("Stand", MovementStyle::Stand);
	stances.Add("Crouch", MovementStyle::Crouch);

	bodyOrientations.Reserve(3);
	bodyOrientations.Add("FullyTowardsMovementDirection", FullyTowardsMovementDirection);
	bodyOrientations.Add("FullyTowardsAimOrLook", FullyTowardsAimOrLook);
	bodyOrientations.Add("HalfwayTowardsAimOrLook", HalfwayTowardsAimOrLook);

	booleans.Reserve(4);
	booleans.Add("true", true);
	booleans.Add("false", false);
	booleans.Add("1", true);
	booleans.Add("0", false);
}

inline void MovementStyle::ReadFromXml(const XmlNodeRef& node)
{
	const MovementStyleDictionaryCollection movementStyleDictionaryCollection;
	movementStyleDictionaryCollection.speeds.Get(node, "speed", m_speed, true);
	movementStyleDictionaryCollection.stances.Get(node, "stance", m_stance);
	movementStyleDictionaryCollection.bodyOrientations.Get(node, "bodyOrientation", m_bodyOrientationMode);
	movementStyleDictionaryCollection.booleans.Get(node, "moveToCover", m_movingToCover);
	movementStyleDictionaryCollection.booleans.Get(node, "turnTowardsMovementDirectionBeforeMoving", m_turnTowardsMovementDirectionBeforeMoving);
	movementStyleDictionaryCollection.booleans.Get(node, "strafe", m_strafe);
	movementStyleDictionaryCollection.booleans.Get(node, "glanceInMovementDirection", m_glanceInMovementDirection);
}

//! \endcond INTERNAL