// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SchematycEntitySensorVolumeComponent.h"

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySerialization/DynArray.h>

#include "SensorMap.h"
#include "SensorSystem.h"
#include "SensorTagLibrary.h"

namespace Cry
{
	namespace SensorSystem
	{
		SERIALIZATION_ENUM_BEGIN_NESTED(CSchematycEntitySensorVolumeComponent, EVolumeShape, "Sensor volume shape")
			SERIALIZATION_ENUM(CSchematycEntitySensorVolumeComponent::EVolumeShape::Box, "Box", "Box")
			SERIALIZATION_ENUM(CSchematycEntitySensorVolumeComponent::EVolumeShape::Sphere, "Sphere", "Sphere")
			SERIALIZATION_ENUM_END()

			inline bool Serialize(Serialization::IArchive& archive, SSensorTagName& value, const char* szName, const char* szLabel)
		{
			return static_cast<CSensorTagLibrary&>(CSensorSystem::GetInstance().GetTagLibrary()).SerializeTagName(archive, static_cast<string&>(value), szName, szLabel); // #TODO : Find a nicer way to access CSensorTagLibrary!!!
		}

		static void ReflectType(Schematyc::CTypeDesc<SSensorTagName>& desc)
		{
			desc.SetGUID("{351924F1-EF4F-47E6-8E2E-05F0643F6A1A}"_cry_guid);
			desc.SetLabel("Tag Name");
		}

		static void ReflectType(Schematyc::CTypeDesc<CSchematycEntitySensorVolumeComponent::EVolumeShape>& desc)
		{
			desc.SetGUID("{0C6ACF7E-9123-4DFE-AFA9-9FCE8AFBDA03}"_cry_guid);
			desc.SetLabel("Volume Shape");
			desc.SetDescription("Determines the type of a shape.");
			desc.SetDefaultValue(CSchematycEntitySensorVolumeComponent::EVolumeShape::Box);
		}

		void CSchematycEntitySensorVolumeComponent::SDimensions::ReflectType(Schematyc::CTypeDesc<CSchematycEntitySensorVolumeComponent::SDimensions>& desc)
		{
			desc.SetGUID("ed2add7d-d063-4371-9a91-b6c13e0c2751"_cry_guid);
			desc.SetLabel("Dimension");
			desc.SetDescription("Determines shape and dimension of the volume.");
			desc.AddMember(&CSchematycEntitySensorVolumeComponent::SDimensions::shape, 'shap', "Shape", "Shape", "Shape of the volume.", EVolumeShape::Box);
			desc.AddMember(&CSchematycEntitySensorVolumeComponent::SDimensions::size, 'size', "Size", "Size", "Determines the size of the box.", Vec3(1.f));
			desc.AddMember(&CSchematycEntitySensorVolumeComponent::SDimensions::radius, 'rad', "Radius", "Radius", "Radius of the sphere.", 1.f);
		}

		// N.B. Non-intrusive serialization is used here only to ensure backward compatibility.
		inline bool Serialize(Serialization::IArchive& archive, CSchematycEntitySensorVolumeComponent::SDimensions& value, const char* szName, const char* szLabel)
		{
			archive(value.shape, "shape", "Shape");
			archive.doc("Shape");
			switch (value.shape)
			{
				case CSchematycEntitySensorVolumeComponent::EVolumeShape::Box:
				{
					archive(value.size, "size", "Size");
					archive.doc("Size");
					break;
				}
				case CSchematycEntitySensorVolumeComponent::EVolumeShape::Sphere:
				{
					archive(value.radius, "radius", "Radius");
					archive.doc("Radius");
					break;
				}
			}
			return true;
		}

		void CSchematycEntitySensorVolumeComponent::STags::ReflectType(Schematyc::CTypeDesc<CSchematycEntitySensorVolumeComponent::STags>& desc)
		{
			desc.SetGUID("6f03eddc-71ff-467e-9d8d-ad7e57438aad"_cry_guid);
			desc.SetLabel("Tags");
			desc.AddMember(&CSchematycEntitySensorVolumeComponent::STags::attributeTags, 'atT', "Attribute", "Attribute", "Tags describing this volume.", SensorTagNames());
			desc.AddMember(&CSchematycEntitySensorVolumeComponent::STags::listenerTags, 'liT', "Listener", "Listener", "Listen for collision with volumes that have one or more of these tags set.", SensorTagNames());
		}

