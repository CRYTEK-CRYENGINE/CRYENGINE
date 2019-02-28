#pragma once

#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CLineConstraintComponent
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

			virtual void OnShutDown() final;
			// ~IEntityComponent

#ifndef RELEASE
			// IEntityComponentPreviewer
			virtual IEntityComponentPreviewer* GetPreviewer() final { return this; }

			virtual void SerializeProperties(Serialization::IArchive& archive) final {}
			virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const final;
			// ~IEntityComponentPreviewer
#endif

		public:
			virtual ~CLineConstraintComponent() = default;

			static void ReflectType(Schematyc::CTypeDesc<CLineConstraintComponent>& desc)
			{
				desc.SetGUID("{D25359AC-E566-47F9-B056-AC52C4361348}"_cry_guid);
				desc.SetEditorCategory("Physics Constraints");
				desc.SetLabel("Line Constraint");
				desc.SetDescription("Constrains the physical object to a line");
				//desc.SetIcon("icons:ObjectTypes/object.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

				desc.AddMember(&CLineConstraintComponent::m_bActive, 'actv', "Active", "Active", "Whether or not the constraint should be added on component reset", true);
				desc.AddMember(&CLineConstraintComponent::m_bLockRotation, 'loro', "LockRot", "Lock Rotation", nullptr, true);
				desc.AddMember(&CLineConstraintComponent::m_axis, 'axis', "Axis", "Axis", "Axis around which the physical entity is constrained", Vec3(0.f, 0.f, 1.f));

				desc.AddMember(&CLineConstraintComponent::m_limitMin, 'lmin', "LimitMin", "Minimum Limit", nullptr, 0.f);
				desc.AddMember(&CLineConstraintComponent::m_limitMax, 'lmax', "LimitMax", "Maximum Limit", nullptr, 1.f);
				desc.AddMember(&CLineConstraintComponent::m_damping, 'damp', "Damping", "Damping", nullptr, 0.f);
			}

			virtual void ConstrainToEntity(Schematyc::ExplicitEntityId targetEntityId, bool bDisableCollisionsWith, bool bAllowRotation)
			{
				if ((EntityId)targetEntityId != INVALID_ENTITYID)
				{
					if (IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity((EntityId)targetEntityId))
					{
						if (IPhysicalEntity* pPhysicalEntity = pTargetEntity->GetPhysicalEntity())
						{
							ConstrainTo(pPhysicalEntity, bDisableCollisionsWith, bAllowRotation);
						}
					}
				}
			}

			virtual void ConstrainToPoint(bool bAllowRotation)
			{
				ConstrainTo(WORLD_ENTITY, false, bAllowRotation);
			}

			virtual void ConstrainTo(IPhysicalEntity* pOtherEntity, bool bDisableCollisionsWith = false, bool bAllowRotation = true)
			{
				if (m_constraintIds.size() > 0)
				{
					Remove();
				}

				if (IPhysicalEntity* pConstraintOwner = m_pEntity->GetPhysicalEntity())
				{
					// Constraints can only be added to rigid-based entities
					if (pConstraintOwner->GetType() != PE_RIGID && pConstraintOwner->GetType() != PE_WHEELEDVEHICLE)
					{
						if (pOtherEntity == WORLD_ENTITY)
						{
							CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Tried to add line constraint to non-rigid or vehicle entity!");
							return;
						}

						// Swap the pointers around
						IPhysicalEntity* pTemp = pConstraintOwner;
						pConstraintOwner = pOtherEntity;
						pOtherEntity = pTemp;

#ifndef RELEASE
						// Validate the same check again
						if (pConstraintOwner->GetType() != PE_RIGID && pConstraintOwner->GetType() != PE_WHEELEDVEHICLE)
						{
							CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Tried to add line constraint to non-rigid or vehicle entities!");
							return;
						}
#endif
					}

					Matrix34 slotTransform = GetWorldTransformMatrix();

					pe_action_add_constraint constraint;
					constraint.flags = world_frames | constraint_no_tears | constraint_line;
					constraint.pt[0] = constraint.pt[1] = slotTransform.GetTranslation();
					constraint.qframe[0] = constraint.qframe[1] = Quat(slotTransform) * Quat::CreateRotationV0V1(Vec3(1, 0, 0), m_axis);
					constraint.xlimits[0] = m_limitMin;
					constraint.xlimits[1] = m_limitMax;
					constraint.yzlimits[0] = constraint.yzlimits[1] = 0;
					constraint.damping = m_damping;

					if (!bAllowRotation)
					{
						constraint.flags |= constraint_no_rotation;
					}

					if (bDisableCollisionsWith && pOtherEntity != WORLD_ENTITY && pOtherEntity != pConstraintOwner)
					{
						constraint.flags |= constraint_ignore_buddy | constraint_inactive;

						constraint.pBuddy = pConstraintOwner;
						pOtherEntity->Action(&constraint);
						m_constraintIds.emplace_back(gEnv->pPhysicalWorld->GetPhysicalEntityId(pOtherEntity), pConstraintOwner->Action(&constraint));

						constraint.flags &= ~constraint_inactive;
					}

					constraint.pBuddy = pOtherEntity;

					int ownerId = gEnv->pPhysicalWorld->GetPhysicalEntityId(pConstraintOwner);
					m_constraintIds.emplace_back(ownerId, pConstraintOwner->Action(&constraint));
				}
			}

			virtual void Remove()
			{
				for (std::pair<int, int> constraintIdPair : m_constraintIds)
				{
					if (IPhysicalEntity* pPhysicalEntity = gEnv->pPhysicalWorld->GetPhysicalEntityById(constraintIdPair.first))
					{
						pe_action_update_constraint constraint;
						constraint.idConstraint = constraintIdPair.second;
						constraint.bRemove = 1;
						pPhysicalEntity->Action(&constraint);
					}
				}

				m_constraintIds.clear();
			}

			virtual void Activate(bool bActivate)
			{
				m_bActive = bActivate;

				m_pEntity->UpdateComponentEventMask(this);
			}
			bool IsActive() const { return m_bActive; }

			virtual void SetAxis(Vec3& axis) { m_axis = axis; }
			const Vec3& GetAxis() const { return m_axis; }

			virtual void SetLimits(float minLimit, float maxLimit) { m_limitMin = minLimit; m_limitMax = maxLimit; }
			float GetMinimumLimit() const { return m_limitMin; }
			float GetMaximumLimit() const { return m_limitMax; }

			virtual void SetDamping(float damping) { m_damping = damping; }
			float GetDamping() const { return m_damping; }

		protected:
			void Reset();

		protected:
			bool m_bActive = true;
			bool m_bLockRotation = true;
			Vec3 m_axis = Vec3(0, 0, 1);

			Schematyc::Range<-10000, 10000> m_limitMin = 0.f;
			Schematyc::Range<-10000, 10000> m_limitMax = 1.f;

			Schematyc::Range<-10000, 10000> m_damping = 0.f;

			std::vector<std::pair<int, int>> m_constraintIds;
		};
	}
}