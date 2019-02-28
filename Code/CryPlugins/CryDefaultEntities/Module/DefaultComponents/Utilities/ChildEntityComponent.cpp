#include "StdAfx.h"
#include "ChildEntityComponent.h"

namespace Cry
{
	namespace DefaultComponents
	{
		void CChildEntityComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
		{
			// Functions
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CChildEntityComponent::GetChildEntityId, "{2E1D9257-8660-4E8E-99DD-D23FDE930F65}"_cry_guid, "GetChildEntityId");
				pFunction->BindOutput(0, 'enti', "EntityId", "Entity Identifier");
				pFunction->SetDescription("Gets the bound child entity's identifier");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				componentScope.Register(pFunction);
			}
		}

		CChildEntityComponent::~CChildEntityComponent()
		{
			// Remove the entity when we're destroyed
			RemoveEntity();

			RemoveIgnoreConstraint();
		}

		void CChildEntityComponent::Initialize()
		{
			SpawnEntity();
		}

		void CChildEntityComponent::ProcessEvent(const SEntityEvent& event)
		{
			if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
			{
				RemoveEntity();
				SpawnEntity();
			}
			else if (event.event ==  ENTITY_EVENT_DONE)
			{
				RemoveEntity();
				RemoveIgnoreConstraint();
			}
			else if (event.event == ENTITY_EVENT_DETACH)
			{
				if (event.nParam[0] == m_childEntityId)
				{
					// Child was detached, probably deleted
					m_childEntityId = INVALID_ENTITYID;
				}
			}
			else if (event.event == ENTITY_EVENT_PHYSICAL_TYPE_CHANGED)
			{
				// Ignore collisions between the two entities
				if (m_bIgnoreContactsWithChild)
				{
					AddIgnoreConstraint();
				}
				else
				{
					RemoveIgnoreConstraint();
				}
			}
#ifndef RELEASE
			else if (event.event == ENTITY_EVENT_XFORM)
			{
				if (gEnv->IsEditing() && (event.nParam[0] & ENTITY_XFORM_EDITOR) != 0)
				{
					RemoveEntity();
					SpawnEntity();
				}
			}
			else if (event.event == ENTITY_EVENT_START_GAME)
			{
				if (IEntity* pChildEntity = gEnv->pEntitySystem->GetEntity(m_childEntityId))
				{
					pChildEntity->SetWorldTM(GetWorldTransformMatrix());
				}

				if (m_bIgnoreContactsWithChild)
				{
					AddIgnoreConstraint();
				}
				else
				{
					RemoveIgnoreConstraint();
				}
			}
#endif
		}

		uint64 CChildEntityComponent::GetEventMask() const
		{
			return ENTITY_EVENT_BIT(ENTITY_EVENT_DETACH)
				| ENTITY_EVENT_BIT(ENTITY_EVENT_PHYSICAL_TYPE_CHANGED)
				| ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
				| ENTITY_EVENT_BIT(ENTITY_EVENT_DONE)
#ifndef RELEASE
				| ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM)
				| ENTITY_EVENT_BIT(ENTITY_EVENT_START_GAME)
#endif
				;
		}

		void CChildEntityComponent::SpawnEntity()
		{
			if (m_childEntityId != INVALID_ENTITYID)
			{
				return;
			}

			SEntitySpawnParams spawnParams;
			spawnParams.nFlags = m_pEntity->GetFlags() | ENTITY_FLAG_UNREMOVABLE;

			if (m_className.value.size() > 0)
			{
				spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(m_className.value);
			}

			Matrix34 worldTransform = GetWorldTransformMatrix();
			spawnParams.vPosition = worldTransform.GetTranslation();
			spawnParams.qRotation = Quat(worldTransform);
			spawnParams.vScale = Vec3(worldTransform.GetUniformScale());

			IEntity* pChildEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams);
			m_childEntityId = pChildEntity->GetId();

			if (m_bLinkTransformation)
			{
				SChildAttachParams attachParams;
				attachParams.m_nAttachFlags = IEntity::EAttachmentFlags::ATTACHMENT_KEEP_TRANSFORMATION;
				m_pEntity->AttachChild(pChildEntity, attachParams);
			}

			// Ignore collisions between the two entities
			if (m_bIgnoreContactsWithChild)
			{
				AddIgnoreConstraint();
			}
			else
			{
				RemoveIgnoreConstraint();
			}
		}

		void CChildEntityComponent::RemoveEntity()
		{
			if (m_childEntityId != INVALID_ENTITYID)
			{
				if (IEntity* pChildEntity = gEnv->pEntitySystem->GetEntity(m_childEntityId))
				{
					pChildEntity->SetFlags(m_pEntity->GetFlags() & ~ENTITY_FLAG_UNREMOVABLE);
					gEnv->pEntitySystem->RemoveEntity(m_childEntityId, true);

					m_childEntityId = INVALID_ENTITYID;
				}
			}
		}

		void CChildEntityComponent::AddIgnoreConstraint()
		{
			IEntity* pChildEntity = gEnv->pEntitySystem->GetEntity(m_childEntityId);
			if (pChildEntity == nullptr)
				return;

			if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
			{
				if (IPhysicalEntity* pChildPhysicalEntity = pChildEntity->GetPhysicalEntity())
				{
					pe_action_add_constraint constraint;
					constraint.pt[0] = ZERO;
					constraint.flags = constraint_ignore_buddy | constraint_inactive;
					constraint.pBuddy = pPhysicalEntity;
					m_childConstraintId = pChildPhysicalEntity->Action(&constraint);

					// Add the same constraint to the other entity
					constraint.flags |= constraint_inactive;
					constraint.pBuddy = pChildPhysicalEntity;
					m_constraintId = pPhysicalEntity->Action(&constraint);
				}
			}
		}

		void CChildEntityComponent::RemoveIgnoreConstraint()
		{
			pe_action_update_constraint constraintUpdate;
			constraintUpdate.bRemove = 1;

			if (IEntity* pChildEntity = gEnv->pEntitySystem->GetEntity(m_childEntityId))
			{
				if (IPhysicalEntity* pChildPhysicalEntity = pChildEntity->GetPhysicalEntity())
				{
					constraintUpdate.idConstraint = m_childConstraintId;
					pChildPhysicalEntity->Action(&constraintUpdate);
				}
			}

			if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
			{
				constraintUpdate.idConstraint = m_constraintId;
				pPhysicalEntity->Action(&constraintUpdate);
			}
		}
	}
}