		// N.B. Non-intrusive serialization is used here only to ensure backward compatibility.
		//      If files were patched we could instead reflect members and let the system serialize them automatically.
		inline bool Serialize(Serialization::IArchive& archive, CSchematycEntitySensorVolumeComponent::STags& value, const char* szName, const char* szLabel)
		{
			if (archive.openBlock("tags", "Tags"))
			{
				archive(value.attributeTags, "attributeTags", "+Attribute");
				archive.doc("Tags describing this volume.");

				archive(value.listenerTags, "listenerTags", "+Listener");
				archive.doc("Listen for collision with volumes that have one or more of these tags set.");

				archive.closeBlock();
			}
			return true;
		}

		CSchematycEntitySensorVolumeComponent::SEnteringSignal::SEnteringSignal() {}

		CSchematycEntitySensorVolumeComponent::SEnteringSignal::SEnteringSignal(EntityId _entityId)
			: entityId(static_cast<Schematyc::ExplicitEntityId>(_entityId))
		{}

		void CSchematycEntitySensorVolumeComponent::SEnteringSignal::ReflectType(Schematyc::CTypeDesc<SEnteringSignal>& desc)
		{
			desc.SetGUID("19cb7f01-1921-441c-b90e-64f940f38e80"_cry_guid);
			desc.SetLabel("Entering");
			desc.SetDescription("Sent when an entity enters the sensor volume.");
			desc.AddMember(&SEnteringSignal::entityId, 'ent', "entityId", "EntityId", "Id of entity entering sensor volume", INVALID_ENTITYID);
		}

		CSchematycEntitySensorVolumeComponent::SLeavingSignal::SLeavingSignal() {}

		CSchematycEntitySensorVolumeComponent::SLeavingSignal::SLeavingSignal(EntityId _entityId)
			: entityId(static_cast<Schematyc::ExplicitEntityId>(_entityId))
		{}

		void CSchematycEntitySensorVolumeComponent::SLeavingSignal::ReflectType(Schematyc::CTypeDesc<SLeavingSignal>& desc)
		{
			desc.SetGUID("474cc5fe-d7f0-4606-a06c-72cc0b3080cb"_cry_guid);
			desc.SetLabel("Leaving");
			desc.SetDescription("Sent when an entity leaves the sensor volume.");
			desc.AddMember(&SLeavingSignal::entityId, 'ent', "entityId", "EntityId", "Id of entity leaving sensor volume", INVALID_ENTITYID);
		}

		void CSchematycEntitySensorVolumeComponent::Initialize()
		{
			SSensorVolumeParams volumeParams;
			volumeParams.entityId = GetEntity()->GetId();
			volumeParams.bounds = CreateBounds(GetEntity()->GetWorldTM(), GetTransform());
			volumeParams.attributeTags = GetTags(m_tags.attributeTags);
			volumeParams.listenerTags = GetTags(m_tags.listenerTags);
			volumeParams.eventListener = SCHEMATYC_MEMBER_DELEGATE(&CSchematycEntitySensorVolumeComponent::OnSensorEvent, *this);

			m_volumeId = CSensorSystem::GetInstance().GetMap().CreateVolume(volumeParams);

			// #TODO : Implement enable/disable functionality!!!
			//if (m_properties.bEnabled)
			//{
			//	Enable();
			//}
		}

		void CSchematycEntitySensorVolumeComponent::OnShutDown()
		{
			CSensorSystem::GetInstance().GetMap().DestroyVolume(m_volumeId);
			m_volumeId = SensorVolumeId();
		}

#ifndef RELEASE
		IGeometry * CSchematycEntitySensorVolumeComponent::CreateBoxGeometry() const
		{
			primitives::box primitive;
			primitive.size = m_dimensions.size;
			primitive.Basis = IDENTITY;
			primitive.bOriented = 1;
			primitive.center = Vec3(0, 0, m_dimensions.size.z);

			return gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive((int)primitive.type, &primitive);
		}

		IGeometry* CSchematycEntitySensorVolumeComponent::CreateSphereGeometry() const
		{
			primitives::sphere primitive;
			primitive.center = ZERO;
			primitive.r = m_dimensions.radius;

			return gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive((int)primitive.type, &primitive);
		}

		void CSchematycEntitySensorVolumeComponent::Render(const IEntity & entity, const IEntityComponent & component, SEntityPreviewContext & context) const
		{
			if (context.bSelected)
			{
				Matrix34 slotTransform = GetWorldTransformMatrix();
				IGeometry* pPrimGeom = m_dimensions.shape == EVolumeShape::Box ? CreateBoxGeometry() : CreateSphereGeometry();

				geom_world_data geomWorldData;
				geomWorldData.R = Matrix33(slotTransform);
				geomWorldData.scale = slotTransform.GetUniformScale();
				geomWorldData.offset = slotTransform.GetTranslation();

				gEnv->pSystem->GetIPhysRenderer()->DrawGeometry(pPrimGeom, &geomWorldData, -1, 0, ZERO, context.debugDrawInfo.color);

				pPrimGeom->Release();
			}
		}
#endif

