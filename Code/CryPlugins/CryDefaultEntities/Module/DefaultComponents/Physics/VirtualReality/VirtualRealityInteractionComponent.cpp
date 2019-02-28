#include "StdAfx.h"
#include "VirtualRealityInteractionComponent.h"

#include <Cry3DEngine/IIndexedMesh.h>

namespace Cry
{
	namespace DefaultComponents
	{
		namespace VirtualReality
		{
			void CInteractionComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
			{
			}

			void CInteractionComponent::Initialize()
			{
				if (IHmdDevice* pDevice = gEnv->pSystem->GetHmdManager()->GetHmdDevice())
				{
					if (pDevice->GetClass() == eHmdClass_Oculus)
					{
						m_hands[static_cast<uint8>(EHand::Left)].controllerId = eHmdController_OculusLeftHand;
						m_hands[static_cast<uint8>(EHand::Right)].controllerId = eHmdController_OculusRightHand;
					}
					else if (pDevice->GetClass() == eHmdClass_OpenVR)
					{
						m_hands[static_cast<uint8>(EHand::Left)].controllerId = eHmdController_OpenVR_1;
						m_hands[static_cast<uint8>(EHand::Right)].controllerId = eHmdController_OpenVR_2;
					}
					else
					{
						CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Unsupported HMD device encountered in interaction component!");
						return;
					}

					LoadHandModel(m_leftHandModelPath.value, EHand::Left);
					LoadHandModel(m_rightHandModelPath.value, EHand::Right);
				}


				pe_params_flags pf; 
				pf.flagsOR = pef_never_affect_triggers;

				primitives::sphere sph;
				sph.center.zero();
				sph.r = 0.05f;

				IGeomManager* pGeoman = gEnv->pPhysicalWorld->GetGeomManager();
				phys_geometry* pGeom = pGeoman->RegisterGeometry(pGeoman->CreatePrimitive(primitives::sphere::type, &sph));

				pe_geomparams gp;
				gp.flags = gp.flagsCollider = 0;

				for (int i = 0; i < 2; ++i)
				{
					m_hands[i].pAttachedEntity = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_RIGID, &pf);
					m_hands[i].pAttachedEntity->AddGeometry(pGeom, &gp);
				}

				pGeom->pGeom->Release();
				pGeoman->UnregisterGeometry(pGeom);
			}

			void CInteractionComponent::LoadHandModel(const char* szModelPath, EHand handType)
			{
				// Characters not currently supported since we need the physics geometry
				/*if (IsCharacterFile(szModelPath))
				{
					m_hands[static_cast<uint8>(hand)].slotId = m_pEntity->LoadCharacter(-1, szModelPath);
				}*/

				SHand& hand = m_hands[static_cast<uint8>(handType)];
				hand.slotId = m_pEntity->LoadGeometry(-1, szModelPath, nullptr, IEntity::EF_NO_STREAMING);

				if (IStatObj* pHandObject = m_pEntity->GetStatObj(hand.slotId))
				{
					pHandObject->Invalidate();
					if (IIndexedMesh* pIndexedMesh = pHandObject->GetIndexedMesh(true))
					{
						CMesh* pMesh = pIndexedMesh->GetMesh();

						int flags = mesh_multicontact1 | mesh_OBB | mesh_AABB | mesh_approx_box | mesh_approx_capsule;

						hand.pPhysicsGeometry = gEnv->pPhysicalWorld->GetGeomManager()->CreateMesh(
							pMesh->GetStreamPtr<Vec3>(CMesh::POSITIONS),
							pMesh->GetStreamPtr<vtx_idx>(CMesh::INDICES),
							nullptr,
							0,
							pIndexedMesh->GetIndexCount() / 3,
							flags);
					}
				}
			}

