// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 23:09:2009   : Created by Filipe Amim

*************************************************************************/

#include "StdAfx.h"

#include "Recoil.h"
#include "Weapon.h"
#include "Player.h"
#include "Single.h"
#include "GameCVars.h"
#include "IronSight.h"
#include "Utility/CryWatch.h"

#include "GameXmlParamReader.h"
#include "ItemParamsRegistrationOperators.h"

IMPLEMENT_OPERATORS(RECOIL_PARAMS_MEMBERS, SRecoilParams)
IMPLEMENT_OPERATORS_WITH_VECTORS(RECOIL_HINT_PARAMS, RECOIL_HINT_PARAM_VECTORS, SRecoilHints)
IMPLEMENT_OPERATORS(PROCEDURALRECOIL_PARAMS_MEMBERS, SProceduralRecoilParams)
IMPLEMENT_OPERATORS(SPREAD_PARAMS_MEMBERS, SSpreadParams)

namespace
{

#	ifndef _RELEASE

	class CRecoilDebugDraw
	{
	private:
		struct SRecoilPoint
		{
			Vec3 direction;
			float recoil;
			float time;
		};

		typedef std::deque<SRecoilPoint> TRecoilPoints;
		typedef TRecoilPoints::iterator TRecoilPointsIterator;

	public:
		static void AddRecoilPoint(const Vec3& recoilDir, float recoil)
		{
			if(g_pGameCVars->i_debug_recoil)
			{
				GetRecoilDebugDraw().PushRecoilPoint(recoilDir, recoil);
			}
		}
		
		static void UpdateRecoilDraw(float frameTime)
		{
			if(g_pGameCVars->i_debug_recoil)
			{
				GetRecoilDebugDraw().Update(frameTime);
			}
		}
		
		static void DebugRecoil(float recoilPercentage, const Vec3 recoilOffset)
		{
			if(g_pGameCVars->i_debug_recoil)
			{
				CryWatch("RECOIL:\n Current Percentage Of Max Recoil: %.4f", recoilPercentage*100.f);
				CryWatch("\n Current Recoil Offset: %.4f %.4f", recoilOffset.x, recoilOffset.y);
			}
		}
		
		static void DebugSpread(float spread)
		{
			if(g_pGameCVars->i_debug_spread)
			{
				CryWatch("\n \nSPREAD: \n Current Spread: %.4f", spread);
			}
		}

	private:
		static CRecoilDebugDraw& GetRecoilDebugDraw()
		{
			static CRecoilDebugDraw debugDraw;
			return debugDraw;
		}

		void PushRecoilPoint(const Vec3& recoilDir, float recoil)
		{
			if (g_pGameCVars->i_debug_recoil)
			{
				SRecoilPoint recoilPoint;
				recoilPoint.direction = recoilDir;
				recoilPoint.recoil = recoil;
				recoilPoint.time = 0.0f;
				m_recoilPoints.push_back(recoilPoint);
			}
		}

		void Update(float frameTime)
		{
			if (g_pGameCVars->i_debug_recoil)
			{
				const float pointTimeOut = 2.0f;
				const Vec2 center = Vec2(400, 400);
				const float yellow[4] = {0, 1, 0, 1};
				const float pointSize = 3.0f;
				const float recoilScale = 100.0f;

				IRenderAuxText::Draw2dLabel(
					center.x, center.y,
					pointSize, yellow, false, ".");

				TRecoilPointsIterator it;
				for (it = m_recoilPoints.begin(); it != m_recoilPoints.end(); ++it)
				{
					it->time += frameTime;
					float opacity = SATURATE(1.0f-(it->time / pointTimeOut));
					const float red[4] = {1, 0, 0, opacity};
					IRenderAuxText::Draw2dLabel(
						it->direction.x*it->recoil*recoilScale + center.x,
						-it->direction.y*it->recoil*recoilScale + center.y,
						pointSize, red, false, ".");
				}

				while (!m_recoilPoints.empty())
				{
					it = m_recoilPoints.begin();
					if (it->time < pointTimeOut)
						break;
					m_recoilPoints.erase(it);
				}
			}
			else if (!m_recoilPoints.empty())
			{
				m_recoilPoints.clear();
			}
		}

		TRecoilPoints m_recoilPoints;
	};