		void CSchematycEntitySensorVolumeComponent::Enable()
		{
			// #TODO : Implement enable/disable functionality!!!
			//CSensorSystem::GetInstance().GetDefaultMap().EnableVolume(m_volumeId, true);
		}

		void CSchematycEntitySensorVolumeComponent::Disable()
		{
			// #TODO : Implement enable/disable functionality!!!
			//CSensorSystem::GetInstance().GetDefaultMap().EnableVolume(m_volumeId, false);
		}

		void CSchematycEntitySensorVolumeComponent::SetVolumeSize(const Vec3& size)
		{
			m_dimensions.size = size;

			IEntity& entity = *GetEntity();
			CSensorSystem::GetInstance().GetMap().UpdateVolumeBounds(m_volumeId, CreateBounds(entity.GetWorldTM(), GetTransform()));
		}

		Vec3 CSchematycEntitySensorVolumeComponent::GetVolumeSize() const
		{
			return m_dimensions.size;
		}

		void CSchematycEntitySensorVolumeComponent::SetVolumeRadius(float radius)
		{
			m_dimensions.radius = radius;

			IEntity& entity = *GetEntity();
			CSensorSystem::GetInstance().GetMap().UpdateVolumeBounds(m_volumeId, CreateBounds(entity.GetWorldTM(), GetTransform()));
		}

		float CSchematycEntitySensorVolumeComponent::GetVolumeRadius() const
		{
			return m_dimensions.radius;
		}

