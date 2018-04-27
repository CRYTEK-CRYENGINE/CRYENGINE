// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a light part

   -------------------------------------------------------------------------
   History:
   - 24:10:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"

#include <CryAnimation/ICryAnimation.h>
#include "IVehicleSystem.h"

#include "CryAction.h"
#include "Vehicle.h"
#include "VehicleComponent.h"
#include "VehiclePartBase.h"
#include "VehiclePartLight.h"

#define DEFAULT_FLOAT -1.f
#define DEFAULT_INT   -1
#define DEFAULT_VEC3  Vec3(0)

BEGIN_SHARED_PARAMS(SVehicleLightParams)

SVehicleLightParams() : radius(5.0f),
	                      diffuseMult(1.0f),
	                      diffuseMult_fp(1.0f),
	                      specularMult(1.0f),
	                      frustumAngle(45.0f),
	                      HDRDynamic(1.0f),
	                      animSpeed(1.0f),
	                      animPhase(0),
	                      diffuse(1.0f, 1.0f, 1.0f),
	                      specular(1.0f, 1.0f, 1.0f),
	                      style(0),
	                      viewDistanceRatio(0),
	                      castShadows(0),
	                      fakeLight(false),
	                      flareFOV(0.0f),
	                      falloff(1.0f)
{
}

string texture, material, flare;

float radius, diffuseMult, diffuseMult_fp, specularMult, frustumAngle, HDRDynamic, animSpeed, flareFOV, falloff;

Vec3 diffuse, specular;

int style;
int animPhase;
int viewDistanceRatio;
int castShadows;

bool fakeLight;

END_SHARED_PARAMS

DEFINE_SHARED_PARAMS_TYPE_INFO(SVehicleLightParams)

namespace
{
SVehicleLightParamsConstPtr LoadVehicleLightParams(ISharedParamsManager* pSharedParamsManager, const char* inputLightType)
{
	const XmlNodeRef lightsRootNode = gEnv->pSystem->LoadXmlFromFile("Scripts/Entities/Vehicles/Lights/DefaultVehicleLights.xml");

	IF_UNLIKELY (lightsRootNode == (IXmlNode*)NULL)
		return SVehicleLightParamsConstPtr();

	const XmlNodeRef lightsListNode = lightsRootNode->findChild("Lights");
	IF_UNLIKELY (lightsListNode == (IXmlNode*)NULL)
		return SVehicleLightParamsConstPtr();

	const int numberOfLights = lightsListNode->getChildCount();

	for (int i = 0; i < numberOfLights; ++i)
	{
		const XmlNodeRef lightParamsNode = lightsListNode->getChild(i);
		assert(lightParamsNode != (IXmlNode*)NULL);

		IF_UNLIKELY (strcmp(lightParamsNode->getTag(), "Light") != 0)
			continue;

		const char* type = lightParamsNode->getAttr("type");

		if (!pSharedParamsManager->Get(type))
		{
			SVehicleLightParams lightParams;

			lightParams.texture = lightParamsNode->getAttr("texture");
			lightParams.material = lightParamsNode->getAttr("material");
			lightParams.flare = lightParamsNode->getAttr("flare");
			if (!lightParamsNode->getAttr("flareFOV", lightParams.flareFOV))
			{
				lightParams.flareFOV = 180.0f;
			}

			lightParamsNode->getAttr("radius", lightParams.radius);
			lightParamsNode->getAttr("diffuseMult", lightParams.diffuseMult);

			if (!lightParamsNode->getAttr("diffuseMult_fp", lightParams.diffuseMult_fp))
			{
				lightParams.diffuseMult_fp = lightParams.diffuseMult;
			}

			lightParamsNode->getAttr("specularMult", lightParams.specularMult);
			lightParamsNode->getAttr("frustumAngle", lightParams.frustumAngle);

			lightParamsNode->getAttr("diffuse", lightParams.diffuse);
			lightParamsNode->getAttr("specular", lightParams.specular);

			lightParamsNode->getAttr("HDRDynamic", lightParams.HDRDynamic);

			lightParamsNode->getAttr("animSpeed", lightParams.animSpeed);
			lightParamsNode->getAttr("animPhase", lightParams.animPhase);

			lightParamsNode->getAttr("style", lightParams.style);
			lightParamsNode->getAttr("viewDistanceRatio", lightParams.viewDistanceRatio);
			lightParamsNode->getAttr("castShadows", lightParams.castShadows);

			lightParamsNode->getAttr("fakeLight", lightParams.fakeLight);
			lightParamsNode->getAttr("falloff", lightParams.falloff);

			SVehicleLightParamsConstPtr pNewVehicleLightParams = CastSharedParamsPtr<SVehicleLightParams>(pSharedParamsManager->Register(type, lightParams));

			if (strcmp(type, inputLightType) == 0)
			{
				return pNewVehicleLightParams;
			}
		}
	}

	return SVehicleLightParamsConstPtr();
}

};