	void DrawDebugZoomMods(const SRecoilParams& recoilParams, const SSpreadParams& spreadParams)
	{
		if(g_pGameCVars->i_debug_zoom_mods!=0)
		{
			float white[4] = {1,1,1,1};
			IRenderAuxText::Draw2dLabel(50.0f, 30.0f, 1.4f, white, false, "Recoil.attack : %f", recoilParams.attack);
			IRenderAuxText::Draw2dLabel(50.0f, 45.0f, 1.4f, white, false, "Recoil.decay : %f", recoilParams.decay);
			IRenderAuxText::Draw2dLabel(50.0f, 60.0f, 1.4f, white, false, "Recoil.impulse : %f", recoilParams.impulse);
			IRenderAuxText::Draw2dLabel(50.0f, 75.0f, 1.4f, white, false, "Recoil.max x,y : %f, %f", recoilParams.max.x, recoilParams.max.y);
			IRenderAuxText::Draw2dLabel(50.0f, 90.0f, 1.4f, white, false, "Recoil.max_recoil : %f", recoilParams.max_recoil);
			IRenderAuxText::Draw2dLabel(50.0f, 105.0f, 1.4f, white, false, "Recoil.recoil_crouch_m : %f", recoilParams.recoil_crouch_m);
			IRenderAuxText::Draw2dLabel(50.0f, 120.0f, 1.4f, white, false, "Recoil.recoil_jump_m : %f", recoilParams.recoil_jump_m);
			IRenderAuxText::Draw2dLabel(50.0f, 135.0f, 1.4f, white, false, "Recoil.recoil_strMode_m : %f", recoilParams.recoil_holdBreathActive_m);

			IRenderAuxText::Draw2dLabel(300.0f, 30.0f, 1.4f, white, false, "Spread.attack : %f", spreadParams.attack);
			IRenderAuxText::Draw2dLabel(300.0f, 45.0f, 1.4f, white, false, "Spread.decay : %f", spreadParams.decay);
			IRenderAuxText::Draw2dLabel(300.0f, 60.0f, 1.4f, white, false, "Spread.max : %f", spreadParams.max);
			IRenderAuxText::Draw2dLabel(300.0f, 75.0f, 1.4f, white, false, "Spread.min : %f", spreadParams.min);
			IRenderAuxText::Draw2dLabel(300.0f, 90.0f, 1.4f, white, false, "Spread.rotation_m : %f", spreadParams.rotation_m);
			IRenderAuxText::Draw2dLabel(300.0f, 105.0f, 1.4f, white, false, "Spread.speed_m : %f", spreadParams.speed_m);
			IRenderAuxText::Draw2dLabel(300.0f, 120.0f, 1.4f, white, false, "Spread.speed_holdBreathActive_m : %f", spreadParams.speed_holdBreathActive_m);
			IRenderAuxText::Draw2dLabel(300.0f, 135.0f, 1.4f, white, false, "Spread.spread_crouch_m : %f", spreadParams.spread_crouch_m);
			IRenderAuxText::Draw2dLabel(300.0f, 150.0f, 1.4f, white, false, "Spread.spread_jump_m : %f", spreadParams.spread_jump_m);
			IRenderAuxText::Draw2dLabel(300.0f, 165.0f, 1.4f, white, false, "Spread.spread_slide_m : %f", spreadParams.spread_slide_m);
			IRenderAuxText::Draw2dLabel(300.0f, 180.0f, 1.4f, white, false, "Spread.spread_holdBreathActive_m : %f", spreadParams.spread_holdBreathActive_m);
		}
	}


#	else


	class CRecoilDebugDraw
	{
	public:
		inline static void AddRecoilPoint(const Vec3& recoilDir, float recoil) {}
		inline static void UpdateRecoilDraw(float frameTime) {}
		inline static void DebugRecoil(float recoilPercentage, const Vec3 recoilOffset) { };
		inline static void DebugSpread(float spread) { };
	private:
	};

	inline void DrawDebugZoomMods(const SRecoilParams& recoilParams, const SSpreadParams& spreadParams) {}

#	endif

}



SSpreadModParams::SSpreadModParams()
{
	Reset(XmlNodeRef(NULL));
}


void SSpreadModParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit /*= true*/ )
{
	if (defaultInit)
	{
		min_mod = 1.0f;
		max_mod = 1.0f;
		attack_mod = 1.0f;
		decay_mod = 1.0f;
		end_decay_mod = 1.0f;
		speed_m_mod = 1.0f;
		speed_holdBreathActive_m_mod = 1.0f;
		rotation_m_mod = 1.0f;
		spread_crouch_m_mod = 1.0f;
		spread_jump_m_mod = 1.0f;
		spread_slide_m_mod = 1.0f;
		spread_holdBreathActive_m_mod = 1.0f;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<float>("min_mod", min_mod);
		reader.ReadParamValue<float>("max_mod", max_mod);
		reader.ReadParamValue<float>("attack_mod", attack_mod);
		reader.ReadParamValue<float>("decay_mod", decay_mod);
		reader.ReadParamValue<float>("end_decay_mod", end_decay_mod);
		reader.ReadParamValue<float>("speed_m_mod", speed_m_mod);
		reader.ReadParamValue<float>("speed_holdBreathActive_m_mod", speed_holdBreathActive_m_mod);
		reader.ReadParamValue<float>("rotation_m_mod", rotation_m_mod);
		reader.ReadParamValue<float>("spread_crouch_m_mod", spread_crouch_m_mod);
		reader.ReadParamValue<float>("spread_jump_m_mod", spread_jump_m_mod);
		reader.ReadParamValue<float>("spread_slide_m_mod", spread_slide_m_mod);
		reader.ReadParamValue<float>("spread_holdBreathActive_m_mod", spread_holdBreathActive_m_mod);
	}
}



SRecoilModParams::SRecoilModParams()
{
	Reset(XmlNodeRef(NULL));
}


void SRecoilModParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit /*= true*/ )
{
	if (defaultInit)
	{
		max_recoil_mod = 1.0f;
		attack_mod = 1.0f;
		first_attack_mod = 1.0f;
		decay_mod = 1.0f;
		end_decay_mod = 1.0f;
		max_mod.x = 1.0f;
		max_mod.y = 1.0f;
		impulse_mod = 1.0f;
		recoil_crouch_m_mod = 1.0f;
		recoil_jump_m_mod = 1.0f;
		recoil_holdBreathActive_m_mod = 1.0f;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<float>("max_recoil_mod", max_recoil_mod);
		reader.ReadParamValue<float>("attack_mod", attack_mod);
		reader.ReadParamValue<float>("first_attack_mod", first_attack_mod);
		reader.ReadParamValue<float>("decay_mod", decay_mod);
		reader.ReadParamValue<float>("end_decay_mod", end_decay_mod);
		reader.ReadParamValue<float>("maxx_mod", max_mod.x);
		reader.ReadParamValue<float>("maxy_mod", max_mod.y);
		reader.ReadParamValue<float>("impulse_mod", impulse_mod);
		reader.ReadParamValue<float>("recoil_crouch_m_mod", recoil_crouch_m_mod);
		reader.ReadParamValue<float>("recoil_jump_m_mod", recoil_jump_m_mod);
		reader.ReadParamValue<float>("recoil_holdBreathActive_m_mod", recoil_holdBreathActive_m_mod);
	}
}




SRecoilParams::SRecoilParams()
{
	Reset(XmlNodeRef(NULL));
}

void SRecoilParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit /*= true*/ )
{
	if (defaultInit)
	{
		max_recoil = 0.0f;
		attack = 0.0f;
		first_attack = 0.0f;
		decay = 0.65f;
		end_decay = 0.0f;
		recoil_time = 0.0f;
		max.x = 8.0f;
		max.y = 4.0f;
		tilt = 0.0f;
		randomness = 1.0f;
		impulse = 0.0f;
		recoil_crouch_m = 0.85f;
		recoil_jump_m = 1.5f;
		recoil_holdBreathActive_m = 0.5f;
	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<float>("max_recoil", max_recoil);
		reader.ReadParamValue<float>("attack", attack);
		reader.ReadParamValue<float>("first_attack", first_attack);
		reader.ReadParamValue<float>("decay", decay);
		reader.ReadParamValue<float>("end_decay", end_decay);
		reader.ReadParamValue<float>("recoil_time", recoil_time);
		reader.ReadParamValue<float>("maxx", max.x);
		reader.ReadParamValue<float>("maxy", max.y);
		reader.ReadParamValue<float>("tilt", tilt);
		reader.ReadParamValue<float>("randomness", randomness);
		reader.ReadParamValue<float>("impulse", impulse);
		reader.ReadParamValue<float>("recoil_crouch_m", recoil_crouch_m);
		reader.ReadParamValue<float>("recoil_jump_m", recoil_jump_m);
		reader.ReadParamValue<float>("recoil_holdBreathActive_m", recoil_holdBreathActive_m);
	}
}

