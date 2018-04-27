#include "StdAfx.h"
#include "LightEntity.h"

#include "Legacy/Helpers/EntityFlowNode.h"

#include <CrySerialization/Enum.h>

class CLightRegistrator
	: public IEntityRegistrator
{
public:
	virtual void Register() override
	{
		if (gEnv->pEntitySystem->GetClassRegistry()->FindClass("Light") != nullptr)
		{
			// Skip registration of default engine class if the game has overridden it
			CryLog("Skipping registration of default engine entity class Light, overridden by game");
			return;
		}

		RegisterEntityWithDefaultComponent<CDefaultLightEntity>("Light", "Lights", "Light.bmp");
	
		// Register flow node
		// Factory will be destroyed by flowsystem during shutdown
		pFlowNodeFactory = new CEntityFlowNodeFactory("entity:Light");

		pFlowNodeFactory->m_inputs.push_back(InputPortConfig<bool>("Active", ""));
		pFlowNodeFactory->m_inputs.push_back(InputPortConfig<bool>("Enable", ""));
		pFlowNodeFactory->m_inputs.push_back(InputPortConfig<bool>("Disable", ""));

		pFlowNodeFactory->m_activateCallback = CDefaultLightEntity::OnFlowgraphActivation;

		pFlowNodeFactory->m_outputs.push_back(OutputPortConfig<bool>("Active"));

		pFlowNodeFactory->Close();
	}

public:
	~CLightRegistrator()
	{
		if (pFlowNodeFactory)
			pFlowNodeFactory->UnregisterFactory();
		pFlowNodeFactory = nullptr;
	}

protected:
	_smart_ptr<CEntityFlowNodeFactory> pFlowNodeFactory = nullptr;
};

CLightRegistrator g_lightRegistrator;

YASLI_ENUM_BEGIN_NESTED(CDefaultLightEntity, ECastShadowsSpec, "CastShadowsSpec")
YASLI_ENUM_VALUE_NESTED(CDefaultLightEntity, eCastShadowsSpec_No, "None")
YASLI_ENUM_VALUE_NESTED(CDefaultLightEntity, eCastShadowsSpec_Low, "Low")
YASLI_ENUM_VALUE_NESTED(CDefaultLightEntity, eCastShadowsSpec_Medium, "Medium")
YASLI_ENUM_VALUE_NESTED(CDefaultLightEntity, eCastShadowsSpec_High, "High")
YASLI_ENUM_VALUE_NESTED(CDefaultLightEntity, eCastShadowsSpec_VeryHigh, "VeryHigh")
YASLI_ENUM_END()

CRYREGISTER_CLASS(CDefaultLightEntity);

CDefaultLightEntity::CDefaultLightEntity()
// Start by setting the light slot to an invalid value by default
	: m_lightSlot(-1)
	, m_bActive(true)
{
}