			void CInteractionComponent::ProcessEvent(const SEntityEvent& event)
			{
				if (event.event == ENTITY_EVENT_UPDATE)
				{
					IHmdDevice* pDevice = gEnv->pSystem->GetHmdManager()->GetHmdDevice();
					if (pDevice == nullptr)
						return;

					SEntityUpdateContext* pUpdateContext = reinterpret_cast<SEntityUpdateContext*>(event.nParam[0]);

					for (SHand& hand : m_hands)
					{
						EKeyId triggerKeyId;
						EKeyId releaseHeldItemKeyId;

						if (pDevice->GetClass() == eHmdClass_Oculus)
						{
							triggerKeyId = static_cast<EKeyId>(hand.controllerId + eKI_Motion_OculusTouch_L1);
							releaseHeldItemKeyId = eKI_Motion_OculusTouch_A;
						}
						else if (pDevice->GetClass() == eHmdClass_OpenVR)
						{
							triggerKeyId = eKI_Motion_OpenVR_Trigger;
							releaseHeldItemKeyId = eKI_Motion_OpenVR_TouchPadBtn;
						}

						Vec3 linearVelocity, linearAcceleration;
						Vec3 angularVelocity, angularAcceleration;

						const HmdTrackingState& trackingState = pDevice->GetController()->GetLocalTrackingState(hand.controllerId);
						if (trackingState.CheckStatusFlags(EHmdStatus::eHmdStatus_OrientationTracked))
						{
							Matrix34 localControllerTransform = Matrix34::Create(Vec3(1.f), trackingState.pose.orientation, trackingState.pose.position);
							localControllerTransform.m23 = trackingState.pose.position.z;

							m_pEntity->SetSlotLocalTM(hand.slotId, localControllerTransform);

							const Matrix34& worldTransform = m_pEntity->GetWorldTM();

							linearVelocity = worldTransform.TransformVector(trackingState.pose.linearVelocity);
							angularVelocity = worldTransform.TransformVector(trackingState.pose.angularVelocity);

							linearAcceleration = worldTransform.TransformVector(trackingState.pose.linearAcceleration);
							angularAcceleration = worldTransform.TransformVector(trackingState.pose.angularAcceleration);
						}
						else
						{
							linearVelocity = ZERO;
							angularVelocity = ZERO;
							linearAcceleration = ZERO;
							angularAcceleration = ZERO;
						}

						constexpr float triggerHoldThreshold = 0.95f;
						bool isHoldingTrigger = pDevice->GetController()->GetTriggerValue(hand.controllerId, triggerKeyId) > triggerHoldThreshold;

						if (!isHoldingTrigger && hand.holdingTrigger)
						{
							hand.holdingTrigger = false;
						}

						if (isHoldingTrigger && !hand.holdingTrigger && hand.heldEntityId == INVALID_ENTITYID)
						{
							hand.holdingTrigger = true;

							SPickableEntity pickableEntity = FindPickableEntity(hand);
							if (pickableEntity.pEntity != nullptr)
							{
								PickUpObject(hand, pickableEntity);
							}
						}
						else if ((m_bReleaseWithPressKey && !isHoldingTrigger) || pDevice->GetController()->IsButtonPressed(hand.controllerId, releaseHeldItemKeyId))
						{
							ReleaseObject(hand, linearVelocity, angularVelocity);
						}
						else if (hand.heldEntityId != INVALID_ENTITYID)
						{
							if (IEntity* pHeldEntity = gEnv->pEntitySystem->GetEntity(hand.heldEntityId))
							{
								pe_params_pos pp;
								pp.pos = m_pEntity->GetSlotWorldTM(hand.slotId).GetTranslation();
								pp.q = IDENTITY;
								hand.pAttachedEntity->SetParams(&pp);
								/*float t = gEnv->pTimer->GetCurrTime();
								if (t > m_timeMove + gEnv->pTimer->GetFrameTime() * 0.5f)
								{
									pe_action_set_velocity asv;
									asv.v = (pp.pos - m_lastAttachPos) / (t - m_timeMove);
									m_pEntAttach->Action(&asv);
									m_lastAttachPos = pp.pos;
									m_timeMove = t;
								}*/
							}
						}
					}
				}
			}

			uint64 CInteractionComponent::GetEventMask() const
			{
				return ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE);
			}

			void CInteractionComponent::ReleaseObject(SHand& hand, const Vec3& linearVelocity, const Vec3& angularVelocity)
			{
				if (IEntity* pHeldEntity = gEnv->pEntitySystem->GetEntity(hand.heldEntityId))
				{
					if (IPhysicalEntity* pHeldPhysicalEntity = pHeldEntity->GetPhysicalEntity())
					{
						pe_action_update_constraint auc;
						auc.idConstraint = hand.constraintId;
						auc.bRemove = 1;
						pHeldPhysicalEntity->Action(&auc);

						pe_action_set_velocity setVel;
						setVel.v = linearVelocity;
						setVel.w = angularVelocity;
						pHeldPhysicalEntity->Action(&setVel);
					}

					DynArray<IInteractableComponent*> usableEntityComponents;
					pHeldEntity->GetAllComponents<IInteractableComponent>(usableEntityComponents);
					for (IInteractableComponent* pUsableEntityComponent : usableEntityComponents)
					{
						pUsableEntityComponent->OnReleased();
					}
				}

				// Re-enable render of the default hand slot
				m_pEntity->SetSlotFlags(hand.slotId, ENTITY_SLOT_RENDER);
				hand.heldEntityId = INVALID_ENTITYID;
			}

