#pragma once
#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CWaterRippleComponent
			: public IEntityComponent
#ifndef RELEASE
			, public IEntityComponentPreviewer
#endif
		{
		public:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

			// IEntityComponent
			virtual void Initialize() final;
			virtual void ProcessEvent(const SEntityEvent& event) final;
			virtual uint64 GetEventMask() const final;
			// ~IEntityComponent

			virtual ~CWaterRippleComponent() {}

			void Enable(const bool bEnable);
			bool IsEnabled() const;

			void SpawnRippleAtPosition(const Vec3& position, const float strength = 1.0f, const float scale = 1.0f) const;

#ifndef RELEASE
			// IEntityComponentPreviewer
			virtual IEntityComponentPreviewer* GetPreviewer() final { return this; }
			virtual void SerializeProperties(Serialization::IArchive& archive) final {}
			virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const final;
			// ~IEntityComponentPreviewer
#endif
			static void ReflectType(Schematyc::CTypeDesc<CWaterRippleComponent>& desc)
			{
				desc.SetGUID("{24F28FE1-826A-4237-988D-4DD3274C7C6A}"_cry_guid);
				desc.SetEditorCategory("Effects");
				desc.SetLabel("Water Ripple");
				desc.SetDescription("Generates water ripples on a water surface");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly });
				desc.AddMember(&CWaterRippleComponent::m_scale, 'sca', "Scale", "Scale", "Scale of the ripple generator", 1.0f);
				desc.AddMember(&CWaterRippleComponent::m_strength, 'str', "Strength", "Strength", "", 1.0f);
				desc.AddMember(&CWaterRippleComponent::m_frequency, 'fre', "Frequency", "Frequency", "", 1.0f);
				desc.AddMember(&CWaterRippleComponent::m_randFrequency, 'rfre', "RandomFrequency", "Random Frequency", "", 0.0f);
				desc.AddMember(&CWaterRippleComponent::m_randScale, 'rsca', "RandomScale", "Random Scale", "", 0.0f);
				desc.AddMember(&CWaterRippleComponent::m_randStrength, 'rstr', "RandomStrength", "Random Strength", "", 0.0f);
				desc.AddMember(&CWaterRippleComponent::m_bEnabled, 'ena', "Enable", "Enable", "", true);
				desc.AddMember(&CWaterRippleComponent::m_bAutoSpawn, 'ats', "AutoSpawn", "Auto Spawn", "", true);
				desc.AddMember(&CWaterRippleComponent::m_bSpawnOnMovement, 'spm', "SpawnMovement", "Spawn On Movement", "", true);
				desc.AddMember(&CWaterRippleComponent::m_randomOffset, 'rao', "RandomOffset", "Random Offset", "", Vec2(0.0f));
			}

		protected:
			bool TestLocation(const Vec3& testPosition);
			void ProcessHit(bool isMoving);
			void Reset();

		protected:
			Schematyc::Range<0, 100> m_scale = 1.0f;
			Schematyc::Range<0, 100> m_strength = 1.0f;
			Schematyc::Range<0, 100> m_frequency = 1.0f;
			Schematyc::Range<0, 100> m_randFrequency = 0.0f;
			Schematyc::Range<0, 100> m_randScale = 0.0f;
			Schematyc::Range<0, 100> m_randStrength = 0.0f;
			bool m_bEnabled = true;
			bool m_bAutoSpawn = true;
			bool m_bSpawnOnMovement = true;
			Vec2 m_randomOffset = Vec2(0.0f);

			float m_lastSpawnTime;
			bool m_currentLocationOk;
		};
	}
}