#pragma once

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryPhysics/IPhysics.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		// Interface for physicalizing as static or rigid
		class CClothComponent
			: public IEntityComponent
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

			// IEntityComponent
			virtual void Initialize() final;

			virtual void ProcessEvent(const SEntityEvent& event) final;
			virtual Cry::Entity::EventFlags GetEventMask() const final;

			virtual NetworkAspectType GetNetSerializeAspectMask() const override { return eEA_GameClientDynamic; };
			// ~IEntityComponent

		public:

			struct SSimulationParameters
			{
				inline bool operator==(const SSimulationParameters &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				static void ReflectType(Schematyc::CTypeDesc<SSimulationParameters>& desc)
				{
					desc.SetGUID("{7D8FCF81-67FE-4A53-B6F9-0F765EF6F04D}"_cry_guid);
					desc.SetLabel("Simulation Parameters");
					desc.AddMember(&CClothComponent::SSimulationParameters::maxTimeStep, 'maxt', "MaxTimeStep", "Maximum Time Step", "The largest time step the entity can make before splitting. Smaller time steps increase stability (can be required for long and thin objects, for instance), but are more expensive.", 0.02f);
					desc.AddMember(&CClothComponent::SSimulationParameters::sleepSpeed, 'slps', "SleepSpeed", "Sleep Speed", "If the object's kinetic energy falls below some limit over several frames, the object is considered sleeping. This limit is proportional to the square of the sleep speed value.", 0.01f);
					desc.AddMember(&CClothComponent::SSimulationParameters::damping, 'damp', "Damping", "Damping", "Strength of damping.", 0.3f);
					desc.AddMember(&CClothComponent::SSimulationParameters::hardness, 'hard', "Hardness", "Hardness", "Strength of connections between cloth's vertices", 20.0f);
					desc.AddMember(&CClothComponent::SSimulationParameters::maxIters, 'mxit', "MaxIters", "Max Iters", "Amount of iterations to solve cloth's constraints (affects the cloth solver speed)", 20);
					desc.AddMember(&CClothComponent::SSimulationParameters::accuracy, 'accu', "Accuracy", "Accuracy", "Accuracy (allowed velocity error per frame) of the cloth solver", 0.05f);
					desc.AddMember(&CClothComponent::SSimulationParameters::maxStretch, 'stch', "MaxStretch", "Max Stretch", "Maximum allowed streching before positional enforcement is activated, in fractions of 1 (i.e. 0.05=5%)\
Smaller values to be used to remove stretching even with low Max Iters. It's a cheaper, but less physically accurate way.", 0.3f);
				}

				float maxTimeStep = 0.02f;
				float sleepSpeed = 0.01f;
				float damping = 0.3f;
				float hardness = 20.0f;
				int   maxIters = 20;
				float accuracy = 0.05f;
				float maxStretch = 0.2f;
			};

			static void ReflectType(Schematyc::CTypeDesc<CClothComponent>& desc)
			{
				desc.SetGUID("{A376C5D6-CE2D-4A6E-8AF0-C7AEE726CB2E}"_cry_guid);
				desc.SetEditorCategory("Physics");
				desc.SetLabel("Cloth");
				desc.SetDescription("Physicalizes mesh as cloth/soft object");
				desc.SetIcon("icons:ObjectTypes/object.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::Singleton });

				desc.AddComponentInteraction(SEntityComponentRequirements::EType::Incompatibility, "{98183F31-A685-43CD-92A9-815274F0A81C}"_cry_guid); // incompatible with Character Controller 
				desc.AddComponentInteraction(SEntityComponentRequirements::EType::Incompatibility, "{47EBC019-41CB-415E-AB57-2A3A99B918C2}}"_cry_guid);	// incompatible with Vehicle Physics
				desc.AddComponentInteraction(SEntityComponentRequirements::EType::Incompatibility, "{EC7F145B-D48F-4863-B9C2-3D3E2C8DCC61}"_cry_guid); // incompatible with Area
				desc.AddComponentInteraction(SEntityComponentRequirements::EType::Incompatibility, "{912C6CE8-56F7-4FFA-9134-F98D4E307BD6}"_cry_guid); // incompatible with RigidBody

				desc.AddMember(&CClothComponent::m_filePath, 'file', "FilePath", "File", "Determines the CGF to load", "%ENGINE%/EngineAssets/Objects/primitive_sphere.cgf");
				desc.AddMember(&CClothComponent::m_materialPath, 'mat', "Material", "Material", "Specifies the override material for the selected object", "");

				desc.AddMember(&CClothComponent::m_mass, 'mass', "Mass", "Mass", "Mass of the cloth object (only useful lighter than air closed objects)", 5.0f);
				desc.AddMember(&CClothComponent::m_density, 'dens', "Density", "Density", "Density of cloth's vertices (only affects floating in water). Unlike rigid bodies, cloth can have independent mass and density settings", 200.0f);
				desc.AddMember(&CClothComponent::m_thickness, 'thik', "Thickness", "Thickness", "Thickness, used for collision detection. Every cloth vertex is surrounded with a colliding sphere with this radius", 0.05f);
				desc.AddMember(&CClothComponent::m_massDecay, 'mdcy', "MassDecay", "Mass Decay", "Sets by how much vertex mass decreases at cloth's free ends compared to attached ones.\
Higher values improve solver and stretch enforcement quality, but it should be 0 for unattached cloth (such as pressurized)", 0.5f);
				desc.AddMember(&CClothComponent::m_friction, 'fric', "Friction", "Friction", "Friction at collision points", false);
				desc.AddMember(&CClothComponent::m_airResistance, 'rair', "AirResist", "Air Resistance", "Sets how strongly cloth is affected by the wind", 1.0f);
				desc.AddMember(&CClothComponent::m_wind, 'wind', "Wind", "Wind", "Extra wind added to this cloth, on top of the global/physical areas", Vec3(ZERO));
				desc.AddMember(&CClothComponent::m_windVariance, 'wvar', "WindVariance", "Wind Variance", "Adds random variance to the wind to make the cloth more flappy", 0.2f);
				desc.AddMember(&CClothComponent::m_pressure, 'pres', "Pressure", "Pressure", "Internal pressure for closed cloth", 0.0f);
				desc.AddMember(&CClothComponent::m_densityInside, 'dins', "DensInside", "Density Inside", "Density of the medium inside the cloth. Can be used to adjust buoyancy,\
 by having cloth objects filled with lighter-than-air mediums for instance (i.e. <1 density). -1 to assume the same density as outside", -1);
				desc.AddMember(&CClothComponent::m_impulseScale, 'imps', "ImpulseScale", "Impulse Scale", "Scales hit impulses applied to the cloth (to avoid excessive reaction)", 0.02f);
				desc.AddMember(&CClothComponent::m_explosionScale, 'exps', "ExplScale", "Explosion Scale", "Scales explosion impulses applied to the cloth (to avoid excessive reaction)", 0.003f);
				desc.AddMember(&CClothComponent::m_simulationParameters, 'simp', "Simulation", "Simulation Parameters", "Simulation parameters", SSimulationParameters());
				desc.AddMember(&CClothComponent::m_collTerrain, 'cter', "CollTerr", "Terrain Collisions", "Enables collisions with the main terrain", false);
				desc.AddMember(&CClothComponent::m_collStatic, 'csta', "CollStat", "Static Collisions", "Enables collisions with static objects", true);
				desc.AddMember(&CClothComponent::m_collDynamic, 'cdyn', "CollDyn", "Dynamic Collisions", "Enables collisions with 'normal' physical object, i.e. rigid bodies, ragdolls, vehicles", true);
				desc.AddMember(&CClothComponent::m_collPlayers, 'cply', "CollPlayer", "Player Collisions", "Enables collisions with legacy-style players (PE_LIVING)", true);
			}

			virtual void AttachToEntity(Schematyc::ExplicitEntityId entAttachTo, int ipart = -1);

			CClothComponent() {}
			virtual ~CClothComponent();

		protected:
			void Physicalize();
			void Reattach();

			Schematyc::GeomFileName m_filePath;
			Schematyc::MaterialFileName m_materialPath;

			_smart_ptr<IStatObj> m_pStatObj;
	
			float m_mass = 5;
			float m_density = 200;
			float m_thickness = 0.06f;
			Schematyc::Range<0, 1> m_massDecay = 0.5f;
			float m_friction = 0;
			float m_airResistance = 1;
			Vec3  m_wind = Vec3(ZERO);
			float m_windVariance = 0.2f;
			float m_pressure = 0;
			float m_densityInside = -1;
			float m_impulseScale = 0.02f;
			float m_explosionScale = 0.003f;
		
			bool m_collTerrain = false;
			bool m_collStatic = true;
			bool m_collDynamic = true;
			bool m_collPlayers = true;

			SSimulationParameters m_simulationParameters;

			EntityId	m_entAttachTo = INVALID_ENTITYID;
			int m_attachPart = -1;
		};
	}
}