// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BaseEnv/Components/BaseEnv_EntityParticleEmitterComponent.h"

// TODO: Fix dependencies from IParticles.h - remove following includes block!
#include <Cry3DEngine/I3DEngine.h>
#include <CryParticleSystem/IParticles.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryRenderer/RenderElements/CREOcclusionQuery.h>

#include "BaseEnv/BaseEnv_AutoRegistrar.h"
#include "BaseEnv/Utils/BaseEnv_EntitySignals.h"

namespace SchematycBaseEnv
{
	namespace EntityParticleEmitterComponentUtils
	{
		struct SPreviewProperties : public Schematyc2::IComponentPreviewProperties
		{
			SPreviewProperties()
				: bShowGizmos(false)
				, gizmoLength(1.0f)
			{}

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

	const Schematyc2::SGUID CEntityParticleEmitterComponent::s_componentGUID = "bf9503cf-d25c-4923-a1cb-8658847aa9a6";

	CEntityParticleEmitterComponent::SParticleEmitterProperties::SParticleEmitterProperties()
		: scale(1.0f)
		, bDisable(false)
		, bPrime(false)
		, countScale(1.0f)
		, speedScale(1.0f)
		, timeScale(1.0f)
		, pulsePeriod(0.0f)
		, strength(-1.0f)
	{}

	void CEntityParticleEmitterComponent::SParticleEmitterProperties::Serialize(Serialization::IArchive& archive)
	{
		Schematyc2::SerializeWithNewName(archive, effectName, "effect_name", "effectName", "Effect");
		archive.doc("Effect");
		archive(bDisable, "disable", "Disable");
		archive.doc("Effect is initially disabled");
		archive(bPrime, "prime", "Prime");
		archive.doc("Advance emitter age to its equilibrium state");

		if(archive.openBlock("advanced", "Advanced"))
		{
			archive(scale, "scale", "Uniform Scale");
			archive.doc("Emitter uniform scale");
			Schematyc2::SerializeWithNewName(archive, countScale, "count_scale", "countScale", "Count Scale");
			archive.doc("Particle count multiplier");
			Schematyc2::SerializeWithNewName(archive, speedScale, "speed_scale", "speedScale", "Speed Scale");
			archive.doc("Particle emission speed multiplier");
			Schematyc2::SerializeWithNewName(archive, timeScale, "time_scale", "timeScale", "Time Scale");
			archive.doc("Emitter time multiplier");
			Schematyc2::SerializeWithNewName(archive, pulsePeriod, "pulse_period", "pulsePeriod", "Pulse Period");
			archive.doc("How often to restart emitter");
			archive(strength, "strength", "Strength");
			archive.doc("Strength curve parameter");
			archive.closeBlock();
		}
	}

	void CEntityParticleEmitterComponent::SProperties::Serialize(Serialization::IArchive& archive)
	{
		Serialization::SContext propertiesContext(archive, static_cast<const CEntityParticleEmitterComponent::SProperties*>(this));
		Schematyc2::SerializeWithNewName(archive, particleEmitter, "particle_emitter", "particleEmitter", "Particle Emitter");

		archive(transform, "transform", "Transform");
		archive.doc("Transform relative to parent/entity");
	}

	Schematyc2::SEnvFunctionResult CEntityParticleEmitterComponent::SProperties::SetTransform(const Schematyc2::SEnvFunctionContext& context, const STransform& _transform)
	{
		transform = _transform;
		return Schematyc2::SEnvFunctionResult();
	}

	bool CEntityParticleEmitterComponent::Init(const Schematyc2::SComponentParams& params)
	{
		if(!CEntityTransformComponentBase::Init(params))
		{
			return false;
		}
		m_properties = *params.properties.ToPtr<SProperties>();
		return true;
	}

	void CEntityParticleEmitterComponent::Run(Schematyc2::ESimulationMode simulationMode)
	{
		if(!m_properties.particleEmitter.effectName.value.empty())
		{
			if(!CEntityTransformComponentBase::SlotIsValid())
			{
				LoadParticleEmitter();
			}
		}
	}

	void CEntityParticleEmitterComponent::LoadParticleEmitter()
	{
		const SParticleEmitterProperties& particleEmitterProperties = m_properties.particleEmitter;
		IParticleEffect*                  pParticleEffect = gEnv->pParticleManager->FindEffect(particleEmitterProperties.effectName.value.c_str(), "SchematycBaseEnv::GameEntity::CEntityParticleEmitterComponent::LoadParticleEmitter");
		if(pParticleEffect)
		{
			SpawnParams spawnParams;

			spawnParams.fCountScale  = particleEmitterProperties.bDisable ? 0.0f : particleEmitterProperties.countScale;
			spawnParams.fSpeedScale  = particleEmitterProperties.speedScale;
			spawnParams.fTimeScale   = particleEmitterProperties.timeScale;
			spawnParams.fPulsePeriod = particleEmitterProperties.pulsePeriod;
			spawnParams.fStrength    = particleEmitterProperties.strength;
			spawnParams.bPrime       = particleEmitterProperties.bPrime;

			// Attach particle to base entity slot.
			int particleSlot = CEntityComponentBase::GetEntity().LoadParticleEmitter(g_emptySlot, pParticleEffect, &spawnParams);

			// Overwrite transform scale property - uniform scaling only.
			m_properties.transform.scale = Vec3(particleEmitterProperties.scale);

			CEntityTransformComponentBase::SetSlot(particleSlot, m_properties.transform);
		}
	}

