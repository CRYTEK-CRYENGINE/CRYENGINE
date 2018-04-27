// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __HEALTH__H__
#define __HEALTH__H__

#include "Utility/MaskedVar.h"

class CHealth
{
private:
	TMaskedFloat m_health;
	float m_healthMax;
	TMaskedInt m_healthAsRoundedPercentage;

public:
	CHealth() : m_health(100.0f), m_healthMax(100.0f), m_healthAsRoundedPercentage(100) {}

	ILINE float GetHealth() const { return m_health; }
	ILINE void  SetHealth( float fHealth ) { m_health = fHealth;m_healthAsRoundedPercentage = int_ceil( m_health * 100.0f / m_healthMax); }
	ILINE float GetHealthMax() const { return m_healthMax; }
	ILINE void  SetHealthMax( float fHealthMax ) { m_healthMax = fHealthMax; }
	ILINE int   GetHealthAsRoundedPercentage() const { return m_healthAsRoundedPercentage; }
	ILINE bool  IsDead() const { return( m_health <= 0.0f ); }
};

#endif