			void CInteractionComponent::PickUpObject(SHand& hand, const SPickableEntity& pickableEntity)
			{
				// Disable render of the held item
				m_pEntity->SetSlotFlags(hand.slotId, 0);
				hand.heldEntityId = pickableEntity.pEntity->GetId();
				
				pe_params_pos pp;
				pp.pos = m_pEntity->GetSlotWorldTM(hand.slotId).GetTranslation();

				pe_status_dynamics dynStatus;
				pickableEntity.pPhysicalEntity->GetStatus(&dynStatus);

				pe_action_add_constraint aac;
				aac.pt[0] = pp.pos;
				aac.flags = constraint_no_tears | constraint_no_enforcement;
				aac.partid[0] = pickableEntity.partId;
				aac.maxPullForce = dynStatus.mass * 100;
				aac.maxBendTorque = dynStatus.mass * 1000;
				aac.hardnessLin = 5;
				aac.pBuddy = hand.pAttachedEntity;
				pp.q = IDENTITY;
				hand.constraintId = pickableEntity.pPhysicalEntity->Action(&aac, (1 - hand.pAttachedEntity->SetParams(&pp)) >> 31);

				DynArray<IInteractableComponent*> usableEntityComponents;
				pickableEntity.pEntity->GetAllComponents<IInteractableComponent>(usableEntityComponents);
				for (IInteractableComponent* pUsableEntityComponent : usableEntityComponents)
				{
					pUsableEntityComponent->OnPickedUp(hand.controllerId);
				}
			}

			CInteractionComponent::SPickableEntity CInteractionComponent::FindPickableEntity(const SHand& hand)
			{
				Matrix34 handTransformation = m_pEntity->GetSlotWorldTM(hand.slotId);
				IStatObj* pHandGeometry = m_pEntity->GetStatObj(hand.slotId);
				if (pHandGeometry == nullptr)
				{
					return SPickableEntity();
				}

				AABB searchBox = pHandGeometry->GetAABB();
				searchBox = AABB::CreateTransformedAABB(handTransformation, searchBox);

				IPhysicalEntity** ppEntityList = nullptr;
				int entityCount = gEnv->pPhysicalWorld->GetEntitiesInBox(searchBox.min, searchBox.max, ppEntityList, ent_sleeping_rigid | ent_rigid);

				std::array<IPhysicalEntity*, 1> skipEntities = { { m_pEntity->GetPhysicalEntity() } };

				std::vector<SPickableEntity> usableEntities;
				usableEntities.resize(entityCount);

				// Remove invalid entities for an early out to avoid intersection
				for (int iEntity = 0; iEntity < entityCount;)
				{
					bool bValid = false;

					IEntity* pUsableEntity = gEnv->pEntitySystem->GetEntityFromPhysics(ppEntityList[iEntity]);

					if (pUsableEntity != nullptr)
					{
						if (ppEntityList[iEntity]->GetType() == PE_RIGID)
						{
							pe_status_dynamics dynStatus;
							if ((ppEntityList[iEntity]->GetStatus(&dynStatus) && dynStatus.mass > 0.f))
							{
								usableEntities[iEntity].pEntity = pUsableEntity;
								usableEntities[iEntity].pPhysicalEntity = ppEntityList[iEntity];
							}
						}

						bValid = usableEntities[iEntity].pEntity != nullptr;
					}

					if (!bValid || stl::find(skipEntities, ppEntityList[iEntity]))
					{
						// Steal the last element
						if (iEntity + 1 < entityCount)
						{
							ppEntityList[iEntity] = ppEntityList[entityCount - 1];
						}

						entityCount--;
					}
					else
					{
						++iEntity;
					}
				}

				pe_status_nparts numParts;

				geom_world_data gwd[2];
				geom_contact* pContacts;

				intersection_params intersectionParams;
				intersectionParams.bSweepTest = 0;
				intersectionParams.time_interval = 1.f;
				intersectionParams.bKeepPrevContacts = 0;
				intersectionParams.bNoAreaContacts = 1;

				real bestContact = std::numeric_limits<real>::max();
				decltype(usableEntities)::iterator bestEntityIt = usableEntities.end();

				for (int iUsableEntity = 0; iUsableEntity < entityCount; ++iUsableEntity)
				{
					int numEntityParts = ppEntityList[iUsableEntity]->GetStatus(&numParts);
					for (int iUsableEntityPart = 0; iUsableEntityPart < numEntityParts; ++iUsableEntityPart)
					{
						pe_status_pos partStatus;
						partStatus.flags &= ~status_local;
						partStatus.ipart = iUsableEntityPart;
						ppEntityList[iUsableEntity]->GetStatus(&partStatus);

						// Hand coordinates
						gwd[0].offset = handTransformation.GetTranslation();
						gwd[0].v = ZERO;
						gwd[0].R = Matrix33(handTransformation);
						gwd[0].scale = handTransformation.GetUniformScale();

						// Liftable entity coordinates
						gwd[1].offset = partStatus.pos;
						gwd[1].R = Matrix33(partStatus.q);
						gwd[1].scale = partStatus.scale;

						int numContacts = hand.pPhysicsGeometry->Intersect(partStatus.pGeom, gwd, gwd + 1, &intersectionParams, pContacts);

						for (int iContact = 0; iContact < numContacts; ++iContact)
						{
							if (pContacts[iContact].t >= bestContact)
								continue;

							bestContact = pContacts[iContact].t;
							bestEntityIt = usableEntities.begin() + iUsableEntity;
							bestEntityIt->partId = iUsableEntityPart;
						}
					}
				}

				return bestEntityIt != usableEntities.end() ? *bestEntityIt : SPickableEntity();
			}
		}
	}
}