	Schematyc2::IComponentPreviewPropertiesPtr CEntityParticleEmitterComponent::GetPreviewProperties() const
	{
  	return std::make_shared<EntityParticleEmitterComponentUtils::SPreviewProperties>();
	}

	void CEntityParticleEmitterComponent::Preview(const SRendParams& params, const SRenderingPassInfo& passInfo, const Schematyc2::IComponentPreviewProperties& previewProperties) const
	{
  	const EntityParticleEmitterComponentUtils::SPreviewProperties& previewPropertiesImpl = static_cast<const EntityParticleEmitterComponentUtils::SPreviewProperties&>(previewProperties);
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

	void CEntityParticleEmitterComponent::Shutdown()
	{
		if(CEntityTransformComponentBase::SlotIsValid())
		{
			CEntityComponentBase::GetEntity().GetParticleEmitter(CEntityTransformComponentBase::GetSlot())->Kill();
		}

		CEntityTransformComponentBase::Shutdown();
	}

	Schematyc2::INetworkSpawnParamsPtr CEntityParticleEmitterComponent::GetNetworkSpawnParams() const
	{
		return Schematyc2::INetworkSpawnParamsPtr();
	}

	Schematyc2::SEnvFunctionResult CEntityParticleEmitterComponent::SetVisible(const Schematyc2::SEnvFunctionContext& context, bool bVisible)
	{
		return Schematyc2::SEnvFunctionResult();
	}

	Schematyc2::SEnvFunctionResult CEntityParticleEmitterComponent::IsVisible(const Schematyc2::SEnvFunctionContext& context, bool &bVisible) const
	{
		return Schematyc2::SEnvFunctionResult();
	}

	void CEntityParticleEmitterComponent::Register()
	{
		Schematyc2::IEnvRegistry& envRegistry = gEnv->pSchematyc2->GetEnvRegistry();

		{
			Schematyc2::IComponentFactoryPtr pComponentFactory = SCHEMATYC2_MAKE_COMPONENT_FACTORY_SHARED(CEntityParticleEmitterComponent, SProperties, CEntityParticleEmitterComponent::s_componentGUID);
			pComponentFactory->SetName("ParticleEmitter");
			pComponentFactory->SetNamespace("Base");
			pComponentFactory->SetAuthor("Crytek");
			pComponentFactory->SetDescription("Particle emitter component");
			pComponentFactory->SetFlags(Schematyc2::EComponentFlags::CreatePropertyGraph);
			pComponentFactory->SetAttachmentType(Schematyc2::EComponentSocket::Parent, g_tranformAttachmentGUID);
			pComponentFactory->SetAttachmentType(Schematyc2::EComponentSocket::Child, g_tranformAttachmentGUID);
			envRegistry.RegisterComponentFactory(pComponentFactory);
		}

		// Functions

		{
			Schematyc2::CEnvFunctionDescriptorPtr pFunctionDescriptor = Schematyc2::MakeEnvFunctionDescriptorShared(&CEntityParticleEmitterComponent::SetVisible);
			pFunctionDescriptor->SetGUID("c9ac7f56-e6d2-4461-8871-54fb58d30e62");
			pFunctionDescriptor->SetName("SetVisible");
			pFunctionDescriptor->BindInput(0, 0, "Visible", "");
			envRegistry.RegisterFunction(pFunctionDescriptor);
		}

		{
			Schematyc2::CEnvFunctionDescriptorPtr pFunctionDescriptor = Schematyc2::MakeEnvFunctionDescriptorShared(&CEntityParticleEmitterComponent::IsVisible);
			pFunctionDescriptor->SetGUID("ba91ef70-02fc-4171-b8a0-637f16e3321d");
			pFunctionDescriptor->SetName("IsVisible");
			pFunctionDescriptor->BindOutput(0, 0, "Visible", "");
			envRegistry.RegisterFunction(pFunctionDescriptor);
		}

		{
			Schematyc2::CEnvFunctionDescriptorPtr pFunctionDescriptor = Schematyc2::MakeEnvFunctionDescriptorShared(&CEntityParticleEmitterComponent::SProperties::SetTransform);
			pFunctionDescriptor->SetGUID("97af940e-6a2c-4374-a43d-74d90ec385e2");
			pFunctionDescriptor->SetName("SetTransform");
			pFunctionDescriptor->BindInput(0, 0, "Transform", "");
			envRegistry.RegisterFunction(pFunctionDescriptor);
		}
	}
}

SCHEMATYC2_GAME_ENV_AUTO_REGISTER(&SchematycBaseEnv::CEntityParticleEmitterComponent::Register)
