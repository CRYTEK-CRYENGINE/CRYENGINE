#pragma once

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CAreaComponent
			: public IEntityComponent
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
			enum class EType
			{
				Box,
				Sphere,
				Cylinder,
				Capsule
			};

			struct SShapeParameters
			{
				SShapeParameters(CAreaComponent* pAreaComponent) : pComponent(pAreaComponent) {}
				inline bool operator==(const SShapeParameters &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				void Serialize(Serialization::IArchive& archive);

				static void ReflectType(Schematyc::CTypeDesc<SShapeParameters>& desc)
				{
					desc.SetGUID("{8A493322-1DCD-442B-BA5A-9C41133AE911}"_cry_guid);
					desc.SetLabel("Shape Parameters");
				}

				CAreaComponent* pComponent;
				Vec3 size = Vec3(1, 1, 1);
			};

			struct SBuoyancyParameters
			{
				enum class EType
				{
					None = -1,
					Air = pe_params_buoyancy::eAir,
					Water = pe_params_buoyancy::eWater
				};

				inline bool operator==(const SBuoyancyParameters &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				static void ReflectType(Schematyc::CTypeDesc<SBuoyancyParameters>& desc)
				{
					desc.SetGUID("{A7270990-667F-4D27-B277-CF7ACFF3343C}"_cry_guid);
					desc.SetLabel("Buoyancy Parameters");
					desc.AddMember(&CAreaComponent::SBuoyancyParameters::medium, 'medi', "Medium", "Type", "Type of buoyancy to apply", CAreaComponent::SBuoyancyParameters::EType::Air);
					desc.AddMember(&CAreaComponent::SBuoyancyParameters::flow, 'flow', "Flow", "Flow", "The flow of the air or water, in meters per second", Vec3(0, 5, 0));
					desc.AddMember(&CAreaComponent::SBuoyancyParameters::density, 'dens', "Density", "Density", "Density of the fluid", 1.f);
					desc.AddMember(&CAreaComponent::SBuoyancyParameters::resistance, 'rest', "Resistance", "Resistance", "Resistance of the fluid", 1.f);
				}

				EType medium = EType::Air;
				Vec3 flow = Vec3(0, 5, 0);
				Schematyc::PositiveFloat density = 1.f;
				Schematyc::PositiveFloat resistance = 1.f;
			};

			static void ReflectType(Schematyc::CTypeDesc<CAreaComponent>& desc)
			{
				desc.SetGUID("{EC7F145B-D48F-4863-B9C2-3D3E2C8DCC61}"_cry_guid);
				desc.SetEditorCategory("Physics");
				desc.SetLabel("Area");
				desc.SetDescription("Creates a physical representation of the entity, ");
				desc.SetIcon("icons:ObjectTypes/object.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::Singleton });

				// Mark the Character Controller component as incompatible
				desc.AddComponentInteraction(SEntityComponentRequirements::EType::Incompatibility, "{98183F31-A685-43CD-92A9-815274F0A81C}"_cry_guid);
				// Mark the Vehicle Physics component as incompatible
				desc.AddComponentInteraction(SEntityComponentRequirements::EType::Incompatibility, "{47EBC019-41CB-415E-AB57-2A3A99B918C2}}"_cry_guid);
				// Mark the RigidBody component as incompatible
				desc.AddComponentInteraction(SEntityComponentRequirements::EType::Incompatibility, "{912C6CE8-56F7-4FFA-9134-F98D4E307BD6}"_cry_guid);

				desc.AddMember(&CAreaComponent::m_type, 'type', "Type", "Type", "Whether to apply custom gravity in this area", CAreaComponent::EType::Box);
				desc.AddMember(&CAreaComponent::m_shapeParameters, 'para', "Shape", "Shape Properties", "Dimensions of the area", SShapeParameters(nullptr));

				desc.AddMember(&CAreaComponent::m_bCustomGravity, 'apgr', "ApplyGravity", "Custom Gravity", "Whether to apply custom gravity in this area", false);
				desc.AddMember(&CAreaComponent::m_gravity, 'grav', "Gravity", "Gravity", "The gravity value to use", ZERO);
				desc.AddMember(&CAreaComponent::m_buoyancyParameters, 'buoy', "Buoyancy", "Buoyancy Parameters", "Fluid behavior of the area", SBuoyancyParameters());
			}

			CAreaComponent();
			virtual ~CAreaComponent();

			virtual EType GetType() const { return m_type; }

		protected:
			void Physicalize();

		protected:
			EType m_type;
			SShapeParameters m_shapeParameters;

			bool m_bCustomGravity = false;
			Vec3 m_gravity = ZERO;

			SBuoyancyParameters m_buoyancyParameters;
		};

		static void ReflectType(Schematyc::CTypeDesc<CAreaComponent::EType>& desc)
		{
			desc.SetGUID("{C42A564A-0024-4030-9381-1BC9004EA0E1}"_cry_guid);
			desc.SetLabel("Area Shape");
			desc.SetDescription("Shape to use for an area");
			desc.SetDefaultValue(CAreaComponent::EType::Box);
			desc.AddConstant(CAreaComponent::EType::Box, "Box", "Box");
			desc.AddConstant(CAreaComponent::EType::Sphere, "Sphere", "Sphere");
			desc.AddConstant(CAreaComponent::EType::Cylinder, "Cylinder", "Cylinder");
			desc.AddConstant(CAreaComponent::EType::Capsule, "Capsule", "Capsule");
		}

		static void ReflectType(Schematyc::CTypeDesc<CAreaComponent::SBuoyancyParameters::EType>& desc)
		{
			desc.SetGUID("{ABBCDB1B-75CD-4B9B-B4F7-D6C43DF38AAE}"_cry_guid);
			desc.SetLabel("Buoyancy Type");
			desc.SetDescription("Type of buoyancy to apply");
			desc.SetDefaultValue(CAreaComponent::SBuoyancyParameters::EType::Air);
			desc.AddConstant(CAreaComponent::SBuoyancyParameters::EType::None, "None", "Disabled");
			desc.AddConstant(CAreaComponent::SBuoyancyParameters::EType::Air, "Air", "Air");
			desc.AddConstant(CAreaComponent::SBuoyancyParameters::EType::Water, "Water", "Water");
		}
	}
}