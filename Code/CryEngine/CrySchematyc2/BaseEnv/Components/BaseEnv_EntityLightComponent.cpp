// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BaseEnv/Components/BaseEnv_EntityLightComponent.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CryRenderer/IRenderer.h>

#include "BaseEnv/BaseEnv_AutoRegistrar.h"

namespace SchematycBaseEnv
{
	namespace EntityLightComponentUtils
	{
		struct SPreviewProperties : public Schematyc2::IComponentPreviewProperties
		{
			SPreviewProperties()
				: bShowGizmos(false)
				, gizmoLength(1.0f)
			{}

			// Schematyc2::IComponentPreviewProperties

			virtual void Serialize(Serialization::IArchive& archive) override
			{
				archive(bShowGizmos, "bShowGizmos", "Show Gizmos");
				archive(gizmoLength, "gizmoLength", "Gizmo Length");
			}

			// ~Schematyc2::IComponentPreviewProperties

			bool  bShowGizmos;
			float gizmoLength;
		};
	}

	const Schematyc2::SGUID CEntityLightComponent::s_componentGUID = "ed123a98-462f-49a0-8d1b-362d6449d81a";

	CEntityLightComponent::SProperties::SProperties()
		: bOn(true)
		, color(Col_White)
		, diffuseMultiplier(1.0f)
		, specularMultiplier(1.0f)
		, hdrDynamicMultiplier(1.0f)
		, radius(10.0f)
		, frustumAngle(45.0f)
		, attenuationBulbSize(0.05f)
	{}

	void CEntityLightComponent::SProperties::Serialize(Serialization::IArchive& archive)
	{
		archive(bOn, "on", "On");
		archive.doc("Initial on/off state");

		archive(color, "color", "Color");
		archive.doc("Color");

		archive(Serialization::Range(diffuseMultiplier, 0.01f, 10.0f), "diffuseMultiplier", "Diffuse Multiplier");
		archive.doc("Diffuse Multiplier");

		archive(Serialization::Range(specularMultiplier, 0.01f, 10.0f), "specularMultiplier", "Specular Multiplier");
		archive.doc("Specular Multiplier");

		archive(Serialization::Range(hdrDynamicMultiplier, 0.01f, 10.0f), "hdrDynamicMultiplier", "HDR Dynamic Multiplier");
		archive.doc("HDR Dynamic Multiplier");

		archive(Serialization::Range(radius, 0.01f, 50.0f), "radius", "Radius");
		archive.doc("Radius");

		archive(Serialization::Range(frustumAngle, 0.01f, 90.0f), "frustumAngle", "Frustum Angle");
		archive.doc("Frustum Angle");

		archive(Serialization::Range(attenuationBulbSize, 0.01f, 5.0f), "attenuationBulbSize", "Attenuation Bulb Size");
		archive.doc("Attenuation Bulb Size");

		archive(TransformDecorator<ETransformComponent::PositionAndRotation>(transform), "transform", "Transform");
		archive.doc("Transform relative to parent/entity");
	}

	Schematyc2::SEnvFunctionResult CEntityLightComponent::SProperties::SetTransform(const Schematyc2::SEnvFunctionContext& context, const STransform& _transform)
	{
		transform = _transform;
		return Schematyc2::SEnvFunctionResult();
	}

	CEntityLightComponent::CEntityLightComponent()
		: m_pProperties(nullptr)
		, m_bOn(false)
	{}

	bool CEntityLightComponent::Init(const Schematyc2::SComponentParams& params)
	{
		if(!CEntityTransformComponentBase::Init(params))
		{
			return false;
		}
		m_pProperties = params.properties.ToPtr<SProperties>();
		return true;
	}

	void CEntityLightComponent::Run(Schematyc2::ESimulationMode simulationMode)
	{
		if(m_pProperties->bOn)
		{
			SwitchOn();
		}
	}
		
	Schematyc2::IComponentPreviewPropertiesPtr CEntityLightComponent::GetPreviewProperties() const
	{
		return std::make_shared<EntityLightComponentUtils::SPreviewProperties>();
	}

