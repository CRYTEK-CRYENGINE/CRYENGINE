#include "StdAfx.h"
#include "ProjectorLightComponent.h"

#include <CrySystem/IProjectManager.h>
#include <ILevelSystem.h>
#include <Cry3DEngine/IRenderNode.h>

#include <array>

namespace Cry
{
namespace DefaultComponents
{
void CProjectorLightComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	// Functions
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CProjectorLightComponent::Enable, "{54F96D7F-3B98-47F2-B256-4AD856CFE5BD}"_cry_guid, "Enable");
		pFunction->SetDescription("Enables or disables the light component");
		pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member, Schematyc::EEnvFunctionFlags::Construction });
		pFunction->BindInput(1, 'ena', "Enable");
		componentScope.Register(pFunction);
	}
}

void CProjectorLightComponent::Initialize()
{
	if (!m_bActive)
	{
		FreeEntitySlot();

		return;
	}

	SRenderLight light;

	light.m_nLightStyle = m_animations.m_style;
	light.SetAnimSpeed(m_animations.m_speed);

	light.SetPosition(ZERO);
	light.m_Flags = DLF_DEFERRED_LIGHT | DLF_PROJECT;

	light.m_fLightFrustumAngle = m_angle.ToDegrees();
	light.m_fProjectorNearPlane = m_projectorOptions.m_nearPlane;

	light.SetLightColor(m_color.m_color * m_color.m_diffuseMultiplier);
	light.SetSpecularMult(m_color.m_specularMultiplier);

	light.m_fHDRDynamic = 0.f;

	if (m_options.m_bAffectsOnlyThisArea)
		light.m_Flags |= DLF_THIS_AREA_ONLY;

	if (m_options.m_bIgnoreVisAreas)
		light.m_Flags |= DLF_IGNORES_VISAREAS;

	if (m_options.m_bVolumetricFogOnly)
		light.m_Flags |= DLF_VOLUMETRIC_FOG_ONLY;

	if (m_options.m_bAffectsVolumetricFog)
		light.m_Flags |= DLF_VOLUMETRIC_FOG;

	if (m_options.m_bLinkToSkyColor)
		light.m_Flags |= DLF_LINK_TO_SKY_COLOR;

	if (m_options.m_bAmbient)
		light.m_Flags |= DLF_AMBIENT;

	//TODO: Automatically add DLF_FAKE when using beams or flares

	if (m_shadows.m_castShadowSpec != EMiniumSystemSpec::Disabled && (int)gEnv->pSystem->GetConfigSpec() >= (int)m_shadows.m_castShadowSpec)
	{
		light.m_Flags |= DLF_CASTSHADOW_MAPS;

		light.SetShadowBiasParams(1.f, 1.f);
		light.m_fShadowUpdateMinRadius = light.m_fRadius;

		float shadowUpdateRatio = 1.f;
		light.m_nShadowUpdateRatio = max((uint16)1, (uint16)(shadowUpdateRatio * (1 << DL_SHADOW_UPDATE_SHIFT)));
	}
	else
		light.m_Flags &= ~DLF_CASTSHADOW_MAPS;

	light.SetRadius(m_radius, m_options.m_attenuationBulbSize);

	light.m_fFogRadialLobe = m_options.m_fogRadialLobe;

	const char* szProjectorTexturePath = m_projectorOptions.GetTexturePath();
	if (szProjectorTexturePath[0] == '\0')
	{
		szProjectorTexturePath = "%ENGINE%/EngineAssets/Textures/lights/softedge.dds";
	}

	const char* pExt = PathUtil::GetExt(szProjectorTexturePath);
	if (!stricmp(pExt, "swf") || !stricmp(pExt, "gfx") || !stricmp(pExt, "usm") || !stricmp(pExt, "ui"))
	{
		light.m_pLightDynTexSource = gEnv->pRenderer->EF_LoadDynTexture(szProjectorTexturePath, false);
	}
	else
	{
		light.m_pLightImage = gEnv->pRenderer->EF_LoadTexture(szProjectorTexturePath, 0);
	}

	if ((light.m_pLightImage == nullptr || !light.m_pLightImage->IsTextureLoaded()) && light.m_pLightDynTexSource == nullptr)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Light projector texture %s not found, disabling projector component for entity %s", szProjectorTexturePath, m_pEntity->GetName());
		FreeEntitySlot();
		return;
	}

	if (m_flare.HasTexturePath())
	{
		int nLensOpticsId;

		if (gEnv->pOpticsManager->Load(m_flare.GetTexturePath(), nLensOpticsId))
		{
			IOpticsElementBase* pOptics = gEnv->pOpticsManager->GetOptics(nLensOpticsId);
			CRY_ASSERT(pOptics != nullptr);

			if (pOptics != nullptr)
			{
				light.SetLensOpticsElement(pOptics);

				float flareAngle = m_flare.m_angle.ToDegrees();

				if (flareAngle != 0)
				{
					int modularAngle = ((int)flareAngle) % 360;
					if (modularAngle == 0)
						light.m_LensOpticsFrustumAngle = 255;
					else
						light.m_LensOpticsFrustumAngle = (uint8)(flareAngle * (255.0f / 360.0f));
				}
				else
				{
					light.m_LensOpticsFrustumAngle = 0;
				}
			}
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Flare lens optics %s for projector component in entity %s doesn't exist!", m_flare.GetTexturePath(), GetEntity()->GetName());
			light.SetLensOpticsElement(nullptr);
		}
	}

	if (m_optics.m_flareEnable && !m_optics.m_lensFlareName.empty())
	{
		int32 opticsIndex = 0;
		if (gEnv->pOpticsManager->Load(m_optics.m_lensFlareName.c_str(), opticsIndex))
		{
			IOpticsElementBase* pOpticsElement = gEnv->pOpticsManager->GetOptics(opticsIndex);
			light.SetLensOpticsElement(pOpticsElement);

			const int32 modularAngle = m_optics.m_flareFOV % 360;
			if (modularAngle == 0)
				light.m_LensOpticsFrustumAngle = 255;
			else
				light.m_LensOpticsFrustumAngle = (uint8)(m_optics.m_flareFOV * (255.0f / 360.0f));

			if (m_optics.m_attachToSun)
			{
				light.m_Flags |= DLF_ATTACH_TO_SUN | DLF_FAKE | DLF_IGNORES_VISAREAS;
				light.m_Flags &= ~DLF_THIS_AREA_ONLY;
			}
		}
	}

	m_pEntity->UpdateLightClipBounds(light);

	// Load the light source into the entity
	m_pEntity->LoadLight(GetOrMakeEntitySlotId(), &light);

	bool needsDefaultLensFlareMaterial = m_optics.m_flareEnable;

	if (m_projectorOptions.HasMaterialPath())
	{
		// Allow setting a specific material for the light in this slot, for example to set up beams
		if (IMaterial* pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(m_projectorOptions.GetMaterialPath(), false))
		{
			m_pEntity->SetSlotMaterial(GetEntitySlotId(), pMaterial);
			needsDefaultLensFlareMaterial = false;
		}
	}

	// Set the default lens flare material in case the user hasn't specified one.
	if(needsDefaultLensFlareMaterial)
	{
		IMaterial* pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(g_szDefaultLensFlareMaterialName);
		if (pMaterial)
			m_pEntity->SetSlotMaterial(GetEntitySlotId(), pMaterial);
	}

	CryTransform::CTransformPtr pTransform = m_pTransform;

	Matrix34 slotTransform = pTransform != nullptr ? pTransform->ToMatrix34() : IDENTITY;
	slotTransform = slotTransform * Matrix33::CreateRotationZ(gf_PI * 0.5f);

	// Fix light orientation to point along the forward axis
	// This has to be done since lights in the engine currently emit from the right axis for some reason.
	m_pEntity->SetSlotLocalTM(GetEntitySlotId(), slotTransform);

	// Restore to the user specified transform, as SetSlotLocalTM might override it
	m_pTransform = pTransform;

	uint32 slotFlags = m_pEntity->GetSlotFlags(GetEntitySlotId());
	UpdateGIModeEntitySlotFlags((uint8)m_options.m_giMode, slotFlags);
	m_pEntity->SetSlotFlags(GetEntitySlotId(), slotFlags);
}

