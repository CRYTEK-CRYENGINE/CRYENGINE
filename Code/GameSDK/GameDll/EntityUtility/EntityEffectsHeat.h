// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: Controls internal heat of entity (used together with thermal vision)

-------------------------------------------------------------------------
History:
- 7:06:2010: Benito G.R.		

*************************************************************************/

// IMPORTANT
//
// Add an object of this class to your entity (weapon, actor...), and ensure its
// life time is tied to your entity, because this is working with an entity pointer.

#pragma once

#ifndef _ENTITY_EFFECTS_HEAT_H_
#define _ENTITY_EFFECTS_HEAT_H_

namespace EntityEffects
{
	class CHeatController
	{
		struct SHeatPulse
		{
			SHeatPulse()
				: heat(0.0f)
				, baseTime(0.0f)
				, runningTime(0.0f)
			{

			}

			void Reset()
			{
				heat = 0.0f;
				baseTime = 0.0f;
				runningTime = 0.0f;
			}

			float heat;
			float baseTime;
			float runningTime;
		};

	public:
		CHeatController();

		void InitWithEntity(IEntity* pEntity, const float baseHeat);
		bool Update(const float frameTime);
		void AddHeatPulse(const float intensity, const float time);

		void Revive(const float baseHeat);
		void CoolDown(const float targetHeat);

	private:

		float UpdateHeat(const float frameTime);
		float UpdateCoolDown(const float frameTime);
		void  SetThermalVisionParams(const float scale);

		IEntity*	m_ownerEntity;
		float		m_baseHeat;
		float		m_coolDownHeat;

		SHeatPulse	m_heatPulse;
		bool		m_thermalVisionOn;
	};
}

#endif