//------------------------------------------------------------------------
CVehiclePartLight::CVehiclePartLight()
{
	m_lightViewDistanceRatio = 0;
	m_pMaterial = 0;
	m_pHelper = 0;

	m_diffuseMult[0] = m_diffuseMult[1] = 1.f;
	m_diffuseCol.Set(1, 1, 1);
	m_enabled = false;
}

//------------------------------------------------------------------------
CVehiclePartLight::~CVehiclePartLight()
{
	ToggleLight(false);
}

//------------------------------------------------------------------------
bool CVehiclePartLight::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType)
{
	if (!CVehiclePartBase::Init(pVehicle, table, parent, initInfo, eVPT_Light))
		return false;

	float specularMul = 1.0f;

	m_light.m_Flags |= DLF_DEFERRED_LIGHT;
	m_light.m_Flags &= ~DLF_DISABLED;

	m_light.m_nLightStyle = 0;
	m_light.SetPosition(Vec3(ZERO));
	m_light.SetRadius(5.0f);

	if (CVehicleParams lightTable = table.findChild("Light"))
	{
		m_lightType = lightTable.getAttr("type");

		ISharedParamsManager* pSharedParamsManager = CCryAction::GetCryAction()->GetISharedParamsManager();

		CRY_ASSERT(pSharedParamsManager);

		SVehicleLightParamsConstPtr pVehicleLightParams = CastSharedParamsPtr<SVehicleLightParams>(pSharedParamsManager->Get(m_lightType.c_str()));

		if (!pVehicleLightParams)
		{
			pVehicleLightParams = LoadVehicleLightParams(pSharedParamsManager, m_lightType.c_str());
		}

		if (pVehicleLightParams)
		{
			m_light.SetRadius(pVehicleLightParams->radius);
			m_diffuseCol = pVehicleLightParams->diffuse;
			m_diffuseMult[1] = pVehicleLightParams->diffuseMult;
			m_diffuseMult[0] = pVehicleLightParams->diffuseMult_fp;
			specularMul = pVehicleLightParams->specularMult;

			m_light.m_nLightStyle = pVehicleLightParams->style;
			m_light.m_fLightFrustumAngle = pVehicleLightParams->frustumAngle;
			m_light.m_fHDRDynamic = pVehicleLightParams->HDRDynamic;
			m_light.m_nLightPhase = pVehicleLightParams->animPhase;

			m_light.SetAnimSpeed(pVehicleLightParams->animSpeed);
			m_light.SetFalloffMax(pVehicleLightParams->falloff);

			m_lightViewDistanceRatio = pVehicleLightParams->viewDistanceRatio;

			if (pVehicleLightParams->castShadows > 0)
			{
				const ICVar* pLightSpec = gEnv->pConsole->GetCVar("sys_spec_light");
				int configSpec = (pLightSpec != NULL) ? pLightSpec->GetIVal() : gEnv->pSystem->GetConfigSpec(true);

				// Treating consoles and custom as med spec for simplicity
				if (configSpec == CONFIG_CUSTOM)
				{
					configSpec = CONFIG_MEDIUM_SPEC;
				}

				if (pVehicleLightParams->castShadows <= configSpec)
				{
					m_light.m_Flags |= DLF_CASTSHADOW_MAPS;
				}
			}

			m_light.m_Flags = pVehicleLightParams->fakeLight ? m_light.m_Flags | DLF_FAKE : m_light.m_Flags & ~DLF_FAKE;

			if (pVehicleLightParams->texture.empty() == false && gEnv->pRenderer)
			{
				m_light.m_pLightImage = gEnv->pRenderer->EF_LoadTexture(pVehicleLightParams->texture.c_str(), 0);
			}

			if (pVehicleLightParams->material.empty() == false)
			{
				m_pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(pVehicleLightParams->material.c_str());
			}

			if (pVehicleLightParams->flare.empty() == false && gEnv->pRenderer)
			{
				int lensOpticsID = 0;
				if (gEnv->pOpticsManager->Load(pVehicleLightParams->flare.c_str(), lensOpticsID))
				{
					m_light.SetLensOpticsElement(gEnv->pOpticsManager->GetOptics(lensOpticsID));

					int modularAngle = ((int)pVehicleLightParams->flareFOV) % 360;
					if (modularAngle == 0)
						m_light.m_LensOpticsFrustumAngle = 255;
					else
						m_light.m_LensOpticsFrustumAngle = (uint8)(pVehicleLightParams->flareFOV * (255.0f / 360.0f));
				}

				if (m_pMaterial == NULL)
				{
					GameWarning("VehiclePartLight - Light uses flares, but it does not have a material. Flares won't work");
				}
			}

		}
	}

	m_light.SetLightColor(ColorF(m_diffuseCol * m_diffuseMult[1], 1.f));
	m_light.SetSpecularMult(specularMul / m_diffuseMult[1]);

	if (m_light.m_fLightFrustumAngle && m_light.m_pLightImage && m_light.m_pLightImage->IsTextureLoaded())
	{
		m_light.m_Flags |= DLF_PROJECT;
	}
	else
	{
		if (m_light.m_pLightImage)
		{
			m_light.m_pLightImage->Release();
			m_light.m_pLightImage = 0;
		}
		m_light.m_Flags |= DLF_POINT;
	}

	return true;
}

