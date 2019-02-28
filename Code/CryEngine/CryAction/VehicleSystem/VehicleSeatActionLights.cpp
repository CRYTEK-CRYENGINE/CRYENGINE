// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a seat action to head/spot lights

   -------------------------------------------------------------------------
   History:
   - 01:03:2006: Created by Mathieu Pinard
   - 20:07:2010: Refactored by Paul Slinger

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehicleSeat.h"
#include "VehiclePartLight.h"
#include "VehicleSeatActionLights.h"

static const char* s_activationSounds[] =
{
	"sounds/vehicles:vehicle_accessories:light",
	"sounds/vehicles:vehicle_accessories:flashlight",
	"sounds/vehicles:vehicle_accessories:spotlight"
};

DEFINE_SHARED_PARAMS_TYPE_INFO(CVehicleSeatActionLights::SSharedParams)

CVehicleSeatActionLights::~CVehicleSeatActionLights()
{
	m_pVehicle->UnregisterVehicleEventListener(this);
}

//------------------------------------------------------------------------
bool CVehicleSeatActionLights::Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;
	m_pVehicle->RegisterVehicleEventListener(this, "SeatActionLights");

	CVehicleParams lightsTable = table.findChild("Lights");

	if (!lightsTable)
	{
		return false;
	}

	ELightActivation activation = eLA_Toggle;

	string inputActivation = lightsTable.getAttr("activation");

	if (inputActivation == "brake")
	{
		activation = eLA_Brake;
	}
	else if (inputActivation == "reverse")
	{
		activation = eLA_Reversing;
	}

	CryFixedStringT<256> sharedParamsName;

	sharedParamsName.Format("%s::SeatActionLights::%d::%s", pVehicle->GetEntity()->GetClass()->GetName(), pSeat->GetSeatId(), inputActivation.c_str());

	ISharedParamsManager* pSharedParamsManager = CCryAction::GetCryAction()->GetISharedParamsManager();

	CRY_ASSERT(pSharedParamsManager);

	m_pSharedParams = CastSharedParamsPtr<SSharedParams>(pSharedParamsManager->Get(sharedParamsName));

	if (!m_pSharedParams)
	{
		SSharedParams sharedParams;

		sharedParams.seatId = pSeat->GetSeatId();
		sharedParams.activation = activation;

		if (activation == eLA_Toggle)
		{
			if (lightsTable.haveAttr("sound"))
			{
				int sound = 1;

				if (!lightsTable.getAttr("sound", sound) || !sound)
				{
					sharedParams.onSound = lightsTable.getAttr("sound");
				}
				else if (sound > 0 && sound <= (CRY_ARRAY_COUNT(s_activationSounds)))
				{
					sharedParams.onSound = s_activationSounds[sound - 1];
				}

				sharedParams.offSound = sharedParams.onSound;
			}
			else
			{
				sharedParams.onSound = lightsTable.getAttr("onSound");
				sharedParams.offSound = lightsTable.getAttr("offSound");
			}
		}

		m_pSharedParams = CastSharedParamsPtr<SSharedParams>(pSharedParamsManager->Register(sharedParamsName, sharedParams));
	}

	CRY_ASSERT(m_pSharedParams.get());

	CVehicleParams lightsPartsArray = lightsTable.findChild("LightParts");

	if (lightsPartsArray)
	{
		int childCount = lightsPartsArray.getChildCount();

		m_lightParts.reserve(childCount);

		for (int i = 0; i < childCount; ++i)
		{
			if (CVehicleParams lightPartRef = lightsPartsArray.getChild(i))
			{
				if (IVehiclePart* pPart = pVehicle->GetPart(lightPartRef.getAttr("value")))
				{
					if (CVehiclePartLight* pPartLight = CAST_VEHICLEOBJECT(CVehiclePartLight, pPart))
					{
						m_lightParts.push_back(SLightPart(pPartLight));
					}
				}
			}
		}
	}

	m_enabled = false;

	return !m_lightParts.empty();
}

