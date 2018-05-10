#pragma once

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

#include <CryParticleSystem/IParticlesPfx2.h>
#include <CrySerialization/SmartPtr.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CParticleComponent
			: public IEntityComponent
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

			// IEntityComponent
			virtual void Initialize() final;

			virtual void ProcessEvent(const SEntityEvent& event) final;
			virtual uint64 GetEventMask() const final;
			// ~IEntityComponent

		public:
			virtual ~CParticleComponent() {}

			static void ReflectType(Schematyc::CTypeDesc<CParticleComponent>& desc)
			{
				desc.SetGUID("{7925BC64-521F-4A2A-9068-C9C9A1D3499E}"_cry_guid);
				desc.SetEditorCategory("Effects");
				desc.SetLabel("Particle Emitter");
				desc.SetDescription("Emits a particle effect");
				desc.SetIcon("icons:schematyc/entity_particle_emitter_component.png");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly });

				desc.AddMember(&CParticleComponent::m_bEnabled, 'actv', "Enabled", "Enabled", "Whether or not the particle should emit by default", true);
				desc.AddMember(&CParticleComponent::m_effectName, 'file', "FilePath", "Effect", "Determines the particle effect to load", "");

				desc.AddMember(&CParticleComponent::m_attributes, 'attr', "Attributes", "Emitter Attributes", nullptr, CParticleComponent::SAttributes());
				desc.AddMember(&CParticleComponent::m_features, 'feat', "Features", "Emitter Features", nullptr, CParticleComponent::SFeatures());
				desc.AddMember(&CParticleComponent::m_spawnParams, 'spaw', "SpawnParams", "Spawn Parameters", nullptr, CParticleComponent::SSpawnParameters());
			}

			virtual void LoadEffect(bool bActivate)
			{
				if (IParticleEffect* pEffect = gEnv->pParticleManager->FindEffect(m_effectName.value, "CParticleComponent"))
				{
					IParticleEmitter* pEmitter = m_pEntity->GetParticleEmitter(GetEntitySlotId());
					if (pEmitter && pEmitter->GetEffect() != pEffect)
					{
						FreeEntitySlot();
						pEmitter = nullptr;
					}

					if (!pEmitter && bActivate)
					{
						m_pEntity->LoadParticleEmitter(GetOrMakeEntitySlotId(), pEffect, &m_spawnParams);
						pEmitter = m_pEntity->GetParticleEmitter(GetEntitySlotId());
						if (pEmitter)
							pEmitter->GetAttributes().TransferInto(m_attributes.get());
					}
					else if (pEmitter)
					{
						pEmitter->SetSpawnParams(m_spawnParams);
						if (!bActivate && m_spawnParams.bPrime)
							pEmitter->Kill();
						else
							pEmitter->Activate(bActivate);
					}
					if (pEmitter)
					{
						pEmitter->GetAttributes().Reset(m_attributes.get());
						pEmitter->SetEmitterFeatures(m_features);
					}
					m_bCurrentlyActive = bActivate;
				}
				else
				{
					FreeEntitySlot();
				}
			}

			virtual void Activate(bool bActive)
			{
				if (m_bCurrentlyActive == bActive)
				{
					return;
				}

				if (IParticleEmitter* pParticleEmitter = GetEntity()->GetParticleEmitter(GetEntitySlotId()))
				{
					pParticleEmitter->Activate(bActive);
					m_bCurrentlyActive = bActive;
				}
				else if (bActive)
				{
					if (IParticleEffect* pEffect = gEnv->pParticleManager->FindEffect(m_effectName.value, "CParticleComponent"))
					{
						m_pEntity->LoadParticleEmitter(GetOrMakeEntitySlotId(), pEffect, &m_spawnParams);
						m_bCurrentlyActive = true;
					}
				}
				else
				{
					m_bCurrentlyActive = false;
				}
			}

			bool IsActive() const { return m_bCurrentlyActive; }
			
			struct SAttributes: TParticleAttributesPtr
			{
				SAttributes()
					: TParticleAttributesPtr(pfx2::GetIParticleSystem()->CreateParticleAttributes())
				{}

				inline bool operator==(const SAttributes &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }  // Todo: Deep compare
				
				static void ReflectType(Schematyc::CTypeDesc<SAttributes>& desc)
				{
					desc.SetGUID("{24AE6687-F855-4736-9C71-3419083BAECB}"_cry_guid);
					desc.SetLabel("Emitter Attributes");
				}
				void Serialize(Serialization::IArchive& archive)
				{
					return get()->Serialize(archive);
				}
			};

			struct SFeatures: pfx2::TParticleFeatures
			{
				inline bool operator==(const SFeatures &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); } // Todo: Deep compare
				static void ReflectType(Schematyc::CTypeDesc<SFeatures>& desc)
				{
					desc.SetGUID("{CAB0D3B2-D3F5-4F47-AA50-32BCED2C42A7}"_cry_guid);
					desc.SetLabel("Emitter Features");
				}
			};

			struct SSpawnParameters: SpawnParams
			{
				inline bool operator==(const SSpawnParameters &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				static void ReflectType(Schematyc::CTypeDesc<SSpawnParameters>& desc)
				{
					desc.SetGUID("{9E4544F0-3E5A-4479-B618-E9CB46905149}"_cry_guid);
					desc.SetLabel("Emitter Spawn Parameters");
				}
			};

 			virtual void SetEffectName(const char* szPath);
 			const char* GetEffectName() const { return m_effectName.value.c_str(); }
			
		protected:
			bool m_bEnabled = true;
			bool m_bCurrentlyActive = false;
			
			SAttributes m_attributes;
			SFeatures m_features;
			SSpawnParameters m_spawnParams;

			Schematyc::ParticleEffectName m_effectName;
		};
	}
}