//------------------------------------------------------------------------
void CVehiclePartLight::PostInit()
{
	m_pHelper = m_pVehicle->GetHelper(m_pSharedParameters->m_helperPosName.c_str());

	// get Components this Part belongs to.
	// currently that's only needed for Lights.
	for (int i = 0, nComps = m_pVehicle->GetComponentCount(); i < nComps; ++i)
	{
		IVehicleComponent* pComponent = m_pVehicle->GetComponent(i);

		for (int j = 0, nParts = pComponent->GetPartCount(); j < nParts; ++j)
		{
			if (pComponent->GetPart(j) == this)
			{
				m_components.push_back(pComponent);
				break;
			}
		}
	}

	if (VehicleCVars().v_lights_enable_always)
		ToggleLight(true);
}

//------------------------------------------------------------------------
void CVehiclePartLight::Reset()
{
	CVehiclePartBase::Reset();

	ToggleLight(false);
}

//------------------------------------------------------------------------
void CVehiclePartLight::OnEvent(const SVehiclePartEvent& event)
{
	if (event.type == eVPE_Damaged)
	{
		if (event.fparam >= 1.0f)
		{
			float dmg = 1.f / max(1.f, (float)m_components.size());
			m_damageRatio += dmg;

			if (m_damageRatio >= 1.f && IsEnabled())
				ToggleLight(false);
		}
	}
	else
	{
		CVehiclePartBase::OnEvent(event);
	}
}