		void CSchematycEntitySensorVolumeComponent::ReflectType(Schematyc::CTypeDesc<CSchematycEntitySensorVolumeComponent>& desc)
		{
			desc.SetGUID("5F0322C0-2EB0-46C7-B3E3-56AB5F200E74"_cry_guid);
			desc.SetLabel("SensorVolume");
			desc.SetDescription("Entity sensor volume component");
			desc.SetIcon("icons:schematyc/entity_sensor_volume_component.ico");
			desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Attach });
			desc.AddMember(&CSchematycEntitySensorVolumeComponent::m_dimensions, 'dims', "dimensions", "Dimensions", "Determines the dimension of the volume.", CSchematycEntitySensorVolumeComponent::SDimensions());
			desc.AddMember(&CSchematycEntitySensorVolumeComponent::m_tags, 'tags', "tags", "Tags", "Tags the volume will listen to.", CSchematycEntitySensorVolumeComponent::STags());
		}

		void CSchematycEntitySensorVolumeComponent::Register(Schematyc::IEnvRegistrar& registrar)
		{
			Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
			{
				Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CSchematycEntitySensorVolumeComponent));
				// Functions
				{
					auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSchematycEntitySensorVolumeComponent::SetVolumeSize, "C926DA55-97A2-4C97-A7E6-C1416DD6284A"_cry_guid, "SetVolumeSize");
					pFunction->SetDescription("Set size of box volume.");
					pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
					pFunction->BindInput(1, 'size', "Size", nullptr, Vec3(1.0f));
					componentScope.Register(pFunction);
				}
				{
					auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSchematycEntitySensorVolumeComponent::GetVolumeSize, "E8E87E86-C96B-4948-A298-0FE010E04F2E"_cry_guid, "GetVolumeSize");
					pFunction->SetDescription("Get size of box volume.");
					pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
					pFunction->BindOutput(0, 'size', "Size");
					componentScope.Register(pFunction);
				}
				{
					auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSchematycEntitySensorVolumeComponent::SetVolumeRadius, "17FB678F-2256-4F7B-AB46-E8EAACD2497C"_cry_guid, "SetVolumeRadius");
					pFunction->SetDescription("Set radius of sphere volume.");
					pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
					pFunction->BindInput(1, 'rad', "Radius", nullptr, 0.0f);
					componentScope.Register(pFunction);
				}
				{
					auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSchematycEntitySensorVolumeComponent::GetVolumeRadius, "3950621F-8D47-4BE7-836C-35F211B41FBB"_cry_guid, "GetVolumeRadius");
					pFunction->SetDescription("Get radius of sphere volume.");
					pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
					pFunction->BindOutput(0, 'rad', "Radius");
					componentScope.Register(pFunction);
				}
				// Signals
				{
					componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SEnteringSignal));
					componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SLeavingSignal));
				}
			}
		}

		uint64 CSchematycEntitySensorVolumeComponent::GetEventMask() const
		{
			return ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM);
		}

		void CSchematycEntitySensorVolumeComponent::ProcessEvent(const SEntityEvent& event)
		{
			switch (event.event)
			{
				case ENTITY_EVENT_XFORM:
				{
					IEntity& entity = *GetEntity();
					CSensorSystem::GetInstance().GetMap().UpdateVolumeBounds(m_volumeId, CreateBounds(entity.GetWorldTM(), GetTransform()));
				}
				break;
			}
		}

		void CSchematycEntitySensorVolumeComponent::OnSensorEvent(const SSensorEvent& event)
		{
			const SSensorVolumeParams otherVolumeParams = CSensorSystem::GetInstance().GetMap().GetVolumeParams(event.eventVolumeId);

			switch (event.type)
			{
				case ESensorEventType::Entering:
				{
					if (Schematyc::IObject* pSchematycObject = GetEntity()->GetSchematycObject())
					{
						pSchematycObject->ProcessSignal(SEnteringSignal(otherVolumeParams.entityId), GetGUID());
					}
					break;
				}
				case ESensorEventType::Leaving:
				{
					if (Schematyc::IObject* pSchematycObject = GetEntity()->GetSchematycObject())
					{
						pSchematycObject->ProcessSignal(SLeavingSignal(otherVolumeParams.entityId), GetGUID());
						break;
					}
				}
			}
		}

		CSensorBounds CSchematycEntitySensorVolumeComponent::CreateBounds(const Matrix34& worldTM, const CryTransform::CTransformPtr& transform) const
		{
			switch (m_dimensions.shape)
			{
				case EVolumeShape::Box:
				{
					Matrix33 rotMat(IDENTITY);
					if (transform)
						rotMat = transform->GetRotation().ToMatrix33();
					if (transform)
						return CreateOBBBounds(worldTM, transform->GetTranslation(), m_dimensions.size, transform->GetRotation().ToMatrix33());
					else
						return CreateOBBBounds(worldTM, Vec3(0, 0, 0), m_dimensions.size, Matrix33(IDENTITY));
				}
				case EVolumeShape::Sphere:
				{
					Vec3 pos(0, 0, 0);
					if (transform)
						pos = transform->GetTranslation();

					return CreateSphereBounds(worldTM, pos, m_dimensions.radius);
				}
			}
			return CSensorBounds();
		}

		CSensorBounds CSchematycEntitySensorVolumeComponent::CreateOBBBounds(const Matrix34& worldTM, const Vec3& pos, const Vec3& size, const Matrix33& rot) const
		{
			const QuatT transform = QuatT(worldTM) * QuatT(Quat(rot), pos);
			return CSensorBounds(OBB::CreateOBB(Matrix33(transform.q), size * 0.5f, transform.q.GetInverted() * transform.t));
		}

		CSensorBounds CSchematycEntitySensorVolumeComponent::CreateSphereBounds(const Matrix34& worldTM, const Vec3& pos, float radius) const
		{
			return CSensorBounds(Sphere(worldTM.TransformPoint(pos), radius));
		}

		SensorTags CSchematycEntitySensorVolumeComponent::GetTags(const SensorTagNames& tagNames) const
		{
			ISensorTagLibrary& sensorTagLibrary = CSensorSystem::GetInstance().GetTagLibrary();
			SensorTags tags;
			for (uint32 i = 0; i < tagNames.Size(); ++i)
			{
				tags.Add(sensorTagLibrary.GetTag(tagNames.At(i).c_str()));
			}
			return tags;
		}

		void CSchematycEntitySensorVolumeComponent::RenderVolume() const
		{
			CSensorSystem::GetInstance().GetMap().GetVolumeParams(m_volumeId).bounds.DebugDraw(ColorB(0, 150, 0, 128));
		}

		class CSensorVolumeComponentPreviewer : public IEntityComponentPreviewer
		{
		public:
			bool m_bShowVolumes = false;

		public:
			void Serialize(Serialization::IArchive& archive)
			{
				archive(m_bShowVolumes, "bShowVolumes", "Show Volumes");
			}

			// IComponentPreviewer
			virtual void SerializeProperties(Serialization::IArchive& archive) override
			{
				archive(*this, "properties", "Sensor Component");
			}
			virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext& context) const override
			{
				const CSchematycEntitySensorVolumeComponent& sensorComponent = static_cast<const CSchematycEntitySensorVolumeComponent&>(component);
				if (m_bShowVolumes)
				{
					sensorComponent.RenderVolume();
				}
			}
			// ~IComponentPreviewer
		};
	}
}