void SRecoilParams::GetMemoryUsage( ICrySizer * s ) const 
{
	
}

void SRecoilHints::Reset(const XmlNodeRef& paramsNode, bool defaultInit /* = true */)
{
	if (defaultInit)
	{
		hints.resize(0);
	}

	CGameXmlParamReader reader(paramsNode);

	XmlNodeRef hintsNode = reader.FindFilteredChild("hints");
	if (hintsNode && (hintsNode->getChildCount() > 0))
	{
		//Replacing hints, delete previous ones
		hints.resize(0);

		CGameXmlParamReader hintsReader(hintsNode);

		Vec2 hintPoint;
		const int hintCount = hintsReader.GetUnfilteredChildCount();
		hints.reserve(hintCount);

		for (int i = 0; i < hintCount; i++)
		{
			XmlNodeRef hintNode = hintsReader.GetFilteredChildAt(i);
			if (hintNode && hintNode->getAttr("x", hintPoint.x) && hintNode->getAttr("y", hintPoint.y))
			{
				hints.push_back(hintPoint);
			}
		}
	}

}

void SRecoilHints::GetMemoryUsage(ICrySizer * s) const
{
	s->AddContainer(hints);
}

SProceduralRecoilParams::SProceduralRecoilParams()
{
	Reset(XmlNodeRef(NULL));
}

void SProceduralRecoilParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit /* = true */ )
{
	CGameXmlParamReader reader(paramsNode);

	if (defaultInit)
	{
		duration = 0.5f;
		strength = 0.1f;
		kickIn = 0.8f;
		arms = 0;

		dampStrength = 7.5f;
		fireRecoilTime = 0.05f;
		fireRecoilStrengthFirst = 3.0f;
		fireRecoilStrength = 0.5f;
		angleRecoilStrength = 0.4f;
		randomness = 0.1f;
	}

	reader.ReadParamValue<float>("duration", duration);
	reader.ReadParamValue<float>("strength", strength);
	reader.ReadParamValue<float>("kickIn", kickIn);
	reader.ReadParamValue<int>("arms", arms);

	reader.ReadParamValue<float>("dampStrength", dampStrength);
	reader.ReadParamValue<float>("fireRecoilTime", fireRecoilTime);
	reader.ReadParamValue<float>("fireRecoilStrengthFirst", fireRecoilStrengthFirst);
	reader.ReadParamValue<float>("fireRecoilStrength", fireRecoilStrength);
	reader.ReadParamValue<float>("angleRecoilStrength", angleRecoilStrength);
	reader.ReadParamValue<float>("randomness", randomness);

	if (defaultInit)
	{
		enabled = (paramsNode != NULL);
	}
	else
	{
		enabled = enabled || (paramsNode != NULL);
	}
}

SSpreadParams::SSpreadParams()
{
	Reset(XmlNodeRef(NULL));
}

void SSpreadParams::Reset( const XmlNodeRef& paramsNode, bool defaultInit /*=true*/ )
{
	if (defaultInit)
	{
		min = 0.015f;
		max = 0.0f;
		attack = 0.95f;
		decay = 0.65f;
		end_decay = 0.65f;
		speed_m = 0.25f;
		speed_holdBreathActive_m = 0.25f;
		rotation_m = 0.35f;

		spread_crouch_m = 0.85f;
		spread_jump_m = 1.5f;
		spread_slide_m = 1.0f;
		spread_holdBreathActive_m = 1.0f;

	}

	if (paramsNode)
	{
		CGameXmlParamReader reader(paramsNode);

		reader.ReadParamValue<float>("min", min);
		reader.ReadParamValue<float>("max", max);
		reader.ReadParamValue<float>("attack", attack);
		reader.ReadParamValue<float>("decay", decay);
		reader.ReadParamValue<float>("end_decay", end_decay);
		reader.ReadParamValue<float>("speed_m", speed_m);
		reader.ReadParamValue<float>("speed_holdBreathActive_m", speed_holdBreathActive_m);
		reader.ReadParamValue<float>("rotation_m", rotation_m);

		reader.ReadParamValue<float>("spread_crouch_m", spread_crouch_m);
		reader.ReadParamValue<float>("spread_jump_m", spread_jump_m);
		reader.ReadParamValue<float>("spread_slide_m", spread_slide_m);
		reader.ReadParamValue<float>("spread_holdBreathActive_m", spread_holdBreathActive_m);

	}
}




