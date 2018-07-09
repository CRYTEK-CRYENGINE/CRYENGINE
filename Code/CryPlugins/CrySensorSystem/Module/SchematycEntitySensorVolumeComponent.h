// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>

#include "SensorMap.h"
#include "SensorTagLibrary.h"

namespace Cry
{
	namespace SensorSystem
	{
		struct SSensorTagName : public string // #TODO : Move to separate header?
		{
			using string::string;
		};

		bool Serialize(Serialization::IArchive& archive, SSensorTagName& value, const char* szName, const char* szLabel);

		typedef Schematyc::CArray<SSensorTagName> SensorTagNames;

		class CSchematycEntitySensorVolumeComponent final : public IEntityComponent
#ifndef RELEASE
			, public IEntityComponentPreviewer
#endif
		{
		public:

			enum class EVolumeShape
			{
				Box,
				Sphere
			};

			struct SDimensions
			{
				inline bool operator==(const SDimensions& rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

				static void ReflectType(Schematyc::CTypeDesc<SDimensions>& desc);

				EVolumeShape shape = EVolumeShape::Box;
				Vec3         size = Vec3(1.0f);
				float        radius = 1.0f;
			};

			struct STags
			{
				static void ReflectType(Schematyc::CTypeDesc<STags>& desc);

				inline bool operator==(const STags& rhs) const
				{
					if (shape == rhs.shape &&
						attributeTags == rhs.attributeTags &&
						listenerTags == rhs.listenerTags
						)
						return true;
					return false;
				}

				SensorTagNames attributeTags;
				SensorTagNames listenerTags;
				EVolumeShape   shape = EVolumeShape::Box;
			};

		private:

			struct SEnteringSignal
			{
				SEnteringSignal();
				SEnteringSignal(EntityId _entityId);

				static void ReflectType(Schematyc::CTypeDesc<SEnteringSignal>& desc);

				Schematyc::ExplicitEntityId entityId;
			};

			struct SLeavingSignal
			{
				SLeavingSignal();
				SLeavingSignal(EntityId _entityId);

				static void ReflectType(Schematyc::CTypeDesc<SLeavingSignal>& desc);

				Schematyc::ExplicitEntityId entityId;
			};

			struct SEntityNotHidden
			{
				inline bool operator()(IEntity* pEntity) const
				{
					return pEntity && !pEntity->IsHidden();
				}
			};
			typedef Schematyc::CConfigurableUpdateFilter<IEntity*, NTypelist::CConstruct<SEntityNotHidden>::TType> EntityNotHiddenUpdateFilter;

		public:

			// IEntityComponent
			virtual void                       Initialize() override;
			virtual uint64                     GetEventMask() const override;
			virtual void                       ProcessEvent(const SEntityEvent& event) override;
			virtual void                       OnShutDown() override;
			// ~IEntityComponent

			void        Enable();
			void        Disable();

			void        SetVolumeSize(const Vec3& size);
			Vec3        GetVolumeSize() const;

			void        SetVolumeRadius(float radius);
			float       GetVolumeRadius() const;

			void        RenderVolume() const;

			static void ReflectType(Schematyc::CTypeDesc<CSchematycEntitySensorVolumeComponent>& desc);
			static void Register(Schematyc::IEnvRegistrar& registrar);

		protected:
#ifndef RELEASE
			// Helper functions to create the preview shapes
			IGeometry* CreateBoxGeometry() const;
			IGeometry* CreateSphereGeometry() const;

			// IEntityComponentPreviewer
			virtual void SerializeProperties(Serialization::IArchive& archive) final {}

			virtual IEntityComponentPreviewer* GetPreviewer() final { return this; }

			virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const final;
			// ~IEntityComponentPreviewer
#endif

		private:

			void          OnEntityEvent(const SEntityEvent& event);
			void          OnSensorEvent(const SSensorEvent& event);

			CSensorBounds CreateBounds(const Matrix34& worldTM, const CryTransform::CTransformPtr& transform) const;
			CSensorBounds CreateOBBBounds(const Matrix34& worldTM, const Vec3& pos, const Vec3& size, const Matrix33& rot) const;
			CSensorBounds CreateSphereBounds(const Matrix34& worldTM, const Vec3& pos, float radius) const;

			SensorTags    GetTags(const SensorTagNames& tagNames) const;

		private:

			SDimensions                 m_dimensions;
			STags                       m_tags;

			SensorVolumeId              m_volumeId = SensorVolumeId::Invalid;
			EntityNotHiddenUpdateFilter m_updateFilter;
			Schematyc::CConnectionScope m_connectionScope;
		};
	}
}