#pragma once

#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

#include <CrySystem/VR/IHMDDevice.h>
#include <CrySystem/VR/IHMDManager.h>

#include <array>

namespace Cry
{
	namespace DefaultComponents
	{
		namespace VirtualReality
		{
			//! An entity component that requires more complex interactions by receiving direct callbacks
			struct IInteractableComponent : public IEntityComponent
			{
				//! Called when the object is let go by the hand
				virtual void OnReleased() = 0;

				//! Called when the object is picked up by a hand
				virtual void OnPickedUp(EHmdController controllerId) = 0;

				static void ReflectType(Schematyc::CTypeDesc<IInteractableComponent>& desc)
				{
					desc.SetGUID("{67C893FD-93FB-49D0-AF62-E6DCFF11E35F}"_cry_guid);
				}
			};

			class CInteractionComponent final : public IEntityComponent
			{
				struct SHand
				{
					int slotId = -1;
					IGeometry* pPhysicsGeometry = nullptr;
					EHmdController controllerId;
					bool holdingTrigger = false;
					EntityId heldEntityId = INVALID_ENTITYID;
					int constraintId = -1;
					_smart_ptr<IPhysicalEntity> pAttachedEntity;
				};

				enum class EHand : uint8
				{
					Left = 0,
					Right
				};

			public:
				static void Register(Schematyc::CEnvRegistrationScope& componentScope);
				static void ReflectType(Schematyc::CTypeDesc<CInteractionComponent>& desc)
				{
					desc.SetGUID("{E7120655-5607-4530-9AC4-63F07A577FEA}"_cry_guid);
					desc.SetEditorCategory("Physics");
					desc.SetLabel("VR Interaction");
					desc.SetDescription("Allows for interacting with objects in VR, picking up things using the controllers");
					desc.SetComponentFlags({ IEntityComponent::EFlags::ClientOnly });
					desc.AddMember(&CInteractionComponent::m_leftHandModelPath, 'left', "LeftHandModel", "Left Hand Model", nullptr, "");
					desc.AddMember(&CInteractionComponent::m_rightHandModelPath, 'righ', "RightHandModel", "Right Hand Model", nullptr, "");
					desc.AddMember(&CInteractionComponent::m_bReleaseWithPressKey, 'relw', "ReleaseWithPressKey", "Release On Hold Release", "Whether to release the object when the key used to hold is released", false);
				}

				// IEntityComponent
				virtual void Initialize() override;

				virtual void ProcessEvent(const SEntityEvent& event) override;
				virtual uint64 GetEventMask() const override;
				// ~IEntityComponent

			protected:
				struct SPickableEntity
				{
					IEntity* pEntity = nullptr;
					IPhysicalEntity* pPhysicalEntity = nullptr;
					int partId = 0;
				};

				void LoadHandModel(const char* szModelPath, EHand hand);
				void PickUpObject(SHand& hand, const SPickableEntity& pickableEntity);
				void ReleaseObject(SHand& hand, const Vec3& linearVelocity, const Vec3& angularVelocity);
				
				SPickableEntity FindPickableEntity(const SHand& hand);

			protected:
				Schematyc::GeomFileName m_leftHandModelPath;
				Schematyc::GeomFileName m_rightHandModelPath;

				std::array<SHand, 2> m_hands;
				bool m_bReleaseWithPressKey = false;
			};
		}
	}
}