void CProjectorLightComponent::ProcessEvent(const SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED || ENTITY_EVENT_BIT(ENTITY_EVENT_LINK) || ENTITY_EVENT_BIT(ENTITY_EVENT_DELINK))
	{
		Initialize();
	}
}

uint64 CProjectorLightComponent::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED) | ENTITY_EVENT_BIT(ENTITY_EVENT_LINK) | ENTITY_EVENT_BIT(ENTITY_EVENT_DELINK);
}

#ifndef RELEASE
void CProjectorLightComponent::Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const
{
	if (context.bSelected)
	{
		Matrix34 slotTransform = GetWorldTransformMatrix();

		SRenderLight light;
		light.SetLightColor(m_color.m_color * m_color.m_diffuseMultiplier);
		light.SetRadius(m_radius, m_options.m_attenuationBulbSize);

		float distance = light.m_fRadius;
		float size = distance * tan(m_angle.ToRadians());

		std::array<Vec3, 4> points = 
		{ {
			Vec3(size, distance, size),
			Vec3(-size, distance, size),
			Vec3(-size, distance, -size),
			Vec3(size, distance, -size)
		} };

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(slotTransform.GetTranslation(), context.debugDrawInfo.color, slotTransform.TransformPoint(points[0]), context.debugDrawInfo.color);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(slotTransform.GetTranslation(), context.debugDrawInfo.color, slotTransform.TransformPoint(points[1]), context.debugDrawInfo.color);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(slotTransform.GetTranslation(), context.debugDrawInfo.color, slotTransform.TransformPoint(points[2]), context.debugDrawInfo.color);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(slotTransform.GetTranslation(), context.debugDrawInfo.color, slotTransform.TransformPoint(points[3]), context.debugDrawInfo.color);

		Vec3 p1 = slotTransform.TransformPoint(points[0]);
		Vec3 p2;
		for (int i = 0; i < points.size(); i++)
		{
			int j = (i + 1) % points.size();
			p2 = slotTransform.TransformPoint(points[j]);
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(p1, context.debugDrawInfo.color, p2, context.debugDrawInfo.color);
			p1 = p2;
		}
	}
}
#endif

void CProjectorLightComponent::SProjectorOptions::SetTexturePath(const char* szPath)
{
	m_texturePath = szPath;
}

void CProjectorLightComponent::SProjectorOptions::SetMaterialPath(const char* szPath)
{
	m_materialPath = szPath;
}

void CProjectorLightComponent::SFlare::SetTexturePath(const char* szPath)
{
	m_texturePath = szPath;
}

void CProjectorLightComponent::Enable(bool bEnable) 
{
	m_bActive = bEnable; 

	Initialize();
}
}
}
