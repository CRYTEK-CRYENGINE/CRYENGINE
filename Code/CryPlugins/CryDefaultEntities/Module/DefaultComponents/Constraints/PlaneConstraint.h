#pragma once

#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CPlaneConstraintComponent
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
			virtual ~CPlaneConstraintComponent() = default;

			static void ReflectType(Schematyc::CTypeDesc<CPlaneConstraintComponent>& desc)
			{
				desc.SetGUID("{F9C3A204-809B-4DA8-965E-CA9417FEBD95}"_cry_guid);
				desc.SetEditorCategory("Physics Constraints");
				desc.SetLabel("Plane Constraint");
				desc.SetDescription("Constrains the physical object to a plan");
				//desc.SetIcon("icons:ObjectTypes/object.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

				desc.AddMember(&CPlaneConstraintComponent::m_bActive, 'actv', "Active", "Active", "Whether or not the constraint should be added on component reset", true);

				desc.AddMember(&CPlaneConstraintComponent::m_limitMin, 'lmin', "LimitMinX", "Twist rotation min angle", nullptr, -360.0_degrees);
				desc.AddMember(&CPlaneConstraintComponent::m_limitMax, 'lmax', "LimitMaxX", "Twist rotation max angle", nullptr, 360.0_degrees);
				desc.AddMember(&CPlaneConstraintComponent::m_limitMaxY, 'lmay', "LimitMaxY", "Bend max angle", nullptr, 0.0_degrees);
				desc.AddMember(&CPlaneConstraintComponent::m_axis, 'axi', "Axis", "Axis", nullptr, Vec3(0, 0, 1));
				desc.AddMember(&CPlaneConstraintComponent::m_damping, 'damp', "Damping", "Damping", nullptr, 0.f);
			}

			virtual void ConstrainToEntity(Schematyc::ExplicitEntityId targetEntityId, bool bDisableCollisionsWith)
			{
				if ((EntityId)targetEntityId != INVALID_ENTITYID)
				{
					if (IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity((EntityId)targetEntityId))
					{
						if (IPhysicalEntity* pPhysicalEntity = pTargetEntity->GetPhysicalEntity())
						{
							ConstrainTo(pPhysicalEntity, bDisableCollisionsWith);
						}
					}
				}
			}

			virtual void ConstrainToPoint()
			{
				ConstrainTo(WORLD_ENTITY, false);
			}

			virtual void ConstrainTo(IPhysicalEntity* pOtherEntity, bool bDisableCollisionsWith = false)
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
							CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Tried to add plane constraint to non-rigid or vehicle entity!");
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
							CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Tried to add plane constraint to non-rigid or vehicle entities!");
							return;
						}
#endif
					}

					Matrix34 slotTransform = GetWorldTransformMatrix();

					pe_action_add_constraint constraint;
					constraint.flags = world_frames | constraint_no_tears | constraint_plane;
					constraint.pt[0] = constraint.pt[1] = slotTransform.GetTranslation();
					constraint.qframe[0] = constraint.qframe[1] = Quat(slotTransform) * Quat::CreateRotationV0V1(Vec3(1, 0, 0), m_axis);
					constraint.xlimits[0] = m_limitMin.ToRadians();
					constraint.xlimits[1] = m_limitMax.ToRadians();
					constraint.yzlimits[0] = 0;
					constraint.yzlimits[1] = m_limitMaxY.ToRadians();
					constraint.damping = m_damping;

					if (!constraint.xlimits[0] && !constraint.xlimits[1] && !constraint.yzlimits[1])
					{
						constraint.flags |= constraint_no_rotation;
					}

					if (bDisableCollisionsWith && pOtherEntity != WORLD_ENTITY && pOtherEntity != pConstraintOwner)
					{
						constraint.flags |= constraint_ignore_buddy;

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
			}

			virtual void Activate(bool bActivate)
			{
				m_bActive = bActivate;

				m_pEntity->UpdateComponentEventMask(this);
			}

			bool IsActive() const { return m_bActive; }

			virtual void SetLimitsX(CryTransform::CAngle minLimit, CryTransform::CAngle maxLimit) { m_limitMin = minLimit; m_limitMax = maxLimit; }
			CryTransform::CAngle GetMinimumLimitX() const { return m_limitMin; }
			CryTransform::CAngle GetMaximumLimitX() const { return m_limitMax; }

			virtual void SetLimitsY(CryTransform::CAngle maxLimit) { m_limitMaxY = maxLimit; }
			CryTransform::CAngle GetMaximumLimitY() const { return m_limitMaxY; }

			virtual void SetDamping(float damping) { m_damping = damping; }
			float GetDamping() const { return m_damping; }

		protected:
			void Reset();

		protected:
			bool m_bActive = true;
			Vec3 m_axis = Vec3(0, 0, 1);

			CryTransform::CClampedAngle<-360, 360>  m_limitMin = -360.0_degrees;
			CryTransform::CClampedAngle<-360, 360>  m_limitMax = 360.0_degrees;
			CryTransform::CClampedAngle<0, 180>  m_limitMaxY = 0.0_degrees;

			Schematyc::Range<-10000, 10000> m_damping = 0.f;

			// Key = physical entity id, value = constraint id
			std::vector<std::pair<int, int>> m_constraintIds;
		};

	}
}