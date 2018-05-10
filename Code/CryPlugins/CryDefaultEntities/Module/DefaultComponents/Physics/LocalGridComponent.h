#pragma once

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Utils/IString.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

namespace Cry
{
	namespace DefaultComponents
	{
		#define DECL_POWERS P(0) P(1) P(2) P(3) P(4) P(5) P(6) P(7) P(8) P(9) P(10)
		#define P(i) ePow_##i = 1<<i,
		enum EPowerOf2 { DECL_POWERS };
		#undef P

		class CLocalGridComponent	: public IEntityComponent
		#ifndef RELEASE
			, public IEntityComponentPreviewer
		#endif
		{
		protected:
			// IEntityComponent
			virtual void Initialize() final {}
			virtual void ProcessEvent(const SEntityEvent& event) final;
			virtual uint64 GetEventMask() const final	
			{	
				return BIT64(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED) | BIT64(ENTITY_EVENT_START_GAME) | BIT64(ENTITY_EVENT_RESET) | BIT64(ENTITY_EVENT_ATTACH) | BIT64(ENTITY_EVENT_DETACH); 
			}
			// ~IEntityComponent

		public:
			CLocalGridComponent() {}
			virtual ~CLocalGridComponent();

			static void Register(Schematyc::CEnvRegistrationScope& componentScope) {}
			static void ReflectType(Schematyc::CTypeDesc<CLocalGridComponent>& desc);

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
		};
	}
}