	void CEntityLightComponent::Preview(const SRendParams& params, const SRenderingPassInfo& passInfo, const Schematyc2::IComponentPreviewProperties& previewProperties) const
	{
		const EntityLightComponentUtils::SPreviewProperties& previewPropertiesImpl = static_cast<const EntityLightComponentUtils::SPreviewProperties&>(previewProperties);
		if(previewPropertiesImpl.bShowGizmos)
		{
			if(CEntityTransformComponentBase::SlotIsValid())
			{
				IRenderAuxGeom& renderAuxGeom = *gEnv->pRenderer->GetIRenderAuxGeom();
				const Matrix34  worldTM = CEntityTransformComponentBase::GetWorldTM();
				const float     lineThickness = 4.0f;

				renderAuxGeom.DrawLine(worldTM.GetTranslation(), ColorB(255, 0, 0, 255), worldTM.GetTranslation() + (worldTM.GetColumn0().GetNormalized() * previewPropertiesImpl.gizmoLength), ColorB(255, 0, 0, 255), lineThickness);
				renderAuxGeom.DrawLine(worldTM.GetTranslation(), ColorB(0, 255, 0, 255), worldTM.GetTranslation() + (worldTM.GetColumn1().GetNormalized() * previewPropertiesImpl.gizmoLength), ColorB(0, 255, 0, 255), lineThickness);
				renderAuxGeom.DrawLine(worldTM.GetTranslation(), ColorB(0, 0, 255, 255), worldTM.GetTranslation() + (worldTM.GetColumn2().GetNormalized() * previewPropertiesImpl.gizmoLength), ColorB(0, 0, 255, 255), lineThickness);
			}
		}
	}

	void CEntityLightComponent::Shutdown()
	{
		CEntityTransformComponentBase::Shutdown();
	}

	Schematyc2::INetworkSpawnParamsPtr CEntityLightComponent::GetNetworkSpawnParams() const
	{
		return Schematyc2::INetworkSpawnParamsPtr();
	}

	void CEntityLightComponent::Register()
	{
		Schematyc2::IEnvRegistry& envRegistry = gEnv->pSchematyc2->GetEnvRegistry();

		{
			Schematyc2::IComponentFactoryPtr pComponentFactory = SCHEMATYC2_MAKE_COMPONENT_FACTORY_SHARED(CEntityLightComponent, SProperties, s_componentGUID);
			pComponentFactory->SetName("Light");
			pComponentFactory->SetNamespace("Base");
			pComponentFactory->SetAuthor("Crytek");
			pComponentFactory->SetDescription("Entity light component");
			pComponentFactory->SetFlags(Schematyc2::EComponentFlags::CreatePropertyGraph);
			pComponentFactory->SetAttachmentType(Schematyc2::EComponentSocket::Parent, g_tranformAttachmentGUID);
			pComponentFactory->SetAttachmentType(Schematyc2::EComponentSocket::Child, g_tranformAttachmentGUID);
			envRegistry.RegisterComponentFactory(pComponentFactory);
		}

		// Functions

		{
			Schematyc2::CEnvFunctionDescriptorPtr pFunctionDescriptor = Schematyc2::MakeEnvFunctionDescriptorShared(&CEntityLightComponent::SProperties::SetTransform);
			pFunctionDescriptor->SetGUID("729721c4-a09e-4903-a8a6-fa69388acfc6");
			pFunctionDescriptor->SetName("SetTransform");
			pFunctionDescriptor->BindInput(0, 0, "Transform", "Transform");
			envRegistry.RegisterFunction(pFunctionDescriptor);
		}
	}

	bool CEntityLightComponent::Switch(bool bOn)
	{
		const bool bPrevOn = m_bOn;
		if(bOn != bPrevOn)
		{
			if(bOn)
			{
				SwitchOn();
			}
			else
			{
				SwitchOff();
			}
		}
		return bPrevOn;
	}

	void CEntityLightComponent::SwitchOn()
	{
		ColorF color = m_pProperties->color;
		color.r *= m_pProperties->diffuseMultiplier;
		color.g *= m_pProperties->diffuseMultiplier;
		color.b *= m_pProperties->diffuseMultiplier;

		SRenderLight light;
		light.m_Flags                = 0; //TODO [andriy 25.05.16]: double check that. was ' = DLF_ALLOW_LPV' (see CL 1380778)
		light.SetLightColor(color);
		light.SetSpecularMult(m_pProperties->specularMultiplier);
		light.SetRadius(m_pProperties->radius, m_pProperties->attenuationBulbSize);
		light.m_fHDRDynamic          = m_pProperties->hdrDynamicMultiplier;
		light.m_fLightFrustumAngle   = m_pProperties->frustumAngle;

		const int slot = CEntityComponentBase::GetEntity().LoadLight(g_emptySlot, &light);
		CEntityTransformComponentBase::SetSlot(slot, m_pProperties->transform);

		m_bOn = true;
	}

	void CEntityLightComponent::SwitchOff()
	{
		CEntityTransformComponentBase::FreeSlot();

		m_bOn = false;
	}
}

SCHEMATYC2_GAME_ENV_AUTO_REGISTER(&SchematycBaseEnv::CEntityLightComponent::Register)