void CDefaultLightEntity::OnResetState()
{
	IEntity& entity = *GetEntity();

	// Check if we have to unload first
	if (m_lightSlot != -1)
	{
		entity.FreeSlot(m_lightSlot);
		m_lightSlot = -1;
	}

	// Check if the light is active
	if (!m_bActive)
	{
		ActivateFlowNodeOutput(eOutputPorts_Active, TFlowInputData(true));
		return;
	}

	ActivateFlowNodeOutput(eOutputPorts_Active, TFlowInputData(true));

	m_light.SetPosition(ZERO);
	m_light.m_fLightFrustumAngle = 45;
	m_light.m_fProjectorNearPlane = 0;
	m_light.m_Flags = DLF_DEFERRED_LIGHT | DLF_THIS_AREA_ONLY;

	if (m_bAffectsThisAreaOnly)
	{
		m_light.m_Flags |= DLF_THIS_AREA_ONLY;
	}
	if (m_bIgnoreVisAreas)
	{
		m_light.m_Flags |= DLF_IGNORES_VISAREAS;
	}
	if (m_bLinkToSkyColor)
	{
		m_light.m_Flags |= DLF_LINK_TO_SKY_COLOR;
	}
	if (m_bAmbient)
	{
		m_light.m_Flags |= DLF_AMBIENT;
	}
	if (m_bFake)
	{
		m_light.m_Flags |= DLF_FAKE;
	}
	if (m_bAffectVolumetricFog)
	{
		m_light.m_Flags |= DLF_VOLUMETRIC_FOG;
	}
	if (m_bAffectVolumetricFogOnly)
	{
		m_light.m_Flags |= DLF_VOLUMETRIC_FOG_ONLY;
	}

	m_light.m_ProbeExtents(m_light.m_fRadius, m_light.m_fRadius, m_light.m_fRadius);

	m_light.SetShadowBiasParams(1.f, 1.f);
	m_light.m_fShadowUpdateMinRadius = m_light.m_fRadius;

	float shadowUpdateRatio = 1.f;
	m_light.m_nShadowUpdateRatio = max((uint16)1, (uint16)(shadowUpdateRatio * (1 << DL_SHADOW_UPDATE_SHIFT)));

	if (m_castShadowSpec != eCastShadowsSpec_No && (int)gEnv->pSystem->GetConfigSpec() >= (int)m_castShadowSpec)
		m_light.m_Flags |= DLF_CASTSHADOW_MAPS;
	else
		m_light.m_Flags &= ~DLF_CASTSHADOW_MAPS;

	m_light.SetAnimSpeed(m_animSpeed);

	m_light.SetLightColor(m_diffuseColor * m_diffuseMultiplier);

	m_light.m_nSortPriority = 0;
	m_light.SetFalloffMax(1.0f);
	m_light.m_fProjectorNearPlane = 0.f;

	m_light.m_fFogRadialLobe = 0.f;

	SAFE_RELEASE(m_light.m_pLightImage);
	SAFE_RELEASE(m_light.m_pLightDynTexSource);

	if (m_projectorTexturePath.size() > 0)
	{
		const char* pExt = PathUtil::GetExt(m_projectorTexturePath);
		if (!stricmp(pExt, "swf") || !stricmp(pExt, "gfx") || !stricmp(pExt, "usm") || !stricmp(pExt, "ui"))
		{
			m_light.m_pLightDynTexSource = gEnv->pRenderer->EF_LoadDynTexture(m_projectorTexturePath, false);
		}
		else
		{
			m_light.m_pLightImage = gEnv->pRenderer->EF_LoadTexture(m_projectorTexturePath, 0);
		}

		if ((!m_light.m_pLightImage || !m_light.m_pLightImage->IsTextureLoaded()) && !m_light.m_pLightDynTexSource)
		{
			CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, 0, m_projectorTexturePath.c_str(),
				"Light projector texture not found: %s", m_projectorTexturePath.c_str());
			m_light.m_pLightImage = gEnv->pRenderer->EF_LoadTexture("Textures/defaults/red.dds", 0);
		}
	}

	if (m_flareTexturePath.size() > 0)
	{
		int nLensOpticsId;

		if (gEnv->pOpticsManager->Load(m_flareTexturePath, nLensOpticsId))
		{
			if (IOpticsElementBase* pOptics = gEnv->pOpticsManager->GetOptics(nLensOpticsId))
			{
				m_light.SetLensOpticsElement(pOptics);

				if (m_flareFieldOfView != 0)
				{
					int modularAngle = ((int)m_flareFieldOfView) % 360;
					if (modularAngle == 0)
						m_light.m_LensOpticsFrustumAngle = 255;
					else
						m_light.m_LensOpticsFrustumAngle = (uint8)(m_flareFieldOfView * (255.0f / 360.0f));
				}
				else
				{
					m_light.m_LensOpticsFrustumAngle = 0;
				}
			}
			else
			{
				m_light.SetLensOpticsElement(nullptr);
			}
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_ERROR, "Flare lens optics %s in light %s doesn't exist!", m_flareTexturePath.c_str(), GetEntity()->GetName());
			m_light.SetLensOpticsElement(nullptr);
		}
	}

	if (!(m_light.m_Flags & DLF_AREA_LIGHT) && m_light.m_fLightFrustumAngle && (m_light.m_pLightImage != NULL) && m_light.m_pLightImage->IsTextureLoaded() || m_light.m_pLightDynTexSource)
		m_light.m_Flags |= DLF_PROJECT;
	else
	{
		SAFE_RELEASE(m_light.m_pLightImage);
		SAFE_RELEASE(m_light.m_pLightDynTexSource);
		m_light.m_Flags |= DLF_POINT;
	}

	// Load the light source into the entity
	m_lightSlot = entity.LoadLight(1, &m_light);
}

void CDefaultLightEntity::OnFlowgraphActivation(EntityId entityId, IFlowNode::SActivationInfo* pActInfo, const class CEntityFlowNode* pNode)
{
	auto* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
	auto* pLightEntity = pEntity->GetComponent<CDefaultLightEntity>();

	if (IsPortActive(pActInfo, eInputPorts_Active))
	{
		pLightEntity->SetActive(GetPortBool(pActInfo, eInputPorts_Active));
	}
	else if (IsPortActive(pActInfo, eInputPorts_Enable))
	{
		pLightEntity->SetActive(true);
	}
	else if (IsPortActive(pActInfo, eInputPorts_Disable))
	{
		pLightEntity->SetActive(false);
	}
}