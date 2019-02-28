// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehiclePartPulsingLight.h"

CVehiclePartPulsingLight::CVehiclePartPulsingLight()
	: m_colorMult(0.f)
	, m_minColorMult(0.f)
	, m_currentColorMultSpeed(0.f)
	, m_colorMultSpeed(0.f)
	, m_colorMultSpeedStageTwo(0.f)
	, m_toggleOnMinDamageRatio(0.f)
	, m_toggleStageTwoMinDamageRatio(0.f)
	, m_colorChangeTimer(0.f)
{
}

bool CVehiclePartPulsingLight::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType)
{
	if (!CVehiclePartLight::Init(pVehicle, table, parent, initInfo, eVPT_Light))
		return false;

	if (CVehicleParams lightTable = table.findChild("PulsingLight"))
	{
		lightTable.getAttr("minColorMult", m_minColorMult);
		lightTable.getAttr("toggleOnMinDamageRatio", m_toggleOnMinDamageRatio);
		lightTable.getAttr("colorMultSpeed", m_colorMultSpeed);
		lightTable.getAttr("toggleStageTwoMinDamageRatio", m_toggleStageTwoMinDamageRatio);
		lightTable.getAttr("colorMultSpeedStageTwo", m_colorMultSpeedStageTwo);
	}

	return true;
}

void CVehiclePartPulsingLight::UpdateLight(const float frameTime)
{
	CVehiclePartLight::UpdateLight(frameTime);

	SEntitySlotInfo info;
	if (m_pVehicle->GetEntity()->GetSlotInfo(m_slot, info) && info.pLight)
	{
		float fNewColorTimer = m_colorChangeTimer + (frameTime * m_currentColorMultSpeed);

		float timerScaledToWavelength = fNewColorTimer * gf_PI;
		timerScaledToWavelength = (float)__fsel(timerScaledToWavelength - gf_PI2, timerScaledToWavelength - gf_PI2, timerScaledToWavelength);

		m_colorMult = sinf(timerScaledToWavelength) * 0.5f + 0.5f;

		m_colorMult *= 1.0f - m_minColorMult;
		m_colorMult += m_minColorMult;
		m_colorChangeTimer = fNewColorTimer;

		SRenderLight& light = info.pLight->GetLightProperties();
		light.SetLightColor(ColorF(m_diffuseCol * m_diffuseMult[0] * m_colorMult));
	}
}

void CVehiclePartPulsingLight::ToggleLight(bool enable)
{
	CVehiclePartLight::ToggleLight(enable);

	if (enable)
	{
		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);
	}
}

void CVehiclePartPulsingLight::OnEvent(const SVehiclePartEvent& event)
{
	if (event.type == eVPE_Damaged)
	{
		if (event.fparam < 1.0f)
		{
			if (m_toggleOnMinDamageRatio && !IsEnabled() && event.fparam >= m_toggleOnMinDamageRatio)
			{
				ToggleLight(true);
				m_currentColorMultSpeed = m_colorMultSpeed;
			}
			else if (m_toggleStageTwoMinDamageRatio && event.fparam >= m_toggleStageTwoMinDamageRatio)
			{
				m_currentColorMultSpeed = m_colorMultSpeedStageTwo;
			}
		}
		else //fParam >= 1.f (Vehicle destroyed)
		{
			ToggleLight(false);
		}
	}

	CVehiclePartLight::OnEvent(event);
}

DEFINE_VEHICLEOBJECT(CVehiclePartPulsingLight);