CRecoil::CRecoil()
	: m_pWeapon(0)
	, m_pFireMode(0)
	, m_recoil(0)
	, m_recoil_dir_idx(0)
	, m_recoil_dir(ZERO)
	, m_recoil_offset(ZERO)
	, m_recoil_time(0.0f)
	, m_attack(0.0f)
	, m_spread(0)
	, m_recoilMultiplier(1.0f)
	, m_spreadMultiplier(1.0f)
	, m_maxSpreadMultiplier(1.0f)
	, m_singleShot(false)
	, m_useRecoilMultiplier(false)
	, m_useSpreadMultiplier(false)
	, m_pRecoilHints(NULL)
{
}



void CRecoil::Update(CActor* pOwnerActor, float frameTime, bool weaponFired, int frameId, bool firstShot)
{
	if ((pOwnerActor == NULL) || (!pOwnerActor->IsClient()))
		return;

	float recoilScale = GetRecoilScale(*pOwnerActor);
	float maxRecoil = GetMaxRecoil();

	const bool weaponFiring = m_pFireMode->IsFiring();

#if TALOS
	if (weaponFired && stricmp(g_pGameCVars->pl_talos->GetString(), pOwnerActor->GetEntity()->GetName()) != 0)
#else
	if (weaponFired)
#endif
	{
		RecoilShoot(firstShot, maxRecoil);
		SpreadShoot();
	}

	UpdateRecoil(recoilScale, maxRecoil, weaponFired, weaponFiring, frameTime);
	UpdateSpread(weaponFired, weaponFiring, frameTime);

#if TALOS
	if (stricmp(g_pGameCVars->pl_talos->GetString(), pOwnerActor->GetEntity()->GetName()) == 0)
	{
		m_recoil = -1.0f;
		m_spread = 0.0f;
		m_spreadMultiplier = 0.0f;
	}
#endif

	if (m_recoil < 0.0f)
	{
		ResetRecoilInternal();
	}
	else if (m_recoil > 0.0f)
	{
		m_pWeapon->ApplyFPViewRecoil(
			frameId,
			Ang3(m_recoil_offset.y, m_recoil_offset.z, m_recoil_offset.x));
	}
}

#ifndef _RELEASE
void CRecoil::DebugUpdate(float frameTime) const
{
	DrawDebugZoomMods(m_recoilParams, m_spreadParams);
	CRecoilDebugDraw::UpdateRecoilDraw(frameTime);
}
#endif



void CRecoil::RecoilShoot(bool firstShot, float maxRecoil)
{
	float attack = firstShot ? m_recoilParams.first_attack : m_recoilParams.attack;
	attack = (float)__fsel(attack, attack, m_recoilParams.attack);
	m_attack = attack;
	m_recoil_time = m_recoilParams.recoil_time;
	if (m_recoil_time == 0.0f)
		m_recoil = clamp_tpl(m_recoil + m_attack, 0.0f, maxRecoil);

	Vec2 direction = Vec2(ZERO);
	const int recoilHintsCount = m_pRecoilHints ? m_pRecoilHints->hints.size() : 0;
	if (recoilHintsCount > 0)
	{
		direction = m_pRecoilHints->hints[m_recoil_dir_idx];
		m_recoil_dir_idx = (m_recoil_dir_idx+1) % recoilHintsCount;
	}

	const Vec3 randomDirectionAdd = Vec3(
		cry_random(0.0f, m_recoilParams.randomness),
		cry_random(-1.0f, 1.0f) * m_recoilParams.randomness,
		cry_random(0.0f, m_recoilParams.tilt));

	m_recoil_dir = direction.GetNormalized() + randomDirectionAdd;

	CRecoilDebugDraw::AddRecoilPoint(m_recoil_dir, m_recoil);
}