//------------------------------------------------------------------------
void CVehicleSeatActionLights::Reset()
{
	m_enabled = false;
}

//------------------------------------------------------------------------
void CVehicleSeatActionLights::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	if (actionId == eVAI_ToggleLights && m_pSharedParams->activation == eLA_Toggle)
	{
		if (eAAM_OnPress == activationMode)
		{
			ToggleLights(value == 0.f ? false : !m_enabled);

			if (CVehicleSeat* pSeat = static_cast<CVehicleSeat*>(m_pVehicle->GetSeatById(m_pSharedParams->seatId)))
			{
				pSeat->ChangedNetworkState(CVehicle::ASPECT_SEAT_ACTION);
			}
		}
	}
}

//---------------------------------------------------------------------------
void CVehicleSeatActionLights::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);

	pSizer->AddObject(m_lightParts);
}

//------------------------------------------------------------------------
void CVehicleSeatActionLights::Serialize(TSerialize ser, EEntityAspects aspects)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.Value("enabled", m_enabled);
	}
	else if (aspects & CVehicle::ASPECT_SEAT_ACTION)
	{
		NET_PROFILE_SCOPE("SeatAction_Lights", ser.IsReading());

		bool enabled = m_enabled;

		ser.Value("enabled", enabled, 'bool');

		if (ser.IsReading() && enabled != m_enabled)
		{
			ToggleLights(enabled);
		}
	}
}

//---------------------------------------------------------------------------
void CVehicleSeatActionLights::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	switch (event)
	{
	case eVE_Brake:
	case eVE_Reversing:
		{
			if ((event == eVE_Brake && eLA_Brake == m_pSharedParams->activation) || (event == eVE_Reversing && eLA_Reversing == m_pSharedParams->activation))
			{
				bool toggle = true;

				if (params.bParam)
				{
					// Only enable brake lights if engine is powered and driver is still inside.

					if (!m_pVehicle->GetMovement()->IsPowered())
					{
						toggle = false;
					}
					else if (IVehicleSeat* pSeat = m_pVehicle->GetSeatById(1))
					{
						if (!pSeat->GetPassenger() || pSeat->GetCurrentTransition() == IVehicleSeat::eVT_Exiting)
						{
							toggle = false;
						}
					}
				}

				if (toggle)
				{
					ToggleLights(params.bParam);
				}
			}

			break;
		}

	case eVE_EngineStopped:
		{
			if (eLA_Brake == m_pSharedParams->activation)
			{
				ToggleLights(false);
			}

			break;
		}
	}
}

//---------------------------------------------------------------------------
void CVehicleSeatActionLights::ToggleLights(bool enable)
{
	for (TVehiclePartLightVector::iterator iter = m_lightParts.begin(), end = m_lightParts.end(); iter != end; ++iter)
	{
		iter->pPart->ToggleLight(enable);
	}

	m_enabled = enable;

	if (enable && !m_pSharedParams->onSound.empty())
	{
		PlaySound(m_pSharedParams->onSound);
	}
	else if (!m_pSharedParams->offSound.empty())
	{
		PlaySound(m_pSharedParams->offSound);
	}
}

//---------------------------------------------------------------------------
void CVehicleSeatActionLights::PlaySound(const string& name)
{
	REINST("send event");
	/*IEntityAudioComponent* pIEntityAudioComponent = m_pVehicle->GetEntity()->GetOrCreateComponent<IEntityAudioComponent>();

	   if(pIEntityAudioComponent)
	   {
	   pIEntityAudioComponent->PlaySound(name.c_str(), Vec3(0.0f, 0.0f, 0.0f), FORWARD_DIRECTION, FLAG_SOUND_DEFAULT_3D, 0, eSoundSemantic_Vehicle);
	   }*/
}

DEFINE_VEHICLEOBJECT(CVehicleSeatActionLights);
