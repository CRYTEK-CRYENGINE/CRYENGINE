// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:  ActorsPresenceAwareness.h
//  Version:     v1.00
//  Created:     30/01/2012 by Francesco Roccucci.
//  Description: This is used for detecting the proximity of a specific 
//				set of AI agents to drive the selection of the behavior in
//				the tree without specific checks in the behavior itself
// -------------------------------------------------------------------------
//  History:
//	30/01/2012 12:00 - Added by Francesco Roccucci
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AloneDetectorModule_h__
#define __AloneDetectorModule_h__

#pragma once

#include "GameAIHelpers.h"

class Agent;

class AloneDetectorContainer : public CGameAIInstanceBase
{
public:
	typedef uint8 FactionID;

	struct AloneDetectorSetup
	{
		enum State
		{
			Alone,
			EntitiesInRange,
			Unkown,
		};

		AloneDetectorSetup(
			const float _rangeSq, 
			const char* _aloneSignal, 
			const char* _notAloneSignal
			)
			:range(_rangeSq)
			,aloneSignal(_aloneSignal)
			,notAloneSignal(_notAloneSignal)
			,state(Unkown)
		{
		}

		AloneDetectorSetup():state(Unkown), range(0.0f)
		{
		}

		float		range;
		string		aloneSignal;
		string		notAloneSignal;
		State		state;
	};

	void		Update(float frameTime);
	void		SetupDetector(const AloneDetectorSetup& setup);
	void		AddEntityClass(const char* const entityClassName);
	void		RemoveEntityClass(const char* const entityClassName);
	void		ResetDetector();
	bool		IsAlone() const;

private:
	float		GetSqDistanceFromLocation(const Vec3& location) const;
	void		SendCorrectSignal();

	const char* GetActorClassName(const Agent& agent) const;
	bool		IsActorValid(const Agent& agent) const;

	AloneDetectorSetup	m_setup;
	typedef std::vector<string> EntitiesList;
	EntitiesList		m_entityClassNames;
};

class AloneDetectorModule : public AIModuleWithInstanceUpdate<AloneDetectorModule, AloneDetectorContainer, 16, 8>
{
public:
	virtual const char* GetName() const { return "AloneDetector"; }
};

#endif // __AloneDetectorModule_h__