void CRecoil::UpdateRecoil(float recoilScale, float maxRecoil, bool weaponFired, bool weaponFiring, float frameTime)
{
	if (m_recoil_time > 0.0f)
	{
		m_recoil_time -= frameTime;
		m_recoil += (m_attack * frameTime) / m_recoilParams.recoil_time;
	}

	const bool isFiring = m_singleShot ? weaponFired : weaponFiring;
	const float decay = isFiring ? m_recoilParams.decay : m_recoilParams.end_decay;
	const float frameDecay = (float)__fsel(-decay, 0.0f, frameTime * recoilScale * maxRecoil * __fres(decay+FLT_EPSILON));
	m_recoil = clamp_tpl(m_recoil - frameDecay, 0.0f, maxRecoil);

	const float t = (float)__fsel(-maxRecoil, 0.0f, m_recoil * __fres(maxRecoil+FLT_EPSILON));
	const Vec3 new_offset = Vec3(
		m_recoil_dir.x * sin_tpl(DEG2RAD(m_recoilParams.max.x)),
		m_recoil_dir.y * sin_tpl(DEG2RAD(m_recoilParams.max.y)),
		m_recoil_dir.z) * t;

	m_recoil_offset = (new_offset * 0.66f) + (m_recoil_offset * 0.33f);

	CRecoilDebugDraw::DebugRecoil(t, m_recoil_offset);
}



void CRecoil::SpreadShoot()
{
	m_spread = clamp_tpl(m_spread + m_spreadParams.attack, m_spreadParams.min, (m_spreadParams.max * m_maxSpreadMultiplier));
}



void CRecoil::UpdateSpread(bool weaponFired, bool weaponFiring, float frameTime)
{
	const bool isFiring = m_singleShot ? weaponFired : weaponFiring;
	const float decay = isFiring ? m_spreadParams.decay : m_spreadParams.end_decay;
	const float frameDecay = (float)__fsel(-decay, 0.0f, ((m_spreadParams.max - m_spreadParams.min) * __fres(decay + FLT_EPSILON)) * frameTime);

	float newSpread = clamp_tpl(m_spread - frameDecay, m_spreadParams.min, (m_spreadParams.max * m_maxSpreadMultiplier));

	if(newSpread < m_spreadParams.max)
	{
		m_maxSpreadMultiplier = 1.f;
	}

	m_spread = newSpread;

	CRecoilDebugDraw::DebugSpread(GetSpread());
}

void CRecoil::Reset(bool spread)
{
	ResetRecoilInternal();
	
	if (spread)
	{
		ResetSpreadInternal();
	}
}

void CRecoil::ResetRecoilInternal()
{
	m_recoil = 0.0f;
	m_recoil_dir_idx = 0;
	m_recoil_dir = Vec2(0.0f,0.0f);
	m_recoil_offset = Vec2(0.0f,0.0f);
}

void CRecoil::ResetSpreadInternal()
{
	m_spread = m_spreadParams.min;
	m_spreadMultiplier = 1.f;
}

float CRecoil::GetRecoilScale(const CActor& weaponOwner) const
{
	//Same as for the spread (apply stance multipliers)
	float stanceScale = 1.0f;
	const float recoilScale = m_useRecoilMultiplier ? m_recoilMultiplier : 1.0f;

	const bool inAir = weaponOwner.IsPlayer() ? static_cast<const CPlayer&>(weaponOwner).IsInAir() : false;

	if (inAir)
	{
		stanceScale = m_recoilParams.recoil_jump_m;
	}
	else if (weaponOwner.GetStance() == STANCE_CROUCH)
	{
		stanceScale = m_recoilParams.recoil_crouch_m;
	}

	return recoilScale * stanceScale;
}


float CRecoil::GetMaxRecoil() const
{
	float scale = 1.0f;
	if (IsHoldingBreath())
		scale = (float)__fsel(-m_recoilParams.recoil_holdBreathActive_m, 1.0f, m_recoilParams.recoil_holdBreathActive_m);
	return m_recoilParams.max_recoil * scale;
}