//------------------------------------------------------------------------
void CVehiclePartLight::ToggleLight(bool enable)
{
	if (enable && !IsEnabled())
	{
		// 0: no lights at all (only intended for debugging etc.)
		// 1: lights only enabled for the local player
		// 2: all lights enabled
		if (VehicleCVars().v_lights == 0
		    || (VehicleCVars().v_lights == 1 && !m_pVehicle->IsPlayerPassenger()))
			return;

		if (m_pHelper && m_damageRatio < 1.0f)
		{
			m_slot = GetEntity()->LoadLight(m_slot, &m_light);

			if (m_slot != -1)
			{
				if (m_pMaterial)
				{
					GetEntity()->SetSlotMaterial(m_slot, m_pMaterial);
				}

				if (m_lightViewDistanceRatio > 0)
				{
					SEntitySlotInfo slotInfo;
					if (GetEntity()->GetSlotInfo(m_slot, slotInfo) && (slotInfo.pLight != NULL))
					{
						slotInfo.pLight->SetViewDistRatio(m_lightViewDistanceRatio);
					}
				}
			}

			UpdateLight(0.f);
		}

		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_Visible);
		m_enabled = true;
	}
	else if (!enable)
	{
		if (m_slot != -1)
		{
			GetEntity()->FreeSlot(m_slot);
			m_slot = -1;
		}

		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
		m_enabled = false;
	}

}

//-----------------------------------------------------------------------
void CVehiclePartLight::Update(const float frameTime)
{
	if (IsEnabled())
		UpdateLight(frameTime);
}

//-----------------------------------------------------------------------
void CVehiclePartLight::UpdateLight(const float frameTime)
{
	if (m_slot == -1)
		return;

	// move to vehicle event change view?
	if (m_diffuseMult[0] != m_diffuseMult[1])
	{
		SEntitySlotInfo info;
		if (m_pVehicle->GetEntity()->GetSlotInfo(m_slot, info) && info.pLight)
		{
			SRenderLight& light = info.pLight->GetLightProperties();

			IActor* pActor = CCryAction::GetCryAction()->GetClientActor();
			bool localPlayer = (pActor != NULL) && (pActor->GetLinkedVehicle() == m_pVehicle);

			IVehicleSeat* pSeat = pActor ? m_pVehicle->GetSeatForPassenger(pActor->GetEntityId()) : NULL;
			IVehicleView* pView = pSeat ? pSeat->GetCurrentView() : NULL;
			bool isThirdPersonView = pView ? pView->IsThirdPerson() : true;
			if (localPlayer && !isThirdPersonView)
				light.SetLightColor(ColorF(m_diffuseCol * m_diffuseMult[0], 1.f));
			else
				light.SetLightColor(ColorF(m_diffuseCol * m_diffuseMult[1], 1.f));
		}
	}

	if (m_pHelper)
	{
		const static Matrix33 rot(Matrix33::CreateRotationXYZ(Ang3(0.f, 0.f, DEG2RAD(90.f))));

		Matrix34 helperTM;
		m_pHelper->GetVehicleTM(helperTM);
		Matrix34 localTM = Matrix33(helperTM) * rot;
		localTM.SetTranslation(helperTM.GetTranslation());

		GetEntity()->SetSlotLocalTM(m_slot, localTM);
	}

}

//------------------------------------------------------------------------
void CVehiclePartLight::Serialize(TSerialize ser, EEntityAspects aspects)
{
	CVehiclePartBase::Serialize(ser, aspects);

	if (ser.GetSerializationTarget() != eST_Network)
	{
		bool isEnabled = IsEnabled();
		ser.Value("lightEnabled", isEnabled, 'bool');

		if (ser.IsReading())
		{
			ToggleLight(isEnabled);
		}
	}
}

DEFINE_VEHICLEOBJECT(CVehiclePartLight);
