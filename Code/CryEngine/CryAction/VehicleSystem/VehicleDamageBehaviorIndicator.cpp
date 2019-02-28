// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleDamageBehaviorIndicator.h"

#include "Vehicle.h"

// all damage lights use same min/max frequency for consistency
float CVehicleDamageBehaviorIndicator::m_frequencyMin = 0.5f;
float CVehicleDamageBehaviorIndicator::m_frequencyMax = 4.f;

//------------------------------------------------------------------------
CVehicleDamageBehaviorIndicator::CVehicleDamageBehaviorIndicator()
	: m_pVehicle(nullptr)
	, m_isActive(false)
	, m_ratioMin(1.0f)
	, m_ratioMax(1.0f)
	, m_currentDamageRatio(0.0f)
	, m_pSubMtl(nullptr)
	, m_soundRatioMin(1.0f)
	, m_pHelper(nullptr)
	, m_soundsPlayed(0)
	, m_lastDamageRatio(0.0f)
	, m_lightUpdate(0.0f)
	, m_lightTimer(0.0f)
	, m_lightOn(false)
{}

//------------------------------------------------------------------------
bool CVehicleDamageBehaviorIndicator::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	m_pVehicle = (CVehicle*) pVehicle;

	table.getAttr("damageRatioMin", m_ratioMin);

	CVehicleParams indicatorParams = table.findChild("Indicator");
	if (!indicatorParams)
		return false;

	if (CVehicleParams params = indicatorParams.findChild("Light"))
	{
		m_material = params.getAttr("material");
		m_sound = params.getAttr("sound");
		m_sound.MakeLower();

		m_soundRatioMin = m_ratioMin;
		params.getAttr("soundRatioMin", m_soundRatioMin);

		if (params.haveAttr("helper"))
		{
			m_pHelper = m_pVehicle->GetHelper(params.getAttr("helper"));
		}
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorIndicator::Reset()
{
	m_currentDamageRatio = 0.f;
	m_lastDamageRatio = 0.f;
	m_soundsPlayed = 0;
	m_lightOn = false;

	EnableLight(false);

	if (m_isActive)
	{
		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
		m_isActive = false;
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorIndicator::SetActive(bool activate)
{
	if (activate && !m_isActive)
	{
		if (!m_pSubMtl && m_frequencyMin > 0.f)
		{
			// first time, get material
			GetMaterial();
		}

		if (m_pSubMtl || !m_sound.empty())
		{
			m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);
			m_isActive = true;
		}

		m_soundsPlayed = 0;
	}
	else if (!activate && m_isActive)
	{
		m_isActive = false;
		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorIndicator::EnableLight(bool enable)
{
	if (m_pSubMtl)
	{
		float glow = (enable) ? 1.f : 0.f;
		m_pSubMtl->SetGetMaterialParamFloat("emissive_intensity", glow, false);
	}

	m_lightOn = enable;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorIndicator::OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams)
{
	if (event == eVDBE_Hit || event == eVDBE_Repair)
	{
		if (behaviorParams.componentDamageRatio >= m_ratioMin && behaviorParams.componentDamageRatio < m_ratioMax)
		{
			if (!m_isActive)
			{
				SetActive(true);
			}

			if (m_isActive && behaviorParams.componentDamageRatio != m_currentDamageRatio)
			{
				m_currentDamageRatio = behaviorParams.componentDamageRatio;

				float ratio = (m_currentDamageRatio - m_ratioMin) / (m_ratioMax - m_ratioMin);
				CRY_ASSERT(ratio >= 0.f && ratio <= 1.f);
				m_lightUpdate = 0.5f / (m_frequencyMin + ratio * (m_frequencyMax - m_frequencyMin));
			}
		}
		else
		{
			bool glow = false;

			if (behaviorParams.componentDamageRatio >= m_ratioMax)
			{
				// set steady glow
				glow = true;

				if (!m_isActive)
				{
					SetActive(true);
				}
			}

			EnableLight(glow);

			// disable
			SetActive(false);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorIndicator::Update(const float deltaTime)
{
	if (m_isActive && m_lightUpdate > 0.f && m_pVehicle->IsPlayerPassenger())
	{
		m_lightTimer += deltaTime;

		if (m_lightTimer >= 2.f * m_lightUpdate)
		{
			// off
			EnableLight(false);
			m_lightTimer = 0.f;
		}
		else if (!m_lightOn && m_lightTimer >= m_lightUpdate)
		{
			// on
			EnableLight(true);
			REINST("report damage ratio?");
			//      if (!m_sound.empty() && m_currentDamageRatio >= m_soundRatioMin)
			//      {
			//        IEntityAudioComponent* pIEntityAudioComponent = m_pVehicle->GetEntity()->GetComponent<IEntityAudioComponent>();
			//        if (pIEntityAudioComponent)
			//        {
			//          if (m_lastDamageRatio != m_currentDamageRatio)
			//            m_soundsPlayed = 0;
			//          else
			//            ++m_soundsPlayed;
			//
			//          /*if (ISound* pSound = pIEntityAudioComponent->GetSound(m_soundId))
			//          {
			//            if (pSound->IsPlaying())
			//              pSound->Stop();
			//          } */
			//
			//          Vec3 offset(ZERO);
			//
			//          if (m_pHelper)
			//						offset = m_pHelper->GetVehicleSpaceTranslation();
			//
			//         m_soundId = pIEntityAudioComponent->PlaySound(m_sound.c_str(), offset, Vec3Constants<float>::fVec3_OneY, FLAG_SOUND_DEFAULT_3D, 0, eSoundSemantic_Vehicle);
			//
			//          const int nSoundsMaxVol = 6;
			//          const int nSoundsMinVol = 10;
			////          const static float fMinVol = 0.5f;
			//
			//          if (m_soundsPlayed > nSoundsMaxVol)
			//          {
			//            // reduce volume after n sounds
			//            if (ISound* pSound = pIEntityAudioComponent->GetSound(m_soundId))
			//            {
			//              float vol = 1.f - 0.5f*min(1.f, float(m_soundsPlayed-nSoundsMaxVol)/float(nSoundsMinVol-nSoundsMaxVol));
			//              pSound->GetInterfaceExtended()->SetVolume(vol);
			//            }
			//          }
			//        }
			//      }
		}
		m_lastDamageRatio = m_currentDamageRatio;
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorIndicator::GetMaterial()
{
	if (!m_material.empty())
	{
		IEntityRender* pIEntityRender = m_pVehicle->GetEntity()->GetRenderInterface();
		
		{
			// use slot 0 here. if necessary, part can be added as attribute
			IMaterial* pMtl = pIEntityRender->GetRenderMaterial(0);
			if (pMtl)
			{
				for (int i = 0; i < pMtl->GetSubMtlCount(); ++i)
				{
					IMaterial* pSubMtl = pMtl->GetSubMtl(i);
					if (pSubMtl && 0 == _stricmp(pSubMtl->GetName(), m_material.c_str()))
					{
						m_pSubMtl = pSubMtl;
						break;
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorIndicator::Serialize(TSerialize ser, EEntityAspects aspects)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		bool active = m_isActive;
		ser.Value("isActive", m_isActive);

		if (active != m_isActive)
			SetActive(m_isActive);

		ser.Value("damageRatio", m_currentDamageRatio);
		ser.Value("nSoundsPlayed", m_soundsPlayed);
		ser.Value("lastRatio", m_lastDamageRatio);
		ser.Value("lightTimer", m_lightTimer);

		bool light = m_lightOn;
		ser.Value("lightOn", light);

		if (ser.IsReading())
		{
			if (light != m_lightOn)
				EnableLight(light);
		}
	}
}

void CVehicleDamageBehaviorIndicator::GetMemoryUsage(ICrySizer* s) const
{
	s->AddObject(this, sizeof(*this));
	s->AddObject(m_material);
}

DEFINE_VEHICLEOBJECT(CVehicleDamageBehaviorIndicator);