float CRecoil::GetSpread() const
{
	CActor *pOwnerActor = m_pWeapon->GetOwnerActor();
	bool playerIsOwner = (pOwnerActor && pOwnerActor->IsPlayer());

	if (!playerIsOwner)
		return m_spread;

	const CActor& ownerActor = *pOwnerActor;
	const CPlayer& ownerPlayer = static_cast<const CPlayer&>(ownerActor);

	bool isHoldingBreath = IsHoldingBreath();
	float stanceScale = 1.0f;
	float speedSpread = 0.0f;
	float rotationSpread = 0.0f;
	float suitScale = 1.0f;
	float spreadScale = m_useSpreadMultiplier ? m_spreadMultiplier : 1.f;

	const SActorStats* pOwnerStats = ownerActor.GetActorStats();
	const SActorPhysics& actorPhysics = ownerActor.GetActorPhysics();

	bool inAir = ownerPlayer.IsInAir();

	if (inAir)
		stanceScale = m_spreadParams.spread_jump_m;
	else if ( ownerPlayer.IsSliding() )
		stanceScale = m_spreadParams.spread_slide_m;
	else if( ownerActor.GetStance() == STANCE_CROUCH )
		stanceScale = m_spreadParams.spread_crouch_m;

	if(inAir && gEnv->bMultiplayer)
	{
		speedSpread = pOwnerStats->maxAirSpeed * m_spreadParams.speed_m;
	}
	else
	{
		const float speed_m = isHoldingBreath ? m_spreadParams.speed_holdBreathActive_m : m_spreadParams.speed_m;
		float playerSpeed = pOwnerStats->mountedWeaponID != 0 ? pOwnerStats->speedFlat.Value() : actorPhysics.velocity.len();
		speedSpread = playerSpeed * speed_m;
	}

	rotationSpread = clamp_tpl(actorPhysics.angVelocity.len() * m_spreadParams.rotation_m, 0.0f, 3.0f);

	suitScale *= isHoldingBreath ? m_spreadParams.spread_holdBreathActive_m : 1.0f;

	return (speedSpread + rotationSpread + m_spread) * stanceScale * suitScale * spreadScale;

}



void CRecoil::RecoilImpulse(const Vec3& firingPos, const Vec3& firingDir)
{
	if (m_recoilParams.impulse > 0.f)
	{
		EntityId id = (m_pWeapon->GetHostId()) ? m_pWeapon->GetHostId() : m_pWeapon->GetOwnerId();
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
		IPhysicalEntity* pPhysicalEntity = pEntity ? pEntity->GetPhysics() : NULL;

		if (pPhysicalEntity)
		{        
			pe_action_impulse impulse;
			impulse.impulse = -firingDir * m_recoilParams.impulse; 
			impulse.point = firingPos;
			pPhysicalEntity->Action(&impulse);
		}
	}
}



void CRecoil::GetMemoryUsage(ICrySizer* s) const
{	
	s->AddObject(m_recoilParams);
	s->AddObject(m_spreadParams);	
}



void CRecoil::PatchSpreadMod(const SSpreadModParams& spreadMod, const SSpreadParams& originalSpreadParams, float modMultiplier)
{
	float oldSpreadMin = m_spreadParams.min;
	float oldSpreadMax = m_spreadParams.max;

	m_spreadParams.attack											= CalculateParamModValue(originalSpreadParams.attack, spreadMod.attack_mod, modMultiplier);
	m_spreadParams.decay											= CalculateParamModValue(originalSpreadParams.decay, spreadMod.decay_mod, modMultiplier);
	m_spreadParams.end_decay									= CalculateParamModValue(originalSpreadParams.end_decay, spreadMod.end_decay_mod, modMultiplier);
	m_spreadParams.max												= CalculateParamModValue(originalSpreadParams.max, spreadMod.max_mod, modMultiplier);
	m_spreadParams.min												= CalculateParamModValue(originalSpreadParams.min, spreadMod.min_mod, modMultiplier);
	m_spreadParams.rotation_m									= CalculateParamModValue(originalSpreadParams.rotation_m, spreadMod.rotation_m_mod, modMultiplier);
	m_spreadParams.speed_m										= CalculateParamModValue(originalSpreadParams.speed_m, spreadMod.speed_m_mod, modMultiplier);
	m_spreadParams.speed_holdBreathActive_m		= CalculateParamModValue(originalSpreadParams.speed_holdBreathActive_m, spreadMod.speed_holdBreathActive_m_mod, modMultiplier);
	m_spreadParams.spread_crouch_m						= CalculateParamModValue(originalSpreadParams.spread_crouch_m, spreadMod.spread_crouch_m_mod, modMultiplier);
	m_spreadParams.spread_jump_m							= CalculateParamModValue(originalSpreadParams.spread_jump_m, spreadMod.spread_jump_m_mod, modMultiplier);
	m_spreadParams.spread_slide_m							= CalculateParamModValue(originalSpreadParams.spread_slide_m, spreadMod.spread_slide_m_mod, modMultiplier);
	m_spreadParams.spread_holdBreathActive_m	= CalculateParamModValue(originalSpreadParams.spread_holdBreathActive_m, spreadMod.spread_holdBreathActive_m_mod, modMultiplier);

	float oldSpreadRange = oldSpreadMax-oldSpreadMin;
	if(oldSpreadRange)
	{
		float inverseOldSpreadRange = __fres(oldSpreadRange);
		float ratio = ((m_spread-oldSpreadMin) * inverseOldSpreadRange);
		m_spread = m_spreadParams.min + (m_spreadParams.max-m_spreadParams.min) * ratio;
	}
	else
	{
		m_spread = m_spreadParams.min;
	}

	m_useSpreadMultiplier = false;
}



