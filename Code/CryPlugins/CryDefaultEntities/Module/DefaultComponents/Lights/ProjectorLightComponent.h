#pragma once
#include "ILightComponent.h"
#include <CryMath/Angle.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CProjectorLightComponent
			: public ILightComponent
#ifndef RELEASE
			, public IEntityComponentPreviewer
#endif
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

			// IEntityComponent
			virtual void Initialize() final;

			virtual void ProcessEvent(const SEntityEvent& event) final;
			virtual uint64 GetEventMask() const final;

#ifndef RELEASE
			virtual IEntityComponentPreviewer* GetPreviewer() final { return this; }
#endif
			// ~IEntityComponent

#ifndef RELEASE
			// IEntityComponentPreviewer
			virtual void SerializeProperties(Serialization::IArchive& archive) final {}

			virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const final;
			// ~IEntityComponentPreviewer
#endif

		public:
			CProjectorLightComponent() {}
			virtual ~CProjectorLightComponent() {}

			virtual void SetOptics(const char* szOpticsFullName) override
			{
				m_optics.m_lensFlareName = szOpticsFullName;

				Initialize(); 
			}

			static void ReflectType(Schematyc::CTypeDesc<CProjectorLightComponent>& desc)
			{
				desc.SetGUID("{07D0CAD1-8E79-4177-9ADD-A2464A009FA5}"_cry_guid);
				desc.SetEditorCategory("Lights");
				desc.SetLabel("Projector Light");
				desc.SetDescription("Emits light from its position in a general direction, constrained to a specified angle");
				desc.SetIcon("icons:ObjectTypes/light.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly, IEntityComponent::EFlags::NoCreationOffset });

				desc.AddMember(&CProjectorLightComponent::m_bActive, 'actv', "Active", "Active", "Determines whether the light is enabled", true);
				desc.AddMember(&CProjectorLightComponent::m_radius, 'radi', "Radius", "Range", "Determines whether the range of the light", 10.f);
				desc.AddMember(&CProjectorLightComponent::m_angle, 'angl', "Angle", "Angle", "Maximum angle to emit light to, from the light's forward axis.", 45.0_degrees);

				desc.AddMember(&CProjectorLightComponent::m_projectorOptions, 'popt', "ProjectorOptions", "Projector Options", nullptr, CProjectorLightComponent::SProjectorOptions());

				desc.AddMember(&CProjectorLightComponent::m_optics, 'opti', "Optics", "Optics", "Specific Optic Options", SOptics());
				desc.AddMember(&CProjectorLightComponent::m_color, 'colo', "Color", "Color", "Color emission information", SColor());
				desc.AddMember(&CProjectorLightComponent::m_shadows, 'shad', "Shadows", "Shadows", "Shadow casting settings", SShadows());
				desc.AddMember(&CProjectorLightComponent::m_options, 'opt', "Options", "Options", "Specific Light Options", SOptions());
				desc.AddMember(&CProjectorLightComponent::m_animations, 'anim', "Animations", "Animations", "Light style / animation properties", SAnimations());
			}

			struct SProjectorOptions
			{
				inline bool operator==(const SProjectorOptions &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				void SetTexturePath(const char* szPath);
				const char* GetTexturePath() const { return m_texturePath.value.c_str(); }
				void SetMaterialPath(const char* szPath);
				const char* GetMaterialPath() const { return m_materialPath.value.c_str(); }
				bool HasMaterialPath() const { return m_materialPath.value.size() > 0; }

				static void ReflectType(Schematyc::CTypeDesc<SProjectorOptions>& desc)
				{
					desc.SetGUID("{705FA6D1-CC00-45A5-8E51-78AF6CA14D2D}"_cry_guid);
					desc.AddMember(&CProjectorLightComponent::SProjectorOptions::m_nearPlane, 'near', "NearPlane", "Near Plane", nullptr, 0.f);
					desc.AddMember(&CProjectorLightComponent::SProjectorOptions::m_texturePath, 'tex', "Texture", "Projected Texture", "Path to a texture we want to emit", "");
					desc.AddMember(&CProjectorLightComponent::SProjectorOptions::m_materialPath, 'mat', "Material", "Material", "Path to a material we want to apply to the projector", "");
				}

				Schematyc::Range<0, 10000> m_nearPlane = 0.f;

				Schematyc::TextureFileName m_texturePath;
				Schematyc::MaterialFileName m_materialPath;
			};

			struct SFlare
			{
				inline bool operator==(const SFlare &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				void SetTexturePath(const char* szPath);
				const char* GetTexturePath() const { return m_texturePath.value.c_str(); }
				bool HasTexturePath() const { return m_texturePath.value.size() > 0; }

				static void ReflectType(Schematyc::CTypeDesc<SFlare>& desc)
				{
					desc.SetGUID("{DE4B89DD-B436-47EC-861F-4A5F3E831594}"_cry_guid);
					desc.AddMember(&CProjectorLightComponent::SFlare::m_angle, 'angl', "Angle", "Angle", nullptr, 360.0_degrees);
					desc.AddMember(&CProjectorLightComponent::SFlare::m_texturePath, 'tex', "Texture", "Flare Texture", "Path to the flare texture we want to use", "");
				}

				CryTransform::CClampedAngle<5, 360> m_angle = 360.0_degrees;

				Schematyc::TextureFileName m_texturePath;
			};

			virtual void Enable(bool bEnable) override;

			virtual void SetRadius(float radius) { m_radius = radius; }
			float GetRadius() const { return m_radius; }

			virtual void SetFieldOfView(CryTransform::CAngle angle) { m_angle = angle; }
			CryTransform::CAngle GetFieldOfView() { return m_angle; }

			virtual void SetNearPlane(float nearPlane) { m_projectorOptions.m_nearPlane = nearPlane; }
			float GetNearPlane() const { return m_projectorOptions.m_nearPlane; }

			virtual void SetFlareAngle(CryTransform::CAngle angle) { m_flare.m_angle = angle; }
			CryTransform:: CAngle GetFlareAngle() const { return m_flare.m_angle; }

		protected:
			bool m_bActive = true;
			Schematyc::Range<0, std::numeric_limits<int>::max()> m_radius = 10.f;
			CryTransform::CClampedAngle<0, 180> m_angle = 45.0_degrees;

			SProjectorOptions m_projectorOptions;

			SFlare m_flare;
		};
	}
}