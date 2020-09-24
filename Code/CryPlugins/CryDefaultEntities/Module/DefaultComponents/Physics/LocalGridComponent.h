#pragma once

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Utils/IString.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

template<int i> constexpr int const_ilog10() { return 1 + const_ilog10<i/10>(); }
template<> constexpr int const_ilog10<0>() { return 0; }

namespace Cry
{
	namespace DefaultComponents
	{
		#define DECL_POWERS P(0) P(1) P(2) P(3) P(4) P(5) P(6) P(7) P(8) P(9) P(10)
		#define P(i) ePow_##i = 1<<i,
		enum EPowerOf2 { DECL_POWERS };
		#undef P

		static void ReflectType(Schematyc::CTypeDesc<EPowerOf2>& desc)
		{
			desc.SetGUID("{D6DAF5E2-55C0-4e5f-BACA-846A6DBC673E}"_cry_guid);
			desc.SetLabel("Power of 2");
			#define P(i) \
				static char label##i[const_ilog10<(1<<i)>()+1]; ltoa(1<<i, label##i, 10); \
				desc.AddConstant(EPowerOf2::ePow_##i, #i, label##i);
			DECL_POWERS
			#undef P
		}


		class CLocalGridComponent	: public IEntityComponent
		#ifndef RELEASE
			, public IEntityComponentPreviewer
		#endif
		{
		protected:
			// IEntityComponent
			virtual void Initialize() final {}
			virtual void ProcessEvent(const SEntityEvent& event) final;
			virtual Cry::Entity::EventFlags GetEventMask() const final	
			{	
				return ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED | ENTITY_EVENT_START_GAME | ENTITY_EVENT_RESET | ENTITY_EVENT_ATTACH | ENTITY_EVENT_DETACH; 
			}
			// ~IEntityComponent

		public:
			CLocalGridComponent() {}
			virtual ~CLocalGridComponent();

			static void Register(Schematyc::CEnvRegistrationScope& componentScope) {}
			static void ReflectType(Schematyc::CTypeDesc<CLocalGridComponent>& desc)
			{
				desc.SetGUID("{594ECA21-953A-48bd-BC4F-4A690A124D6A}"_cry_guid);
				desc.SetEditorCategory("Physics");
				desc.SetLabel("Local Grid");
				desc.SetDescription("Creates a local simulation grid that allows entities to be attached to larger hosts but also be physically simulated within its local environment");
				desc.SetIcon("icons:ObjectTypes/object.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

				desc.AddMember(&CLocalGridComponent::m_sizex, 'sizx', "SizeX", "Number Of Cells X", "Number of cells in X dimension", ePow_3);
				desc.AddMember(&CLocalGridComponent::m_sizey, 'sizy', "SizeY", "Number Of Cells Y", "Number of cells in Y dimension", ePow_3);
				desc.AddMember(&CLocalGridComponent::m_cellSize, 'clsz', "CellSize", "Cell Size", "Grid Cell Dimensions", Vec2(1,1));
				desc.AddMember(&CLocalGridComponent::m_height, 'hght', "Height", "Height", "Height of the grid area", 2.0f);
				desc.AddMember(&CLocalGridComponent::m_accThresh, 'acct', "AccThresh", "Acceleration Threshold", "Minimal host acceleration that is applied to objects inside the grid", 3.0f);
			}


			void Reset();
			void Physicalize();

			#ifndef RELEASE
			virtual IEntityComponentPreviewer* GetPreviewer() final { return this; }
			// IEntityComponentPreviewer
			virtual void SerializeProperties(Serialization::IArchive& archive) final {}
			virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const final;
			#endif

			EPowerOf2 m_sizex = ePow_3;
			EPowerOf2 m_sizey = ePow_3;
			Vec2  m_cellSize  = Vec2(1, 1);
			float m_height    = 2.0f;
			float m_accThresh = 3.0f;

		protected:
			_smart_ptr<IPhysicalEntity> m_pGrid;
			std::vector<_smart_ptr<IPhysicalEntity>> m_physAreas;
		};
	}
}