void CRecoil::ResetSpreadMod(const SSpreadParams& originalSpreadParams)
{
	m_spreadParams = originalSpreadParams;
	m_spread = m_spreadParams.min;
	m_useSpreadMultiplier = true;
}



void CRecoil::PatchRecoilMod(const SRecoilModParams& recoilMod, const SRecoilParams& originalRecoilParams, float modMultiplier)
{
	m_recoilParams.attack											= CalculateParamModValue(originalRecoilParams.attack, recoilMod.attack_mod, modMultiplier);
	m_recoilParams.first_attack								= CalculateParamModValue(originalRecoilParams.first_attack, recoilMod.first_attack_mod, modMultiplier);
	m_recoilParams.decay											= CalculateParamModValue(originalRecoilParams.decay, recoilMod.decay_mod, modMultiplier);
	m_recoilParams.end_decay									= CalculateParamModValue(originalRecoilParams.end_decay, recoilMod.end_decay_mod, modMultiplier);
	m_recoilParams.impulse										= CalculateParamModValue(originalRecoilParams.impulse, recoilMod.impulse_mod, modMultiplier);
	m_recoilParams.max.x											= CalculateParamModValue(originalRecoilParams.max.x, recoilMod.max_mod.x, modMultiplier);
	m_recoilParams.max.y											= CalculateParamModValue(originalRecoilParams.max.y, recoilMod.max_mod.y, modMultiplier);
	m_recoilParams.max_recoil									= CalculateParamModValue(originalRecoilParams.max_recoil, recoilMod.max_recoil_mod, modMultiplier);
	m_recoilParams.recoil_crouch_m						= CalculateParamModValue(originalRecoilParams.recoil_crouch_m, recoilMod.recoil_crouch_m_mod, modMultiplier);
	m_recoilParams.recoil_jump_m							= CalculateParamModValue(originalRecoilParams.recoil_jump_m, recoilMod.recoil_jump_m_mod, modMultiplier);
	m_recoilParams.recoil_holdBreathActive_m	= CalculateParamModValue(originalRecoilParams.recoil_holdBreathActive_m, recoilMod.recoil_holdBreathActive_m_mod, modMultiplier);

	m_useRecoilMultiplier = true;
}



void CRecoil::ResetRecoilMod(const SRecoilParams& originalRecoilParams, const SRecoilHints* pRecoilHints)
{
	CRY_ASSERT(pRecoilHints);

	m_recoilParams = originalRecoilParams;
	m_pRecoilHints = pRecoilHints;
	m_useRecoilMultiplier = false;
}



bool CRecoil::IsSingleFireMode() const
{
	return m_pFireMode->GetRunTimeType() == CSingle::GetStaticType();
}

bool CRecoil::IsHoldingBreath() const
{
	IZoomMode* pZoomMode = m_pWeapon->GetZoomMode(m_pWeapon->GetCurrentZoomMode());
	if (pZoomMode)
	{
		return pZoomMode->IsStable();
	}
	return false;
}

void CRecoil::Init( CWeapon* pWeapon, CFireMode *pFiremode )
{
	assert(pWeapon);
	assert(pFiremode);

	m_pWeapon = pWeapon;
	m_pFireMode = pFiremode;

	m_singleShot = IsSingleFireMode();
}
