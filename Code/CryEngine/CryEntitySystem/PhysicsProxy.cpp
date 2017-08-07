// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   PhysicsProxy.cpp
//  Version:     v1.00
//  Created:     25/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "PhysicsProxy.h"
#include "Entity.h"
#include "EntitySlot.h"
#include "EntitySystem.h"
#include <CryNetwork/ISerialize.h>
#include "AffineParts.h"
#include <Cry3DEngine/CGF/CryHeaders.h>

#include <CrySystem/TimeValue.h>
#include <CryAnimation/ICryAnimation.h>
#include <Cry3DEngine/IIndexedMesh.h>
#include <CryNetwork/INetwork.h>
#include <Cry3DEngine/IGeomCache.h>

#include <CrySystem/ICodeCheckpointMgr.h>

#define MAX_MASS_TO_RESTORE_VELOCITY 500

const int PHYS_ENTITY_SUSPEND = 1;
const int PHYS_ENTITY_RESTORE = 2;

#define PHYS_DEFAULT_DENSITY 1000.0f

#if !defined(_RELEASE)
namespace
{
static int ValidateSetParams(const EventPhys* pEvent)
{
	const EventPhysStateChange* event = static_cast<const EventPhysStateChange*>(pEvent);
	if ((event->iSimClass[1] == SC_SLEEPING_RIGID || event->iSimClass[1] == SC_ACTIVE_RIGID) && event->pEntity->GetType() == PE_ARTICULATED)
	{
		pe_status_dynamics dynamics;
		pe_params_articulated_body articulated_body;
		if (event->pEntity->GetParams(&articulated_body) && event->pEntity->GetStatus(&dynamics) && articulated_body.pHost && dynamics.mass != 0.f)
		{
			CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_ERROR_DBGBRK, "articulated entity simulated as rigid while still attached to a host");
		}
	}
	return 1;
}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::EnableValidation()
{
	if (IPhysicalWorld* pPhysWorld = gEnv->pPhysicalWorld)
	{
		pPhysWorld->AddEventClient(EventPhysStateChange::id, ValidateSetParams, 0, 1e14f);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::DisableValidation()
{
	if (IPhysicalWorld* pPhysWorld = gEnv->pPhysicalWorld)
	{
		pPhysWorld->RemoveEventClient(EventPhysStateChange::id, ValidateSetParams, 0);
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////
CEntityPhysics::CEntityPhysics()
{
}

//////////////////////////////////////////////////////////////////////////
CEntityPhysics::~CEntityPhysics()
{
	ReleasePhysicalEntity();
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::ReleasePhysicalEntity()
{
	// Delete physical entity from physical world.
	if (m_pPhysicalEntity)
	{
		m_pPhysicalEntity->Release();
		PhysicalWorld()->DestroyPhysicalEntity(m_pPhysicalEntity);
		m_pPhysicalEntity = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////

int CEntityPhysics::GetPhysAttachId()
{
	return m_pEntity->m_hierarchy.attachId;
}

int CEntityPhysics::GetPartId0(int nSlot)
{
	int id = nSlot;
	int nLevels, nBits;
	EntityPhysicsUtils::ParsePartId(id, nLevels, nBits);
	if (m_pEntity->m_hierarchy.attachId >= 0)
		id |= 1 << 30 | m_pEntity->m_hierarchy.attachId << nBits;
	return id;
}

int AttachPartId(int idPart, int* attachIdRemap)
{
	int nLevels, nBits;
	EntityPhysicsUtils::ParsePartId(idPart, nLevels, nBits);
	return 1 << 30 | (nLevels - 1) << 28 | idPart & (1 << nBits) - 1 | attachIdRemap[idPart & 1 << 30 ? idPart >> nBits & EntityPhysicsUtils::PARTID_MAX_ATTACHMENTS - 1 : EntityPhysicsUtils::PARTID_MAX_ATTACHMENTS] << nBits;
}

int DetachPartId(int idPart, int* attachIdRemap)
{
	int nLevels, nBits;
	EntityPhysicsUtils::ParsePartId(idPart, nLevels, nBits);
	int idAttach = attachIdRemap[idPart >> nBits & EntityPhysicsUtils::PARTID_MAX_ATTACHMENTS - 1];
	if (!(idPart & 1 << 30 && idAttach >= 0))
		return -1;
	return idAttach & 1 << 30 ^ 1 << 30 | (nLevels - 1) << 28 | idAttach << nBits | idPart & (1 << nBits) - 1;
}

inline int AllocAttachId(attachMask& usedMask)
{
	attachMask mask1 = (usedMask ^ usedMask + 1) + 1;
	int id = !!mask1 ? ilog2(mask1) - 1 : EntityPhysicsUtils::PARTID_MAX_ATTACHMENTS - 1;
	usedMask |= attachMask1 << id;
	return id;
}

void CEntityPhysics::RemapChildAttachIds(CEntity* pent, attachMask& idmaskSrc, attachMask& idmaskDst, int* idmap)
{
	if (pent->m_hierarchy.pParent)
		for (uint i = 0; i < pent->m_hierarchy.childs.size(); i++)
		{
			int idOld = pent->m_hierarchy.childs[i]->m_hierarchy.attachId;
			if (idOld < 0)
				continue;
			int idNew = idmap[idOld] = AllocAttachId(idmaskDst);
			idmaskDst |= attachMask1 << idNew;
			idmaskSrc &= ~(attachMask1 << idOld);
			pent->m_hierarchy.childs[i]->m_hierarchy.attachId = idNew;
			RemapChildAttachIds(pent->m_hierarchy.childs[i], idmaskSrc, idmaskDst, idmap);
		}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::UpdateParamsFromRenderMaterial(int nSlot, IPhysicalEntity* pPhysEntity)
{
	pe_params_part ppart;

	if (nSlot >= 0)
		ppart.partid = GetPartId0(nSlot);

	IMaterial* pMtl = m_pEntity->GetMaterial();

	CEntityRender* pIEntityRender = m_pEntity->GetEntityRender();
	if (pIEntityRender && nSlot >= 0)
	{
		pMtl = pIEntityRender->GetRenderMaterial(nSlot);
	}

	if (pMtl)
	{
		// Assign custom material to physics.
		int surfaceTypesId[MAX_SUB_MATERIALS];
		memset(surfaceTypesId, 0, sizeof(surfaceTypesId));
		int numIds = pMtl->FillSurfaceTypeIds(surfaceTypesId);

		// Assign physical materials mapping table for this material.
		ppart.nMats = numIds;
		ppart.pMatMapping = surfaceTypesId;

		bool bCollisionProxy = false;
		bool bRaycastProxy = false;

		if (pMtl->GetFlags() & MTL_FLAG_COLLISION_PROXY)
			bCollisionProxy = true;

		if (pMtl->GetFlags() & MTL_FLAG_RAYCAST_PROXY)
			bRaycastProxy = true;

		if (ISurfaceType* pSurf = pMtl->GetSurfaceType())
		{
			if (pSurf->GetPhyscalParams().collType >= 0)
			{
				ppart.flagsAND &= ~(geom_collides | geom_floats);
				ppart.flagsOR |= pSurf->GetPhyscalParams().collType & geom_collides;
			}
		}
		if (bRaycastProxy || bCollisionProxy)
		{
			ppart.flagsAND &= ~((bRaycastProxy ? geom_colltype_solid : 0) | (bCollisionProxy ? geom_colltype_ray : 0));
			ppart.flagsOR |= (!bRaycastProxy ? geom_colltype_solid : 0) | (!bCollisionProxy ? geom_colltype_ray : 0);
		}
	}

	pPhysEntity->SetParams(&ppart);
}

CEntity* GetAdam(CEntity* pAdam, Matrix34& mtx)
{
	CEntity* pParent;
	mtx.SetIdentity();
	while (pParent = (CEntity*)pAdam->GetParent())
	{
		mtx = Matrix34::CreateScale(pParent->GetScale()) * pAdam->GetLocalTM() * mtx;
		pAdam = pParent;
	}
	return pAdam;
}

void CEntityPhysics::ProcessEvent(SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_XFORM:
		OnEntityXForm(event);
		if (m_pPhysicalEntity && m_pPhysicalEntity->GetType() <= PE_RIGID && m_pEntity->m_hierarchy.attachId >= 0)
		{
			CEntity* pAdam;
			CEntityPhysics* pAdamProxy;
			pe_status_pos sp;
			sp.flags = status_local;
			pe_params_bbox pbb;
			Matrix34 mtxPart, mtxLoc;
			pe_params_part pp;
			pp.pMtx3x4 = &mtxPart;
			AABB bbox;
			bbox.Reset();
			if ((pAdam = (CEntity*)GetAdam(m_pEntity, mtxLoc)) != m_pEntity && (pAdamProxy = pAdam->GetPhysicalProxy()) &&
			    pAdamProxy->m_pPhysicalEntity && pAdamProxy->m_pPhysicalEntity->GetType() <= PE_WHEELEDVEHICLE)
			{
				for (sp.ipart = 0; pAdamProxy->m_pPhysicalEntity->GetStatus(&sp); sp.ipart++)
				{
					int nLevels, nBits;
					EntityPhysicsUtils::ParsePartId(sp.partid, nLevels, nBits);
					if (sp.partid & 1 << 30 && (sp.partid >> nBits & EntityPhysicsUtils::PARTID_MAX_ATTACHMENTS - 1) == m_pEntity->m_hierarchy.attachId)
					{
						bbox.Add(sp.BBox[0] + sp.pos), bbox.Add(sp.BBox[1] + sp.pos);
						if (!CheckFlags(FLAG_IGNORE_XFORM_EVENT) && !pAdamProxy->CheckFlags(FLAG_IGNORE_XFORM_EVENT))
						{
							pp.partid = sp.partid;
							mtxPart = mtxLoc * m_pEntity->GetSlotLocalTM(sp.partid & EntityPhysicsUtils::PARTID_MAX_SLOTS - 1, true);
							pAdamProxy->m_pPhysicalEntity->SetParams(&pp);
						}
					}
				}
				if (bbox.max.x > bbox.min.x)
				{
					pbb.BBox[0] = bbox.min;
					pbb.BBox[1] = bbox.max;
					m_pPhysicalEntity->SetParams(&pbb);
				}
			}
		}
		break;
	case ENTITY_EVENT_HIDE:
	case ENTITY_EVENT_INVISIBLE:
		if (event.nParam[0] != 1)
		{
			if (!gEnv->IsEditor() && m_pPhysicalEntity && m_pPhysicalEntity->GetType() == PE_SOFT)
			{
				ReleasePhysicalEntity();
				SetFlags(GetFlags() | (FLAG_PHYSICS_REMOVED));
			}
			else
				EnablePhysics(false);
		}
		break;
	case ENTITY_EVENT_UNHIDE:
	case ENTITY_EVENT_VISIBLE:
		if (event.nParam[0] != 1)
		{
			if (!gEnv->IsEditor() && !m_pPhysicalEntity && (GetFlags() & (FLAG_PHYSICS_REMOVED)))
			{
				SEntityEvent e;
				e.event = ENTITY_EVENT_RESET;
				GetEntity()->GetScriptProxy()->ProcessEvent(e);
				SetFlags(GetFlags() & (~FLAG_PHYSICS_REMOVED));
			}
			else
				EnablePhysics(true);
			if (event.event == ENTITY_EVENT_UNHIDE)
			{
				// Force xform update to move physics proxy. Physics ignores updates while hidden.
				SEntityEvent event(ENTITY_EVENT_XFORM);
				event.nParam[0] = ENTITY_XFORM_POS | ENTITY_XFORM_ROT | ENTITY_XFORM_SCL;
				OnEntityXForm(event);
			}
		}
		break;
	case ENTITY_EVENT_DONE:
		{
			SEntityPhysicalizeParams epp;
			epp.type = PE_NONE;
			Physicalize(epp);

			// Disabling the physics on 'done' prevents any outstanding physics CBs being executed.
			SetFlags(FLAG_PHYSICS_DISABLED);
			break;
		}
	case ENTITY_EVENT_ATTACH:
		{
			CEntity* pChild, * pAdam;
			CEntityPhysics* pChildProxy, * pAdamProxy;
			pe_action_move_parts amp;
			pChild = (CEntity*)m_pEntity->GetEntitySystem()->GetEntity((EntityId)event.nParam[0]);
			pAdam = (CEntity*)GetAdam(pChild, amp.mtxRel);
			if (pChild && pChild->GetParentBindingType() != CEntity::EBindingType::eBT_LocalSim)
			{
				int i;
				for (i = pChild->GetSlotCount() - 1; i >= 0 && (!pChild->GetCharacter(i) || pChild->GetCharacter(i)->GetObjectType() != CGA); i--)
					;

				if (i < 0 &&
				    (pChildProxy = pChild->GetPhysicalProxy()) && pChildProxy->m_pPhysicalEntity && pChildProxy->m_pPhysicalEntity->GetType() <= PE_RIGID &&
				    pAdam && (pAdamProxy = pAdam->GetPhysicalProxy()) && pAdamProxy->m_pPhysicalEntity &&
				    (i = pAdamProxy->m_pPhysicalEntity->GetType()) <= PE_WHEELEDVEHICLE && i != PE_STATIC)
				{
					// Move pChild (and all of its children) to pAdam
					int idmap[EntityPhysicsUtils::PARTID_MAX_ATTACHMENTS + 1];
					memset(idmap, -1, sizeof(idmap));
					pChild->m_hierarchy.attachId = idmap[EntityPhysicsUtils::PARTID_MAX_ATTACHMENTS] = AllocAttachId(pAdam->m_hierarchy.childrenAttachIds);
					RemapChildAttachIds(pChild, pChild->m_hierarchy.childrenAttachIds, pAdam->m_hierarchy.childrenAttachIds, idmap);
					amp.pTarget = pAdamProxy->m_pPhysicalEntity;
					amp.MapPartId = AttachPartId;
					amp.auxData = idmap;
					amp.szAuxData = EntityPhysicsUtils::PARTID_MAX_ATTACHMENTS + 1;
					pChildProxy->m_pPhysicalEntity->Action(&amp);
				}
			}
		} break;
	case ENTITY_EVENT_DETACH:
		{
			CEntity* pChild, * pAdam;
			CEntityPhysics* pChildProxy, * pAdamProxy;
			pChild = (CEntity*)m_pEntity->GetEntitySystem()->GetEntity((EntityId)event.nParam[0]);
			pAdam = (CEntity*)m_pEntity->GetAdam();
			if (pChild && pChild->GetParentBindingType() != CEntity::EBindingType::eBT_LocalSim)
			{
				if ((pChildProxy = pChild->GetPhysicalProxy()) && pChildProxy->m_pPhysicalEntity &&
				    pAdam && (pAdamProxy = pAdam->GetPhysicalProxy()) && pAdamProxy->m_pPhysicalEntity &&
				    pChild->m_hierarchy.attachId >= 0)
				{
					Matrix34 childWorldTM = pChild->GetWorldTM();
					childWorldTM.OrthonormalizeFast();
					pe_action_move_parts amp;
					int idmap[EntityPhysicsUtils::PARTID_MAX_ATTACHMENTS + 1];
					memset(idmap, -1, sizeof(idmap));
					idmap[pChild->m_hierarchy.attachId] = 1 << 30;
					RemapChildAttachIds(pChild, pAdam->m_hierarchy.childrenAttachIds, pChild->m_hierarchy.childrenAttachIds, idmap);
					amp.mtxRel = childWorldTM.GetInverted() * pAdam->GetWorldTM();
					amp.pTarget = pChildProxy->m_pPhysicalEntity;
					amp.MapPartId = DetachPartId;
					amp.auxData = idmap;
					amp.szAuxData = EntityPhysicsUtils::PARTID_MAX_ATTACHMENTS + 1;
					pAdamProxy->m_pPhysicalEntity->Action(&amp);
					pAdam->m_hierarchy.childrenAttachIds &= ~(attachMask1 << pChild->m_hierarchy.attachId);
					pChild->m_hierarchy.attachId = -1;
				}
			}
		} break;
	case ENTITY_EVENT_COLLISION:
		if (event.nParam[1] == 1)
		{
			EventPhysCollision* pColl = (EventPhysCollision*)event.nParam[0];
			int islot = EntityPhysicsUtils::GetSlotIdx(pColl->partid[1]);
			pe_params_bbox pbb;
			Vec3 sz;
			float r, strength;
			Matrix34 mtx;
			IStatObj* pNewObj;
			CEntitySlot* pSlot = m_pEntity->GetSlot(islot);
			if (!pSlot || !pSlot->GetStatObj())
				pSlot = m_pEntity->GetSlot(islot = 0);
			pColl->pEntity[0]->GetParams(&pbb);
			sz = pbb.BBox[1] - pbb.BBox[0];
			r = min(0.5f, max(0.05f, (sz.x + sz.y + sz.z) * (1.0f / 3)));
			strength = min(r, pColl->vloc[0].len() * (1.0f / 50));
			mtx = m_pEntity->GetSlotWorldTM(islot).GetInverted();

			if (pSlot && pSlot->GetStatObj())
			{
				if ((pNewObj = pSlot->GetStatObj()->DeformMorph(mtx * pColl->pt, r, strength)) != pSlot->GetStatObj())
					m_pEntity->SetStatObj(pNewObj, islot, true);
			}
		}
		break;
	case ENTITY_EVENT_PHYS_BREAK:
		{
			EventPhysJointBroken* pepjb = (EventPhysJointBroken*)event.nParam[0];

			// Counter for feature test setup
			CODECHECKPOINT(physics_on_tree_joint_break);

			if (pepjb && !pepjb->bJoint && pepjb->idJoint == 1000000)
			{
				// special handling for splinters on breakable trees
				CEntity* pent, * pentNew;
				IStatObj* pStatObj;
				int i, j, iop;
				float dist, mindist;
				for (iop = 0; iop < 2; iop++)
					if (!is_unused(pepjb->pNewEntity[iop]) && (pent = (CEntity*)pepjb->pEntity[iop]->GetForeignData(PHYS_FOREIGN_ID_ENTITY)))
					{
						for (i = pent->GetSlotCount() - 1, j = -1, mindist = 1E10f; i > 0; i--)
							if ((pStatObj = pent->GetStatObj(i | ENTITY_SLOT_ACTUAL)) && !(pStatObj->GetFlags() & STATIC_OBJECT_GENERATED) &&
							    !pStatObj->GetPhysGeom() && (dist = (pent->GetSlotWorldTM(i).GetTranslation() - pepjb->pt).len2()) < mindist)
								mindist = dist, j = i;
						if (j >= 0)
						{
							pStatObj = pent->GetStatObj(j | ENTITY_SLOT_ACTUAL);
							if (pepjb->pNewEntity[iop] && (pentNew = (CEntity*)pepjb->pNewEntity[iop]->GetForeignData(PHYS_FOREIGN_ID_ENTITY)))
								pentNew->SetSlotLocalTM(pentNew->SetStatObj(pStatObj, -1, false), pent->GetSlotLocalTM(j | ENTITY_SLOT_ACTUAL, false));
							else
							{
								IStatObj::SSubObject* pSubObj = pStatObj->FindSubObject("splinters_cloud");
								if (pSubObj)
								{
									IParticleEffect* pEffect = gEnv->pParticleManager->FindEffect(pSubObj->properties);
									if (pEffect)
										pEffect->Spawn(true, IParticleEffect::ParticleLoc(pepjb->pt));
								}
							}
							pent->FreeSlot(j);
						}
					}
			}
		} break;
	case ENTITY_EVENT_RENDER_VISIBILITY_CHANGE:
		{
			bool bVisible = event.nParam[0] != 0;

			AwakeOnRender(bVisible);
		}
		break;
	case ENTITY_EVENT_MATERIAL:
		if (IMaterial* pMtl = (IMaterial*)event.nParam[0])
		{
			IEntity* pAdam = m_pEntity->GetAdam();
			if (m_pEntity->m_hierarchy.attachId >= 0 && pAdam && pAdam != m_pEntity && pAdam->GetPhysics())
			{
				UpdateParamsFromRenderMaterial(0, pAdam->GetPhysics());
			}
			else if (m_pPhysicalEntity)
			{
				UpdateParamsFromRenderMaterial(-1, m_pPhysicalEntity);
			}
		}
		break;
	}
}

void CEntityPhysics::AwakeOnRender(bool bRender)
{
	if (bRender)
	{
		if (m_nFlags & FLAG_PHYS_AWAKE_WHEN_VISIBLE)
		{
			pe_action_awake aa;
			if (m_pPhysicalEntity &&
			    (GetEntity()->GetWorldPos() - GetISystem()->GetViewCamera().GetPosition()).len2() < sqr(CVar::es_MaxPhysDist))
			{
				m_pPhysicalEntity->Action(&aa);
				m_pEntity->SetFlags(m_pEntity->GetFlags() & ~ENTITY_FLAG_SEND_RENDER_EVENT);
				m_nFlags &= ~FLAG_PHYS_AWAKE_WHEN_VISIBLE;
			}
		}
		else if (m_nFlags & FLAG_ATTACH_CLOTH_WHEN_VISIBLE && m_pPhysicalEntity)
		{
			for (int slot = 0; slot < m_pEntity->GetSlotCount(); slot++)
			{
				IRenderMesh* pRM;
				if (!m_pEntity->GetSlot(slot) || !m_pEntity->GetSlot(slot)->GetStatObj() || !(pRM = m_pEntity->GetSlot(slot)->GetStatObj()->GetRenderMesh()))
					continue;
				pe_params_foreign_data pfd;
				m_pPhysicalEntity->GetParams(&pfd);
				if (pfd.iForeignData == PHYS_FOREIGN_ID_USER)
				{
					IPhysicalEntity* pAttachTo = (IPhysicalEntity*)pfd.pForeignData;
					if (pAttachTo)
					{
						pe_status_pos sp;
						pAttachTo->GetStatus(&sp);
						pAttachTo->Release();
						if (1 << sp.iSimClass > ent_independent)
							pAttachTo = 0;
					}
					AttachSoftVtx(pRM, pAttachTo, pfd.iForeignFlags);
				}
				pfd.pForeignData = m_pEntity;
				pfd.iForeignData = PHYS_FOREIGN_ID_ENTITY;
				pfd.iForeignFlags = 0;
				m_pPhysicalEntity->SetParams(&pfd);
				m_pEntity->SetFlags(m_pEntity->GetFlags() & ~ENTITY_FLAG_SEND_RENDER_EVENT);
				m_nFlags &= ~FLAG_ATTACH_CLOTH_WHEN_VISIBLE;
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::SetActive(bool bActive) 
{
	if(bActive)
	{
		m_nFlags |= FLAG_ACTIVE;
	}
	else
	{
		m_nFlags &= ~FLAG_ACTIVE;
	}

	// Add / remove from the active entity list if necessary
	m_pEntity->ActivateEntityIfNecessary();
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPhysics::NeedNetworkSerialize()
{
	if (m_pPhysicalEntity)
	{
		return true;

	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::Serialize(TSerialize ser)
{
	if (ser.BeginOptionalGroup("PhysicsProxy", (ser.GetSerializationTarget() == eST_Network ? ((m_nFlags & FLAG_DISABLE_ENT_SERIALIZATION) == 0) : NeedNetworkSerialize())))
	{
		if (m_pPhysicalEntity)
		{
			if (ser.IsReading())
			{
				m_pPhysicalEntity->SetStateFromSnapshot(ser, 0);
			}
			else
			{
				m_pPhysicalEntity->GetStateSnapshot(ser);
			}
		}
		else
		{
			CRY_ASSERT_MESSAGE(ser.GetSerializationTarget() != eST_Network,
				"Attempting to serialize physics without physical entity");
		}

		if (ser.GetSerializationTarget() != eST_Network) // no folieage over network for now.
		{
			CEntitySlot* pSlot = m_pEntity->GetSlot(0);
			if (IFoliage* pFoliage = GetFoliage(0))
				pFoliage->Serialize(ser);

			IPhysicalEntity* pCharPhys;
			// character physics is equal to the physproxy's physics only for ragdolls
			bool bSerializableRopes =
			  pSlot && pSlot->GetCharacter() && (!(pCharPhys = pSlot->GetCharacter()->GetISkeletonPose()->GetCharacterPhysics()) || pCharPhys == m_pPhysicalEntity);
			if (bSerializableRopes && ser.BeginOptionalGroup("SerializableRopes", bSerializableRopes))
			{
				for (int i = 0; pSlot->GetCharacter()->GetISkeletonPose()->GetCharacterPhysics(i); i++)
				{
					if (ser.BeginOptionalGroup("CharRope", true))
					{
						if (ser.IsReading())
							pSlot->GetCharacter()->GetISkeletonPose()->GetCharacterPhysics(i)->SetStateFromSnapshot(ser);
						else
							pSlot->GetCharacter()->GetISkeletonPose()->GetCharacterPhysics(i)->GetStateSnapshot(ser);
						ser.EndGroup();
					}
				}
				ser.EndGroup();
			}
			if (bSerializableRopes && ser.IsReading())
				m_pEntity->ActivateForNumUpdates(2);
		}

		ser.EndGroup(); //PhysicsProxy
	}
}

void CEntityPhysics::SerializeTyped(TSerialize ser, int type, int flags)
{
	NET_PROFILE_SCOPE("PhysicalProxy", ser.IsReading());
	ser.BeginGroup("PhysicsProxy");
	if (m_pPhysicalEntity)
	{
		if (ser.IsReading())
		{
			m_pPhysicalEntity->SetStateFromTypedSnapshot(ser, type, flags);
		}
		else
		{
			m_pPhysicalEntity->GetStateSnapshot(ser);
		}
	}
	ser.EndGroup();
}

void CEntityPhysics::SerializeXML(XmlNodeRef& entityNode, bool bLoading)
{
	if (bLoading)
	{
		XmlNodeRef physicsState = entityNode->findChild("PhysicsState");
		if (physicsState)
			m_pEntity->SetPhysicsState(physicsState);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::OnEntityXForm(SEntityEvent& event)
{
	if (event.nParam[0] & ENTITY_XFORM_IGNORE_PHYSICS || 
		  event.nParam[0] & ENTITY_XFORM_FROM_PARENT && (
				m_pEntity->GetParentBindingType() == CEntity::EBindingType::eBT_LocalSim || 
				m_pPhysicalEntity && m_pPhysicalEntity->GetType() == PE_GRID))
		return;

	if (!CheckFlags(FLAG_IGNORE_XFORM_EVENT))
	{
		if (m_pPhysicalEntity)
		{
			if (m_pPhysicalEntity->GetType() == PE_SOFT && event.nParam[0] & ENTITY_XFORM_FROM_PARENT && gEnv->pPhysicalWorld->GetPhysVars()->lastTimeStep > 0)
				return;
			// This set flag prevents endless recursion of events recieved from physics.
			SetFlags(m_nFlags | FLAG_IGNORE_XFORM_EVENT);
			pe_params_pos ppos;

			bool bAnySet = false;
			if (event.nParam[0] & ENTITY_XFORM_POS) // Position changes.
			{
				ppos.pos = m_pEntity->GetWorldTM().GetTranslation();
				bAnySet = true;
			}
			if (event.nParam[0] & ENTITY_XFORM_ROT) // Rotation changes.
			{
				bAnySet = true;
				if (!m_pEntity->GetParent())
					ppos.q = m_pEntity->GetRotation();
				else
					ppos.pMtx3x4 = const_cast<Matrix34*>(&m_pEntity->GetWorldTM());
			}
			if (event.nParam[0] & ENTITY_XFORM_SCL) // Scale changes.
			{
				bAnySet = true;
				Vec3 s = m_pEntity->GetScale();
				if (!m_pEntity->GetParent() && sqr(s.x - s.y) + sqr(s.y - s.z) == 0.0f)
					ppos.scale = s.x;
				else
					ppos.pMtx3x4 = const_cast<Matrix34*>(&m_pEntity->GetWorldTM());
			}
			if (!bAnySet)
				ppos.pMtx3x4 = const_cast<Matrix34*>(&m_pEntity->GetWorldTM());

			if (m_pEntity->m_hierarchy.pParent && m_pEntity->GetParentBindingType() == CEntity::EBindingType::eBT_LocalSim)
			{
				ppos.pos = m_pEntity->GetPos();
				ppos.q = m_pEntity->GetRotation();
				ppos.pMtx3x4 = nullptr;
				ppos.pGridRefEnt = m_pEntity->m_hierarchy.pParent->GetPhysics();
			}

#ifdef SEG_WORLD
			if (event.nParam[1])
				ppos.bRecalcBounds |= 2;
#endif

			m_pPhysicalEntity->SetParams(&ppos);
			SetFlags(m_nFlags & (~FLAG_IGNORE_XFORM_EVENT));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::GetLocalBounds(AABB& bbox) const
{
	bbox.min.Set(0, 0, 0);
	bbox.max.Set(0, 0, 0);

	pe_status_pos pstate;
	if (m_pPhysicalEntity && m_pPhysicalEntity->GetStatus(&pstate))
	{
		bbox.min = pstate.BBox[0] / pstate.scale;
		bbox.max = pstate.BBox[1] / pstate.scale;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::GetWorldBounds(AABB& bbox) const
{
	pe_params_bbox pbb;
	if (m_pPhysicalEntity && m_pPhysicalEntity->GetParams(&pbb))
	{
		bbox.min = pbb.BBox[0];
		bbox.max = pbb.BBox[1];
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::EnablePhysics(bool bEnable)
{
	if (bEnable == !CheckFlags(FLAG_PHYSICS_DISABLED))
		return;

	if (bEnable)
	{
		SetFlags(GetFlags() & (~FLAG_PHYSICS_DISABLED));
	}
	else
	{
		SetFlags(GetFlags() | FLAG_PHYSICS_DISABLED);
	}

	if (!m_pPhysicalEntity)
		return;

	if (!bEnable && GetPhysAttachId() >= 0)
	{
		SEntityEvent eed(ENTITY_EVENT_DETACH);
		eed.nParam[0] = m_pEntity->GetId();
		ProcessEvent(eed);
	}

	if (bEnable)
	{
		PhysicalWorld()->DestroyPhysicalEntity(m_pPhysicalEntity, PHYS_ENTITY_RESTORE);
	}
	else
	{
		PhysicalWorld()->DestroyPhysicalEntity(m_pPhysicalEntity, PHYS_ENTITY_SUSPEND);
	}

	// Enable/Disable character physics characters.
	if (m_nFlags & FLAG_PHYS_CHARACTER)
	{
		int nSlot = (m_nFlags & CHARACTER_SLOT_MASK);
		ICharacterInstance* pCharacter = m_pEntity->GetCharacter(nSlot);
		IPhysicalEntity* pCharPhys;
		if (pCharacter)
		{
			pCharacter->GetISkeletonPose()->DestroyCharacterPhysics((bEnable) ? PHYS_ENTITY_RESTORE : PHYS_ENTITY_SUSPEND);
			if (bEnable && (pCharPhys = pCharacter->GetISkeletonPose()->GetCharacterPhysics()) && pCharPhys != m_pPhysicalEntity)
			{
				pe_params_articulated_body pab;
				pab.pHost = m_pPhysicalEntity;
				pCharPhys->SetParams(&pab);
			}
		}
	}

	if (bEnable && GetPhysAttachId() >= 0)
	{
		SEntityEvent eea(ENTITY_EVENT_ATTACH);
		eea.nParam[0] = m_pEntity->GetId();
		ProcessEvent(eea);
	}

	OnChangedPhysics(bEnable);
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPhysics::IsPhysicsEnabled() const
{
	return !CheckFlags(FLAG_PHYSICS_DISABLED);
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::Physicalize(SEntityPhysicalizeParams& params)
{
	ENTITY_PROFILER

	SEntityEvent eed(ENTITY_EVENT_DETACH);
	int i;
	for (i = 0; i < m_pEntity->GetChildCount(); i++)
	{
		eed.nParam[0] = m_pEntity->GetChild(i)->GetId();
		ProcessEvent(eed);
	}
	if (m_pEntity->GetParent())
	{
		eed.nParam[0] = m_pEntity->GetId();
		static_cast<CEntity*>(m_pEntity->GetParent())->GetPhysicalProxy()->ProcessEvent(eed);
	}

	pe_type previousPhysType = PE_NONE;
	Vec3 v(ZERO);

	pe_status_pos psp;
	pe_status_pos* prevStatus = nullptr;

	if (m_pPhysicalEntity)
	{
		if (m_pPhysicalEntity->GetStatus(&psp))
		{
			prevStatus = &psp;
		}

		previousPhysType = m_pPhysicalEntity->GetType();
		if (params.type == PE_ARTICULATED && previousPhysType == PE_LIVING)
		{
			pe_status_dynamics sd;
			m_pPhysicalEntity->GetStatus(&sd);
			m_pEntity->PrePhysicsActivate(true); // make sure we are completely in-sync with physics b4 ragdollizing
			v = sd.v;
			int hideOnly = params.fStiffnessScale > 0;
			if (hideOnly)
			{
				pe_params_foreign_data pfd;
				pfd.iForeignData = PHYS_FOREIGN_ID_RAGDOLL;
				pfd.pForeignData = m_pEntity->GetCharacter(0);
				m_pPhysicalEntity->SetParams(&pfd);
			}
			DestroyPhysicalEntity(false, hideOnly);
		}
		else if (params.type != PE_NONE)
		{
			DestroyPhysicalEntity();
		}
	}

	// Reset special flags.
	m_nFlags &= ~(FLAG_SYNC_CHARACTER | FLAG_PHYS_CHARACTER);

	switch (params.type)
	{
	case PE_RIGID:
	case PE_STATIC:
		PhysicalizeSimple(params);
		break;
	case PE_LIVING:
		PhysicalizeLiving(params);
		break;
	case PE_ARTICULATED:
		// Check for Special case, converting from living characters to the rag-doll.
		if (previousPhysType != PE_LIVING)
		{
			CreatePhysicalEntity(params);
			PhysicalizeGeomCache(params);
			PhysicalizeCharacter(params);

			// Apply joint velocities if desired
			if (params.bCopyJointVelocities)
			{
				// [*DavidR | 6/Oct/2010] This call is bound to be 80% redundant with the previous two, but it will make sure
				// the latest joint velocity preservation code gets used even when the previous character is not a living entity
				ConvertCharacterToRagdoll(params, v);
			}
		}
		else
		{
			ConvertCharacterToRagdoll(params, v);
		}
		break;
	case PE_PARTICLE:
		PhysicalizeParticle(params);
		break;
	case PE_SOFT:
		PhysicalizeSoft(params);
		break;
	case PE_WHEELEDVEHICLE:
		CreatePhysicalEntity(params);
		break;
	case PE_ROPE:
		if (params.nSlot >= 0)
			PhysicalizeCharacter(params);
		break;
	case PE_AREA:
		PhysicalizeArea(params);
		break;
	case PE_NONE:
		SetFlags(GetFlags() & ~FLAG_POS_EXTRAPOLATED);
		DestroyPhysicalEntity();
		break;
	default:
		DestroyPhysicalEntity();
		break;
	}

	// Set parameters if we have physical entity created
	if (m_pPhysicalEntity)
	{
		pe_params_flags pf;
		pf.flagsOR = pef_log_state_changes | pef_log_poststep;
#if !defined(RELEASE)
		if (m_pPhysicalEntity->GetType() == PE_ARTICULATED) pf.flagsOR |= pef_monitor_state_changes;
#endif
		m_pPhysicalEntity->SetParams(&pf);

		// Set physical entity Buoyancy.
		if (params.pBuoyancy)
			m_pPhysicalEntity->SetParams(params.pBuoyancy);

		// Assign physical materials mapping table for this material.
		UpdateParamsFromRenderMaterial(params.nSlot, m_pPhysicalEntity);
	}

	if (m_pPhysicalEntity)
	{
		pe_params_foreign_data pfd;
		pfd.iForeignData = PHYS_FOREIGN_ID_ENTITY;
		pfd.pForeignData = m_pEntity;
		if (!(m_nFlags & FLAG_ATTACH_CLOTH_WHEN_VISIBLE))
			m_pPhysicalEntity->SetParams(&pfd);

		if (CheckFlags(FLAG_PHYSICS_DISABLED))
		{
			// If Physics was disabled disable physics on new physical object now.
			PhysicalWorld()->DestroyPhysicalEntity(m_pPhysicalEntity, PHYS_ENTITY_SUSPEND);
		}
	}

	// Check if character physics disabled.
	if (CheckFlags(FLAG_PHYSICS_DISABLED | FLAG_PHYS_CHARACTER))
	{
		int nSlot = (m_nFlags & CHARACTER_SLOT_MASK);
		ICharacterInstance* pCharacter = m_pEntity->GetCharacter(nSlot);
		if (pCharacter && pCharacter->GetISkeletonPose()->GetCharacterPhysics())
			pCharacter->GetISkeletonPose()->DestroyCharacterPhysics(PHYS_ENTITY_SUSPEND);
	}

	SEntityEvent eea(ENTITY_EVENT_ATTACH);
	for (i = 0; i < m_pEntity->GetChildCount(); i++)
	{
		eea.nParam[0] = m_pEntity->GetChild(i)->GetId();
		ProcessEvent(eea);
	}
	if (m_pEntity->GetParent())
	{
		eea.nParam[0] = m_pEntity->GetId();
		static_cast<CEntity*>(m_pEntity->GetParent())->GetPhysicalProxy()->ProcessEvent(eed);
	}

	if (params.type != PE_NONE) // are actually physicalising
	{
		if (m_pEntity->IsHidden())
		{
			EnablePhysics(false);
		}
	}

	if (m_pPhysicalEntity)
	{
		TriggerEventIfStateChanged(m_pPhysicalEntity, prevStatus);
	}

	SEntityEvent event(ENTITY_EVENT_PHYSICAL_TYPE_CHANGED);
	m_pEntity->SendEvent(event);
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPhysics::TriggerEventIfStateChanged(IPhysicalEntity* pPhysEntity, const pe_status_pos* pPrevStatus) const
{
	pe_status_pos psp;
	if (!pPhysEntity->GetStatus(&psp))
		return false;

	const AABB currBBox(psp.BBox[0] + psp.pos, psp.BBox[1] + psp.pos);
	const int currSimClass = psp.iSimClass;

	AABB prevBBox(0.0f);
	int prevSimClass = -1;
	if (pPrevStatus)
	{
		prevBBox.min = pPrevStatus->BBox[0] + pPrevStatus->pos;
		prevBBox.max = pPrevStatus->BBox[1] + pPrevStatus->pos;
		prevSimClass = pPrevStatus->iSimClass;
	}

	//send event if bounding box or simulation class is updated
	if (!IsEquivalent(prevBBox, currBBox) || prevSimClass != currSimClass)
	{
		EventPhysStateChange event;
		event.pEntity = pPhysEntity;
		event.iForeignData = pPhysEntity->GetiForeignData();
		event.pForeignData = pPhysEntity->GetForeignData(event.iForeignData);
		event.timeIdle = 0.0f;
		event.iSimClass[0] = prevSimClass;
		event.iSimClass[1] = currSimClass;
		event.BBoxOld[0] = prevBBox.min;
		event.BBoxOld[1] = prevBBox.max;
		event.BBoxNew[0] = currBBox.min;
		event.BBoxNew[1] = currBBox.max;
		PhysicalWorld()->AddDeferredEvent(EventPhysStateChange::id, &event);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::CreatePhysicalEntity(SEntityPhysicalizeParams& params)
{
	// Initialize Rigid Body.
	pe_params_pos ppos;
	ppos.pMtx3x4 = const_cast<Matrix34*>(&m_pEntity->GetWorldTM());
	if (params.type == PE_ARTICULATED)
		ppos.iSimClass = 2;
	m_pPhysicalEntity = PhysicalWorld()->CreatePhysicalEntity(
	  (pe_type)params.type, &ppos, m_pEntity, PHYS_FOREIGN_ID_ENTITY, CEntitySystem::IdToHandle(m_pEntity->GetId()).GetIndex());
	m_pPhysicalEntity->AddRef();

	if (params.nFlagsOR != 0)
	{
		pe_params_flags pf;
		pf.flagsOR = params.nFlagsOR;
		m_pPhysicalEntity->SetParams(&pf);
	}

	if (params.pCar)
	{
		m_pPhysicalEntity->SetParams(params.pCar);
	}

	if (params.type == PE_ARTICULATED)
	{
		pe_params_articulated_body pab;
		pab.bGrounded = 0;
		pab.scaleBounceResponse = 1;
		pab.bCheckCollisions = 1;
		pab.bCollisionResp = 1;
		m_pPhysicalEntity->SetParams(&pab);
	}

	OnChangedPhysics(true);
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::AssignPhysicalEntity(IPhysicalEntity* pPhysEntity, int nSlot)
{
	assert(pPhysEntity);

	// Get Position form physical entity.
	(m_pPhysicalEntity = pPhysEntity)->AddRef();

	// This set flag prevents endless recursion of events recieved from physics.
	SetFlags(m_nFlags | FLAG_IGNORE_XFORM_EVENT);
	//
	Matrix34* mt = const_cast<Matrix34*>(nSlot >= 0 ? &m_pEntity->GetSlotWorldTM(nSlot) : &m_pEntity->GetWorldTM());
	pe_params_pos ppos;
	ppos.pos = mt->GetTranslation();

	Matrix34 normalised_mat = *mt;
	normalised_mat.OrthonormalizeFast();
	ppos.q = Quat(normalised_mat);
	//ppos.pMtx3x4 = const_cast<Matrix34*>(nSlot>=0 ? &m_pEntity->GetSlotWorldTM(nSlot) : &m_pEntity->GetWorldTM());
	m_pPhysicalEntity->SetParams(&ppos);

	//////////////////////////////////////////////////////////////////////////
	// Set user data of physical entity to be pointer to the entity.
	pe_params_foreign_data pfd;
	pfd.pForeignData = m_pEntity;
	pfd.iForeignData = PHYS_FOREIGN_ID_ENTITY;
	m_pPhysicalEntity->SetParams(&pfd);

	//////////////////////////////////////////////////////////////////////////
	// Enable update callbacks.
	pe_params_flags pf;
	pf.flagsOR = pef_log_state_changes | pef_log_poststep;
	m_pPhysicalEntity->SetParams(&pf);

	SetFlags(m_nFlags & (~FLAG_IGNORE_XFORM_EVENT));

}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::PhysicalizeSimple(SEntityPhysicalizeParams& params)
{
	CreatePhysicalEntity(params);

	if (params.nSlot < 0)
	{
		// Use all slots.
		for (int slot = 0; slot < m_pEntity->GetSlotCount(); slot++)
		{
			AddSlotGeometry(slot, params);
		}
	}
	else
	{
		// Use only specified slot.
		AddSlotGeometry(params.nSlot, params);
	}

	if ((params.type == PE_RIGID || params.type == PE_LIVING) && m_pPhysicalEntity)
	{
		// Rigid bodies should report at least one collision per frame.
		pe_simulation_params sp;
		sp.maxLoggedCollisions = 1;
		m_pPhysicalEntity->SetParams(&sp);
		pe_params_flags pf;
		pf.flagsOR = pef_log_collisions;
		m_pPhysicalEntity->SetParams(&pf);
	}
}

//////////////////////////////////////////////////////////////////////////
int CEntityPhysics::AddSlotGeometry(int nSlot, SEntityPhysicalizeParams& params, int bNoSubslots)
{
	CEntitySlot* pSlot = m_pEntity->GetSlot(nSlot);
	if (pSlot && pSlot->GetCharacter())
	{
		PhysicalizeCharacter(params);
		return -1;
	}
	else if (pSlot && pSlot->GetRenderNode() && pSlot->GetRenderNode()->GetRenderNodeType() == eERType_GeomCache)
	{
		PhysicalizeGeomCache(params);
		return -1;
	}

	IStatObj* pStatObj = m_pEntity->GetStatObj(nSlot | ENTITY_SLOT_ACTUAL & - bNoSubslots);
	if (!pStatObj)
		return -1;
	int physId = -1;

	pe_geomparams partpos;
	Matrix34 mtx;
	mtx.SetIdentity();
	Vec3 scale = m_pEntity->GetScale();

	//if (pSlot->HaveLocalMatrix())
	{
		mtx = m_pEntity->GetSlotLocalTM(nSlot | ENTITY_SLOT_ACTUAL & - bNoSubslots, false);
		mtx.SetTranslation(Diag33(scale) * mtx.GetTranslation());
		//scale *= mtx.GetColumn(0).len();
	}
	partpos.pMtx3x4 = &mtx;

	CEntity* pAdam = (CEntity*)m_pEntity->GetAdam();
	CEntityPhysics* pAdamProxy = pAdam->GetPhysicalProxy();
	if (!pAdamProxy || !pAdamProxy->m_pPhysicalEntity || GetPhysAttachId() < 0)
		pAdamProxy = this;
	if (pAdamProxy != this)
		mtx = m_pEntity->GetLocalTM() * mtx;
	else if (max(max(fabs_tpl(scale.x - 1.0f), fabs_tpl(scale.y - 1.0f)), fabs_tpl(scale.z - 1.0f)) > 0.0001f)
		mtx = mtx * Matrix33::CreateScale(scale);

	partpos.flags = geom_collides | geom_floats;
	partpos.flags &= params.nFlagsAND;
	partpos.flagsCollider &= params.nFlagsAND;
	partpos.density = params.density;
	partpos.mass = params.mass;
	return pStatObj->Physicalize(pAdamProxy->m_pPhysicalEntity, &partpos, pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND && !nSlot ? -1 : GetPartId0(nSlot), params.szPropsOverride);
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::RemoveSlotGeometry(int nSlot)
{
	CEntitySlot* pSlot = m_pEntity->GetSlot(nSlot);
	if (!pSlot || !m_pPhysicalEntity || pSlot->GetStatObj() && pSlot->GetStatObj()->GetFlags() & STATIC_OBJECT_COMPOUND)
		return;

	CEntityPhysics* pAdamProxy = static_cast<CEntity*>(m_pEntity->GetAdam())->GetPhysicalProxy();
	if (!pAdamProxy || !pAdamProxy->m_pPhysicalEntity || GetPhysAttachId() < 0)
		pAdamProxy = this;
	if (pSlot->GetStatObj() && pSlot->GetStatObj()->GetPhysGeom())
		pAdamProxy->m_pPhysicalEntity->RemoveGeometry(GetPartId0(nSlot));
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::MovePhysics(CEntityPhysics* dstPhysics)
{
	//--- Release any existing physics on the dest
	if (dstPhysics->m_pPhysicalEntity)
	{
		dstPhysics->m_pPhysicalEntity->Release();
		PhysicalWorld()->DestroyPhysicalEntity(dstPhysics->m_pPhysicalEntity);
	}

	dstPhysics->m_nFlags = m_nFlags;
	dstPhysics->m_pPhysicalEntity = m_pPhysicalEntity;

	m_pPhysicalEntity = NULL;
	m_nFlags &= ~(CHARACTER_SLOT_MASK | FLAG_PHYS_CHARACTER | FLAG_SYNC_CHARACTER);

	//////////////////////////////////////////////////////////////////////////
	// Set user data of physical entity to be pointer to the entity.
	pe_params_foreign_data pfd;
	pfd.pForeignData = dstPhysics->m_pEntity;
	pfd.iForeignData = PHYS_FOREIGN_ID_ENTITY;
	dstPhysics->m_pPhysicalEntity->SetParams(&pfd);
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::UpdateSlotGeometry(int nSlot, IStatObj* pStatObjNew, float mass, int bNoSubslots)
{
	//CEntityObject *pSlot = m_pEntity->GetSlot(nSlot);
	//if (!pSlot || !m_pPhysicalEntity || pSlot->pStatObj && !pSlot->pStatObj->GetRenderMesh())
	//	return;
	IStatObj* pStatObj = m_pEntity->GetStatObj(nSlot);
	if (!m_pPhysicalEntity)
		return;
	if (pStatObjNew && pStatObjNew->GetFlags() & STATIC_OBJECT_COMPOUND)
	{
		Matrix34 mtx = Matrix34::CreateScale(m_pEntity->GetScale()) * m_pEntity->GetSlotLocalTM(ENTITY_SLOT_ACTUAL | nSlot, false);
		pe_action_remove_all_parts tmp;
		m_pPhysicalEntity->Action(&tmp);
		pStatObjNew->PhysicalizeSubobjects(m_pPhysicalEntity, &mtx, mass, -1, nSlot ? GetPartId0(nSlot) : -1);
		return;
	}
	else if (pStatObj && pStatObj->GetPhysGeom())
	{
		pe_params_part pp;
		pp.partid = GetPartId0(nSlot);
		pp.pPhysGeom = pStatObj->GetPhysGeom();
		if (mass >= 0)
			pp.mass = mass;

		pe_status_pos sp;
		sp.partid = nSlot;
		if (m_pPhysicalEntity->GetStatus(&sp))
			m_pPhysicalEntity->SetParams(&pp);
		else
		{
			SEntityPhysicalizeParams epp;
			epp.mass = mass;
			AddSlotGeometry(nSlot, epp, bNoSubslots);
		}

		UpdateParamsFromRenderMaterial(nSlot, m_pPhysicalEntity);
	}
	else
		m_pPhysicalEntity->RemoveGeometry(GetPartId0(nSlot));
}

//////////////////////////////////////////////////////////////////////////
phys_geometry* CEntityPhysics::GetSlotGeometry(int nSlot)
{
	CEntitySlot* pSlot = m_pEntity->GetSlot(nSlot);
	if (!pSlot || !pSlot->GetStatObj())
		return 0;

	phys_geometry* pPhysGeom = pSlot->GetStatObj()->GetPhysGeom();
	return pPhysGeom;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::DestroyPhysicalEntity(bool bDestroyCharacters, int iMode)
{
	if (m_nFlags & FLAG_POS_EXTRAPOLATED)
	{
		SetFlags(m_nFlags | FLAG_IGNORE_XFORM_EVENT);
		pe_status_pos sp;
		if (m_pPhysicalEntity && m_pPhysicalEntity->GetStatus(&sp))
			m_pEntity->SetPos(sp.pos);
		SetFlags(m_nFlags & ~(FLAG_IGNORE_XFORM_EVENT | FLAG_POS_EXTRAPOLATED));
	}

	if (GetPhysAttachId() >= 0)
	{
		SEntityEvent eed(ENTITY_EVENT_DETACH);
		eed.nParam[0] = m_pEntity->GetId();
		ProcessEvent(eed);
	}

	bool bCharDeleted = false;
	for (int i = 0; i < m_pEntity->GetSlotCount(); i++)
		if (m_pEntity->GetCharacter(i))
		{
			bool bSameChar = m_pEntity->GetCharacter(i)->GetISkeletonPose()->GetCharacterPhysics() == m_pPhysicalEntity;
			if (bDestroyCharacters)
			{
				m_pEntity->GetCharacter(i)->GetISkeletonPose()->DestroyCharacterPhysics(iMode);
				bCharDeleted |= bSameChar;
			}
			else if (bSameChar && iMode == 0)
				m_pEntity->GetCharacter(i)->GetISkeletonPose()->SetCharacterPhysics(0);
		}

	if (m_pPhysicalEntity)
	{
		m_pPhysicalEntity->Release();
		if (!bCharDeleted)
			PhysicalWorld()->DestroyPhysicalEntity(m_pPhysicalEntity, iMode);
	}
	m_pPhysicalEntity = NULL;

	IRenderNode* pRndNode = m_pEntity->GetRenderNode();
	if (pRndNode)
		pRndNode->m_nInternalFlags &= ~(IRenderNode::WAS_INVISIBLE | IRenderNode::WAS_IN_VISAREA | IRenderNode::WAS_FARAWAY);

	OnChangedPhysics(false);
}

//////////////////////////////////////////////////////////////////////////
IPhysicalEntity* CEntityPhysics::QueryPhyscalEntity(IEntity* pEntity) const
{
	CEntity* pCEntity = (CEntity*)pEntity;
	if (pCEntity && pCEntity->GetPhysicalProxy())
	{
		CEntityPhysics* pPhysProxy = pCEntity->GetPhysicalProxy();
		return pPhysProxy->m_pPhysicalEntity;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::PhysicalizeLiving(SEntityPhysicalizeParams& params)
{
	// Dimensions and dynamics must be specified.
	assert(params.pPlayerDimensions && "Player dimensions parameter must be specified.");
	assert(params.pPlayerDynamics && "Player dynamics parameter must be specified.");
	if (!params.pPlayerDimensions || !params.pPlayerDynamics)
		return;

	PhysicalizeSimple(params);

	if (m_pPhysicalEntity)
	{
		m_pPhysicalEntity->SetParams(params.pPlayerDimensions);
		m_pPhysicalEntity->SetParams(params.pPlayerDynamics);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::PhysicalizeParticle(SEntityPhysicalizeParams& params)
{
	assert(params.pParticle && "Particle parameter must be specified.");
	if (!params.pParticle)
		return;

	CreatePhysicalEntity(params);
	m_pPhysicalEntity->SetParams(params.pParticle);
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::ReattachSoftEntityVtx(IPhysicalEntity* pAttachToEntity, int nAttachToPart)
{
	for (int slot = 0; slot < m_pEntity->GetSlotCount(); slot++)
	{
		CEntitySlot* pSlot = m_pEntity->GetSlot(slot);
		if (!pSlot || !pSlot->GetStatObj())
			continue;

		phys_geometry* pPhysGeom = pSlot->GetStatObj()->GetPhysGeom();
		if (!pPhysGeom)
			continue;

		IRenderMesh* pRM = pSlot->GetStatObj()->GetRenderMesh();
		if (pRM)
		{
			AttachSoftVtx(pRM, pAttachToEntity, nAttachToPart);
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CEntityPhysics::AttachSoftVtx(IRenderMesh* pRM, IPhysicalEntity* pAttachToEntity, int nAttachToPart)
{
	strided_pointer<ColorB> pColors(0);
	pRM->LockForThreadAccess();
	if ((pColors.data = (ColorB*)pRM->GetColorPtr(pColors.iStride, FSL_READ)))
	{
		pe_action_attach_points aap;
		aap.nPoints = 0;
		const int verticesCount = pRM->GetVerticesCount();
		aap.piVtx = new int[verticesCount];
		uint32 dummy = 0, mask = 0, * pVtxMap = pRM->GetPhysVertexMap();
		int i;
		if (!pVtxMap)
			pVtxMap = &dummy, mask = ~0;

		for (i = 0; i < pRM->GetVerticesCount(); i++)
		{
			if (pColors[i].g == 0)
			{
				PREFAST_ASSUME(aap.nPoints >= 0 && aap.nPoints < verticesCount);
				aap.piVtx[aap.nPoints++] = pVtxMap[i & ~mask] | i & mask;
			}
		}
		if (aap.nPoints)
		{
			if (m_pEntity->GetParent() && m_pEntity->GetParent()->GetPhysics())
				pAttachToEntity = m_pEntity->GetParent()->GetPhysics();
			if (pAttachToEntity)
			{
				aap.pEntity = pAttachToEntity;
				if (nAttachToPart >= 0)
					aap.partid = nAttachToPart;
			}
			m_pPhysicalEntity->Action(&aap);
		}
		delete[] aap.piVtx;
	}
	pRM->UnLockForThreadAccess();
}

void CEntityPhysics::PhysicalizeSoft(SEntityPhysicalizeParams& params)
{
	bool bReady = false;
	CreatePhysicalEntity(params);

	// Find first slot with static physical geometry.
	for (int slot = 0; slot < m_pEntity->GetSlotCount(); slot++)
	{
		CEntitySlot* pSlot = m_pEntity->GetSlot(slot);
		IRenderMesh* pRM;
		if (!pSlot || !pSlot->GetStatObj())
			continue;

		phys_geometry* pPhysGeom = pSlot->GetStatObj()->GetPhysGeom();
		if (!pPhysGeom)
			continue;

		const Matrix34& mtx = m_pEntity->GetSlotWorldTM(slot);
		const Vec3& scaleVec = Vec3(mtx.GetColumn0().len(), mtx.GetColumn1().len(), mtx.GetColumn2().len());
		float abs0 = (scaleVec.x - scaleVec.y) * (scaleVec.x - scaleVec.y);
		float abs1 = (scaleVec.x - scaleVec.z) * (scaleVec.x - scaleVec.z);
		float abs2 = (scaleVec.y - scaleVec.z) * (scaleVec.y - scaleVec.z);
		if ((abs0 + abs1 + abs2) > FLT_EPSILON)
			EntityWarning("<CEntityPhysics::PhysicalizeSoft> '%s' non-uniform scaling not supported for softbodies", m_pEntity->GetEntityTextDescription().c_str());
		float scale = scaleVec.x;

		pe_geomparams partpos;
		partpos.pos = pSlot->GetLocalTM().GetTranslation();
		Matrix33 rot = Matrix33(pSlot->GetLocalTM());
		rot.OrthonormalizeFast();
		partpos.q = Quat(rot);
		partpos.density = params.density;
		partpos.mass = params.mass;
		partpos.flags = 0;
		partpos.pLattice = pSlot->GetStatObj()->GetTetrLattice();
		partpos.scale = scale;
		pSlot->GetStatObj()->UpdateVertices(0, 0, 0, 0, 0); // with 0 input will just prep skin data
		if (pSlot->GetStatObj()->GetFlags() & STATIC_OBJECT_CLONE)
			partpos.flags |= geom_can_modify;
		m_pPhysicalEntity->AddGeometry(pPhysGeom, &partpos);
		if (partpos.pLattice)
		{
			pe_action_attach_points aap;
			aap.nPoints = -1; // per-point auto-attachment
			if (params.nAttachToPart >= 0)
				m_pPhysicalEntity->Action(&aap);
		}
		else
		{
			if (!(pRM = pSlot->GetStatObj()->GetRenderMesh()))
				continue;
			AttachSoftVtx(pRM, params.pAttachToEntity, params.nAttachToPart);
		}

		bReady = true;
		break;
	}

	if (!bReady && m_pPhysicalEntity)
	{
		pe_params_foreign_data pfd;
		pfd.iForeignData = PHYS_FOREIGN_ID_USER;
		pfd.pForeignData = params.pAttachToEntity;
		if (params.pAttachToEntity)
			params.pAttachToEntity->AddRef();
		pfd.iForeignFlags = params.nAttachToPart;
		m_pPhysicalEntity->SetParams(&pfd);
		m_pEntity->SetFlags(m_pEntity->GetFlags() | ENTITY_FLAG_SEND_RENDER_EVENT);
		m_nFlags |= FLAG_ATTACH_CLOTH_WHEN_VISIBLE;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::PhysicalizeArea(SEntityPhysicalizeParams& params)
{
	// Create area physical entity.
	assert(params.pAreaDef);
	if (!params.pAreaDef)
		return;
	if (!params.pAreaDef->pGravityParams && !params.pBuoyancy)
		return;

	AffineParts affineParts;
	affineParts.SpectralDecompose(m_pEntity->GetWorldTM());
	float fEntityScale = affineParts.scale.x;

	SEntityPhysicalizeParams::AreaDefinition* pAreaDef = params.pAreaDef;

	IPhysicalEntity* pPhysicalEntity = NULL;

	switch (params.pAreaDef->areaType)
	{
	case SEntityPhysicalizeParams::AreaDefinition::AREA_SPHERE:
		{
			primitives::sphere geomSphere;
			geomSphere.center.x = 0;
			geomSphere.center.y = 0;
			geomSphere.center.z = 0;
			geomSphere.r = pAreaDef->fRadius;
			IGeometry* pGeom = PhysicalWorld()->GetGeomManager()->CreatePrimitive(primitives::sphere::type, &geomSphere);
			pPhysicalEntity = PhysicalWorld()->AddArea(pGeom, affineParts.pos, affineParts.rot, fEntityScale);
		}
		break;

	case SEntityPhysicalizeParams::AreaDefinition::AREA_BOX:
		{
			primitives::box geomBox;
			geomBox.Basis.SetIdentity();
			geomBox.center = (pAreaDef->boxmax + pAreaDef->boxmin) * 0.5f;
			geomBox.size = (pAreaDef->boxmax - pAreaDef->boxmin) * 0.5f;
			geomBox.bOriented = 0;
			IGeometry* pGeom = PhysicalWorld()->GetGeomManager()->CreatePrimitive(primitives::box::type, &geomBox);
			pPhysicalEntity = PhysicalWorld()->AddArea(pGeom, affineParts.pos, affineParts.rot, fEntityScale);
		}
		break;

	case SEntityPhysicalizeParams::AreaDefinition::AREA_GEOMETRY:
		{
			// Will not physicalize if no slot geometry.
			phys_geometry* pPhysGeom = GetSlotGeometry(params.nSlot);
			if (!pPhysGeom || !pPhysGeom->pGeom)
			{
				EntityWarning("<CEntityPhysics::PhysicalizeArea> No Physical Geometry in Slot %d of Entity %s", params.nSlot, m_pEntity->GetEntityTextDescription().c_str());
				return;
			}
			pPhysicalEntity = PhysicalWorld()->AddArea(pPhysGeom->pGeom, affineParts.pos, affineParts.rot, fEntityScale);
		}
		break;

	case SEntityPhysicalizeParams::AreaDefinition::AREA_SHAPE:
		{
			assert(pAreaDef->pPoints);
			if (!pAreaDef->pPoints)
				return;

			pPhysicalEntity = PhysicalWorld()->AddArea(pAreaDef->pPoints, pAreaDef->nNumPoints, pAreaDef->zmin, pAreaDef->zmax);
		}
		break;

	case SEntityPhysicalizeParams::AreaDefinition::AREA_CYLINDER:
		{
			primitives::cylinder geomCylinder;
			geomCylinder.center = pAreaDef->center - affineParts.pos;
			if (pAreaDef->axis.len2())
				geomCylinder.axis = pAreaDef->axis;
			else
				geomCylinder.axis = Vec3(0, 0, 1);
			geomCylinder.r = pAreaDef->fRadius;
			geomCylinder.hh = pAreaDef->zmax - pAreaDef->zmin;
			/*
			   CryLogAlways( "CYLINDER: center:(%f %f %f) axis:(%f %f %f) radius:%f height:%f",
			   geomCylinder.center.x, geomCylinder.center.y, geomCylinder.center.z,
			   geomCylinder.axis.x, geomCylinder.axis.y, geomCylinder.axis.z,
			   geomCylinder.r, geomCylinder.hh );
			 */
			IGeometry* pGeom = PhysicalWorld()->GetGeomManager()->CreatePrimitive(primitives::cylinder::type, &geomCylinder);
			pPhysicalEntity = PhysicalWorld()->AddArea(pGeom, affineParts.pos, affineParts.rot, fEntityScale);
		}
		break;

	case SEntityPhysicalizeParams::AreaDefinition::AREA_SPLINE:
		{
			if (!pAreaDef->pPoints)
				return;
			Matrix34 rmtx = m_pEntity->GetWorldTM().GetInverted();
			for (int i = 0; i < pAreaDef->nNumPoints; i++)
				pAreaDef->pPoints[i] = rmtx * pAreaDef->pPoints[i];
			pPhysicalEntity = PhysicalWorld()->AddArea(pAreaDef->pPoints, pAreaDef->nNumPoints, pAreaDef->fRadius,
			                                           affineParts.pos, affineParts.rot, fEntityScale);
			if (params.pAreaDef->pGravityParams)
				params.pAreaDef->pGravityParams->bUniform = 0;
		}
		break;
	}

	if (!pPhysicalEntity)
		return;

	AssignPhysicalEntity(pPhysicalEntity);

	if ((pAreaDef->boxmax - pAreaDef->boxmin).len2())
	{
		pe_params_pos ppos;
		ppos.pos = m_pEntity->GetWorldPos();
		pe_params_bbox pbbox;
		pbbox.BBox[0] = ppos.pos + pAreaDef->boxmin;
		pbbox.BBox[1] = ppos.pos + pAreaDef->boxmax;
		pPhysicalEntity->SetParams(&pbbox);
		ppos.bRecalcBounds = 2;
		pPhysicalEntity->SetParams(&ppos);
	}

	if (params.nFlagsOR != 0)
	{
		pe_params_flags pf;
		pf.flagsOR = params.nFlagsOR;
		m_pPhysicalEntity->SetParams(&pf);
	}

	if (params.pAreaDef->pGravityParams)
	{
		if (!is_unused(pAreaDef->pGravityParams->falloff0))
		{
			if (pAreaDef->pGravityParams->falloff0 < 0.f)
				MARK_UNUSED pAreaDef->pGravityParams->falloff0;
			else
			{
				pe_status_pos pos;
				pPhysicalEntity->GetStatus(&pos);
			}
		}
		m_pPhysicalEntity->SetParams(params.pAreaDef->pGravityParams);
	}
	if (params.pBuoyancy)
	{
		m_pPhysicalEntity->SetParams(params.pBuoyancy);
	}

	if (m_pEntity->GetFlags() & ENTITY_FLAG_OUTDOORONLY)
	{
		pe_params_foreign_data fd;
		fd.iForeignFlags = PFF_OUTDOOR_AREA;
		m_pPhysicalEntity->SetParams(&fd);
	}
}

/////////////////////////////////////////////////////////////////////////
bool CEntityPhysics::PhysicalizeGeomCache(SEntityPhysicalizeParams& params)
{
	if (params.nSlot < 0)
	{
		return false;
	}

	IGeomCacheRenderNode* pGeomCacheRenderNode = m_pEntity->GetGeomCacheRenderNode(params.nSlot);
	if (!pGeomCacheRenderNode)
	{
		return false;
	}

	IGeomCache* pGeomCache = pGeomCacheRenderNode->GetGeomCache();
	if (!pGeomCache)
	{
		return false;
	}

	pe_articgeomparams geomParams;
	geomParams.flags = geom_collides | geom_floats;
	geomParams.density = -1;
	geomParams.mass = -1;

	pGeomCacheRenderNode->InitPhysicalEntity(m_pPhysicalEntity, geomParams);
	m_pEntity->InvalidateTM(ENTITY_XFORM_POS | ENTITY_XFORM_ROT | ENTITY_XFORM_SCL);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPhysics::PhysicalizeCharacter(SEntityPhysicalizeParams& params)
{
	if (params.nSlot < 0)
		return false;

	ICharacterInstance* pCharacter = m_pEntity->GetCharacter(params.nSlot);
	if (!pCharacter)
		return false;
	int partid0 = GetPartId0(params.nSlot);
	Matrix34 mtxloc = m_pEntity->GetSlotLocalTM(params.nSlot, false);
	mtxloc.ScaleColumn(m_pEntity->GetScale());

	m_nFlags |= FLAG_PHYS_CHARACTER;
	m_nFlags &= ~CHARACTER_SLOT_MASK;
	m_nFlags |= (params.nSlot & CHARACTER_SLOT_MASK);

	if (pCharacter->GetObjectType() != CGA)
		m_nFlags |= FLAG_SYNC_CHARACTER, partid0 = -1;

	if (params.type == PE_LIVING)
	{
		pe_params_part pp;
		pp.ipart = 0;
		pp.flagsAND = ~(geom_colltype_ray | geom_colltype13);
		pp.bRecalcBBox = 0;
		m_pPhysicalEntity->SetParams(&pp);
	}

	int i, iAux;
	pe_params_foreign_data pfd;
	pfd.pForeignData = m_pEntity;
	pfd.iForeignData = PHYS_FOREIGN_ID_ENTITY;
	pfd.iForeignFlagsOR = PFF_UNIMPORTANT;

	if (!m_pPhysicalEntity || params.type == PE_LIVING)
	{
		if (params.fStiffnessScale < 0)
			params.fStiffnessScale = 1.0f;
		IPhysicalEntity* pCharPhys = pCharacter->GetISkeletonPose()->CreateCharacterPhysics(m_pPhysicalEntity, params.mass, -1, params.fStiffnessScale,
		                                                                                    params.nLod, mtxloc);
		pCharacter->GetISkeletonPose()->CreateAuxilaryPhysics(pCharPhys, m_pEntity->GetSlotWorldTM(params.nSlot), params.nLod);

		for (iAux = 0; pCharacter->GetISkeletonPose()->GetCharacterPhysics(iAux); iAux++)
			pCharacter->GetISkeletonPose()->GetCharacterPhysics(iAux)->SetParams(&pfd);
		if (!m_pPhysicalEntity && iAux == 1)
		{
			// we have a single physicalized rope
			pe_params_flags pf;
			pf.flagsOR = pef_log_poststep;
			pCharacter->GetISkeletonPose()->GetCharacterPhysics(0)->SetParams(&pf);
		}

		IAttachmentManager* pAttman = pCharacter->GetIAttachmentManager();
		IAttachmentObject* pAttachmentObject;
		if (pAttman)
			for (i = pAttman->GetAttachmentCount() - 1; i >= 0; i--)
				if ((pAttachmentObject = pAttman->GetInterfaceByIndex(i)->GetIAttachmentObject()) &&
				    pAttman->GetInterfaceByIndex(i)->GetType() == CA_BONE && (pCharacter = pAttachmentObject->GetICharacterInstance()))
					for (iAux = 0; pCharacter->GetISkeletonPose()->GetCharacterPhysics(iAux); iAux++)
						pCharacter->GetISkeletonPose()->GetCharacterPhysics(iAux)->SetParams(&pfd);

		if (pCharPhys)
		{
			// make sure the skeleton goes before the ropes in the list
			pe_params_pos pp;
			pp.iSimClass = 6;
			pCharPhys->SetParams(&pp);
			pp.iSimClass = 4;
			pCharPhys->SetParams(&pp);
		}
	}
	else
	{
		pCharacter->GetISkeletonPose()->BuildPhysicalEntity(m_pPhysicalEntity, params.mass, -1, params.fStiffnessScale, params.nLod, partid0, mtxloc);
		pCharacter->GetISkeletonPose()->CreateAuxilaryPhysics(m_pPhysicalEntity, m_pEntity->GetSlotWorldTM(params.nSlot), params.nLod);
		pCharacter->GetISkeletonPose()->SetCharacterPhysics(m_pPhysicalEntity);
		pe_params_pos pp;
		pp.q = m_pEntity->m_qRotation;
		for (IEntity* pent = m_pEntity->GetParent(); pent; pent = pent->GetParent())
			pp.q = pent->GetRotation() * pp.q;
		m_pPhysicalEntity->SetParams(&pp);
		for (iAux = 0; pCharacter->GetISkeletonPose()->GetCharacterPhysics(iAux); iAux++)
			pCharacter->GetISkeletonPose()->GetCharacterPhysics(iAux)->SetParams(&pfd);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPhysics::ConvertCharacterToRagdoll(SEntityPhysicalizeParams& params, const Vec3& velInitial)
{
	if (params.nSlot < 0)
		return false;

	ICharacterInstance* pCharacter = m_pEntity->GetCharacter(params.nSlot);
	if (!pCharacter)
		return false;

	m_nFlags |= FLAG_PHYS_CHARACTER;
	m_nFlags |= FLAG_SYNC_CHARACTER;
	m_nFlags &= ~CHARACTER_SLOT_MASK;
	m_nFlags |= (params.nSlot & CHARACTER_SLOT_MASK);

	// This is special case when converting living character into the rag-doll
	IPhysicalEntity* pPhysEntity = pCharacter->GetISkeletonPose()->RelinquishCharacterPhysics(//m_pEntity->GetSlotWorldTM(params.nSlot),
	  m_pEntity->GetWorldTM(), params.fStiffnessScale, params.bCopyJointVelocities, velInitial);
	if (pPhysEntity)
	{
		// Store current velocity.
		//pe_status_dynamics sd;
		//sd.v.Set(0,0,0);
		if (m_pPhysicalEntity)
		{
			//m_pPhysicalEntity->GetStatus(&sd);
			DestroyPhysicalEntity(false);
		}

		AssignPhysicalEntity(pPhysEntity, -1);  //params.nSlot );

		int i, nAux;// = pCharacter->CreateAuxilaryPhysics( pPhysEntity, m_pEntity->GetSlotWorldTM(params.nSlot) );
		pe_params_foreign_data pfd;
		pfd.iForeignFlagsOR = PFF_UNIMPORTANT;
		if (pPhysEntity)
			pPhysEntity->SetParams(&pfd);
		pfd.pForeignData = m_pEntity;
		pfd.iForeignData = PHYS_FOREIGN_ID_ENTITY;
		pe_params_flags pf;
		pf.flagsOR = pef_log_collisions | params.nFlagsOR; // without nMaxLoggedCollisions it will only allow breaking thing on contact
		pf.flagsAND = params.nFlagsAND;
		pPhysEntity->SetParams(&pf);
		pe_simulation_params sp;
		sp.maxLoggedCollisions = 3;
		pPhysEntity->SetParams(&sp);
		pf.flagsOR = pef_traceable | rope_collides | rope_collides_with_terrain;//|rope_ignore_attachments;
		pe_params_rope pr;
		pr.flagsCollider = geom_colltype0;
		for (nAux = 0; pCharacter->GetISkeletonPose()->GetCharacterPhysics(nAux); nAux++)
		{
			pCharacter->GetISkeletonPose()->GetCharacterPhysics(nAux)->SetParams(&pfd);
			pCharacter->GetISkeletonPose()->GetCharacterPhysics(nAux)->SetParams(&pf);
			pCharacter->GetISkeletonPose()->GetCharacterPhysics(nAux)->SetParams(&pr);
		}

		//////////////////////////////////////////////////////////////////////////
		// Iterate thru character attachments, and set rope parameters.
		IAttachmentManager* pAttman = pCharacter->GetIAttachmentManager();
		if (pAttman)
		{
			for (i = pAttman->GetAttachmentCount() - 1; i >= 0; i--)
			{
				IAttachmentObject* pAttachmentObject = pAttman->GetInterfaceByIndex(i)->GetIAttachmentObject();
				if (pAttachmentObject && pAttman->GetInterfaceByIndex(i)->GetType() == CA_BONE)
				{
					ICharacterInstance* pAttachedCharacter = pAttachmentObject->GetICharacterInstance();
					if (pAttachedCharacter)
						for (nAux = 0; pAttachedCharacter->GetISkeletonPose()->GetCharacterPhysics(nAux); nAux++)
						{
							pAttachedCharacter->GetISkeletonPose()->GetCharacterPhysics(nAux)->SetParams(&pfd);
							pAttachedCharacter->GetISkeletonPose()->GetCharacterPhysics(nAux)->SetParams(&pf);
						}
				}
			}
		}

		if (!params.bCopyJointVelocities)
		{
			//////////////////////////////////////////////////////////////////////////
			// Restore previous velocity
			pe_action_set_velocity asv;
			asv.v = velInitial;
			pPhysEntity->Action(&asv);
		}
	}

	// Force character bones to be updated from physics bones.
	//if (m_pPhysicalEntity)
	//	pCharacter->SynchronizeWithPhysicalEntity( m_pPhysicalEntity );
	/*SEntityUpdateContext ctx;
	   if (m_pEntity->GetEntityRender())
	   m_pEntity->GetEntityRender()->Update(ctx);*/
	SetFlags(m_nFlags | FLAG_IGNORE_XFORM_EVENT);
	m_pEntity->SetRotation(Quat(IDENTITY));
	SetFlags(m_nFlags & ~FLAG_IGNORE_XFORM_EVENT);

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::SyncCharacterWithPhysics()
{
	// Find characters.
	int nSlot = (m_nFlags & CHARACTER_SLOT_MASK);
	ICharacterInstance* pCharacter = m_pEntity->GetCharacter(nSlot);
	if (pCharacter)
	{
		pe_type type = m_pPhysicalEntity->GetType();
		switch (type)
		{
		case PE_ARTICULATED:
			pCharacter->GetISkeletonPose()->SynchronizeWithPhysicalEntity(m_pPhysicalEntity);
			break;
			/*case PE_LIVING:
			   pCharacter->UpdatePhysics(1.0f);
			   break;*/
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPhysics::PhysicalizeFoliage(int iSlot)
{
	if (m_pEntity->GetEntityRender())
		if (IRenderNode* pRN = m_pEntity->GetRenderNode(iSlot))
		{
			pRN->SetPhysics(m_pPhysicalEntity);
			pRN->PhysicalizeFoliage(true, 0, iSlot);
			pRN->SetPhysics(nullptr);
			return true;
		}
	return false;
}

void CEntityPhysics::DephysicalizeFoliage(int iSlot)
{
	if (IFoliage* pFoliage = GetFoliage(iSlot))
		pFoliage->Release();
}

IFoliage* CEntityPhysics::GetFoliage(int iSlot)
{
	if (m_pEntity->GetEntityRender())
		if (IRenderNode* pRN = m_pEntity->GetRenderNode(iSlot))
			return pRN->GetFoliage();
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::AddImpulse(int ipart, const Vec3& pos, const Vec3& impulse, bool bPos, float fAuxScale, float fPushScale /* = 1.0f */)
{
	ENTITY_PROFILER

	IPhysicalEntity* pPhysicalEntity = m_pPhysicalEntity;
	CEntityPhysics* pAdamProxy = ((CEntity*)m_pEntity->GetAdam())->GetPhysicalProxy();
	if (pAdamProxy && pAdamProxy->m_pPhysicalEntity)
		pPhysicalEntity = pAdamProxy->m_pPhysicalEntity;

	if (!pPhysicalEntity)
		return;

	const pe_type physicalEntityType = pPhysicalEntity->GetType();
	const bool bNotLiving = physicalEntityType != PE_LIVING;

	//if (m_pPhysicalEntity && (!m_bIsADeadBody || (m_pEntitySystem->m_pHitDeadBodies->GetIVal() )))
	if (pPhysicalEntity && CVar::pHitDeadBodies->GetIVal() && (fPushScale > 0.0f))
	{
		// Ignore the pushScale on not living entities
		const float fImpulseScale = bNotLiving ? fAuxScale : fPushScale;

		Vec3 mod_impulse = impulse;
		pe_status_nparts statusTmp;
		if (!(pPhysicalEntity->GetStatus(&statusTmp) > 5 && physicalEntityType == PE_ARTICULATED))
		{
			// don't scale impulse for complex articulated entities
			pe_status_dynamics sd;
			float minVel = CVar::pMinImpulseVel->GetFVal();
			if (minVel > 0 && pPhysicalEntity->GetStatus(&sd) && mod_impulse * mod_impulse < sqr(sd.mass * minVel))
			{
				float fScale = CVar::pImpulseScale->GetFVal();
				if (fScale > 0)
					mod_impulse *= fScale;
				else if (sd.mass < CVar::pMaxImpulseAdjMass->GetFVal())
					mod_impulse = mod_impulse.normalized() * (minVel * sd.mass);
			}
		}
		pe_action_impulse ai;
		ai.partid = ipart;
		if (bPos)
			ai.point = pos;
		ai.impulse = mod_impulse * fImpulseScale;
		pPhysicalEntity->Action(&ai);
	}

	//	if (bPos && m_bVisible && (!m_physic || bNotLiving || (CVar::pHitCharacters->GetIVal())))
	if (bPos && (bNotLiving || (CVar::pHitCharacters->GetIVal())))
	{
		// Use all slots.
		for (int slot = 0; slot < m_pEntity->GetSlotCount(); slot++)
		{
			ICharacterInstance* pCharacter = m_pEntity->GetCharacter(slot);
			if (pCharacter)
				pCharacter->GetISkeletonPose()->AddImpact(ipart, pos, impulse * fAuxScale);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::OnPhysicsPostStep(EventPhysPostStep* pEvent)
{
	if (CheckFlags(FLAG_PHYSICS_DISABLED))
		return;

	int nWhyFlags = ENTITY_XFORM_PHYSICS_STEP | ((m_nFlags & FLAG_DISABLE_ENT_SERIALIZATION) ? ENTITY_XFORM_NO_PROPOGATE : 0) |
	                ENTITY_XFORM_NO_EVENT & gEnv->pPhysicalWorld->GetPhysVars()->bLogStructureChanges - 1;
	if (!m_pPhysicalEntity)
	{
		if (m_nFlags & FLAG_SYNC_CHARACTER && !m_pEntity->IsActivatedForUpdates())
			m_pEntity->ActivateForNumUpdates(4);
		if (pEvent)
			m_pEntity->SetPosRotScale(pEvent->pos, pEvent->q, m_pEntity->GetScale(), nWhyFlags);
		return;
	}

	// Guards to not let physical proxy move physical entity in response to XFORM event.
	SetFlags(m_nFlags | FLAG_IGNORE_XFORM_EVENT);

	// Set transformation on the entity from transformation of the physical entity.
	pe_status_pos ppos;
	// If Entity is attached do not accept entity position from physics (or assume its world space coords)
	if (!(m_pEntity->m_hierarchy.pParent) || m_pEntity->GetParentBindingType() == CEntity::EBindingType::eBT_LocalSim)
	{
		if (pEvent)
		{
			ppos.pos = pEvent->pos, ppos.q = pEvent->q;
			CEntity *pNewHost;
			if (pEvent->pGrid && (pNewHost = (CEntity*)pEvent->pGrid->GetForeignData(PHYS_FOREIGN_ID_ENTITY)) != m_pEntity->m_hierarchy.pParent)
			{
				m_pEntity->DetachThis(IEntity::ATTACHMENT_SUPPRESS_UPDATE);
				if (pNewHost)
					pNewHost->AttachChild(m_pEntity, SChildAttachParams(IEntity::ATTACHMENT_LOCAL_SIM | IEntity::ATTACHMENT_SUPPRESS_UPDATE));
			}
		}
		else
			m_pPhysicalEntity->GetStatus(&ppos);

		m_pEntity->SetPosRotScale(ppos.pos, ppos.q, m_pEntity->GetScale(), nWhyFlags);
		SetFlags(m_nFlags & ~FLAG_POS_EXTRAPOLATED);
	}

	pe_type physType = m_pPhysicalEntity->GetType();

	if (physType != PE_RIGID) // In Rigid body sub-parts are not controlled by physics.
	{
		if (m_nFlags & FLAG_SYNC_CHARACTER)
		{
			//SyncCharacterWithPhysics();
			if (!m_pEntity->IsActivatedForUpdates())
				m_pEntity->ActivateForNumUpdates(4);
		}
		else if (physType == PE_ARTICULATED || physType == PE_WHEELEDVEHICLE)
		{
			// Use all slots.
			ppos.flags = status_local;
			for (int slot = 0; slot < m_pEntity->GetSlotCount(); slot++)
			{
				int nSlotFlags = m_pEntity->GetSlotFlags(slot);
				if (!(nSlotFlags & ENTITY_SLOT_IGNORE_PHYSICS))
				{
					ppos.partid = GetPartId0(slot);
					if (m_pPhysicalEntity->GetStatus(&ppos))
					{
						Matrix34 tm = Matrix34::Create(Vec3(ppos.scale, ppos.scale, ppos.scale), ppos.q, ppos.pos);
						m_pEntity->SetSlotLocalTM(slot, tm);
					}
				}
			}
		}

		IRenderNode* pRenderNode = m_pEntity->GetEntityRender() ? m_pEntity->GetRenderNode() : nullptr;

		if (physType == PE_SOFT && !(pRenderNode && pRenderNode->m_nInternalFlags & IRenderNode::WAS_INVISIBLE))
		{
			pe_status_softvtx ssv;
			const Vec3& entityScale = m_pEntity->GetScale();

			m_pPhysicalEntity->GetStatus(&ssv);
			if (m_pEntity->GetParent())
			{
				Matrix34 mtx = m_pEntity->GetParent()->GetWorldTM().GetInverted();
				ssv.pos = mtx * ssv.pos;
				ssv.q = Quat(Matrix33(mtx) * Diag33(mtx.GetColumn0().len(), mtx.GetColumn1().len(), mtx.GetColumn2().len()).invert());
			}
			m_pEntity->SetPosRotScale(ssv.pos, ssv.q, entityScale, ENTITY_XFORM_PHYSICS_STEP);

			IStatObj* pStatObj = m_pEntity->GetStatObj(0);
			IStatObj* pStatObjNew = pStatObj->UpdateVertices(ssv.pVtx, ssv.pNormals, 0, ssv.nVtx, ssv.pVtxMap);
			if (pStatObjNew != pStatObj)
			{
				ssv.pMesh->SetForeignData(pStatObjNew, 0);
				m_pEntity->GetEntityRender()->SetSlotGeometry(0, pStatObjNew);
			}
			m_pEntity->GetEntityRender()->InvalidateLocalBounds();
		}
	}

	SetFlags(m_nFlags & (~FLAG_IGNORE_XFORM_EVENT));
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::AttachToPhysicalEntity(IPhysicalEntity* pPhysEntity)
{
	if (!pPhysEntity && m_pEntity->GetParent())
	{
		SEntityEvent event(ENTITY_EVENT_DETACH);
		event.nParam[0] = m_pEntity->GetId();
		m_pEntity->GetParent()->SendEvent(event);
	}
	// Get Position from physical entity.
	if (m_pPhysicalEntity = pPhysEntity)
	{
		m_pPhysicalEntity->AddRef();
		pe_params_foreign_data pfd;
		pfd.pForeignData = m_pEntity;
		pfd.iForeignData = PHYS_FOREIGN_ID_ENTITY;
		pPhysEntity->SetParams(&pfd, 1);
		gEnv->pPhysicalWorld->SetPhysicalEntityId(pPhysEntity, CEntitySystem::IdToHandle(m_pEntity->GetId()).GetIndex());

		pe_params_flags pf;
		pf.flagsOR = pef_log_state_changes | pef_log_poststep;
		pPhysEntity->SetParams(&pf);

		OnPhysicsPostStep();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::CreateRenderGeometry(int nSlot, IGeometry* pFromGeom, bop_meshupdate* pLastUpdate)
{
	m_pEntity->GetEntityRender()->SetSlotGeometry(0, gEnv->p3DEngine->UpdateDeformableStatObj(pFromGeom, pLastUpdate));
	m_pEntity->GetEntityRender()->InvalidateLocalBounds();  // since SetSlotGeometry doesn't do it if pStatObj is the same
}

//////////////////////////////////////////////////////////////////////////
CEntity* CEntityPhysics::GetCEntity(IPhysicalEntity* pPhysEntity)
{
	assert(pPhysEntity);
	CEntity* pEntity = (CEntity*)pPhysEntity->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
	return pEntity;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPhysics::OnContactWithEntity(CEntity* pEntity)
{

}

void CEntityPhysics::OnCollision(CEntity* pTarget, int matId, const Vec3& pt, const Vec3& n, const Vec3& vel, const Vec3& targetVel, int partId, float mass)
{
	CEntityComponentLuaScript* pProxy = m_pEntity->GetScriptProxy();

	if (pProxy)
	{
		if (CVar::pLogCollisions->GetIVal() != 0)
		{
			string s1 = m_pEntity->GetEntityTextDescription();
			string s2;
			if (pTarget)
				s2 = pTarget->GetEntityTextDescription();
			else
				s2 = "<Unknown>";
			CryLogAlways("OnCollision %s (Target: %s)", s1.c_str(), s2.c_str());
		}
		pProxy->OnCollision(pTarget, matId, pt, n, vel, targetVel, partId, mass);
	}
}

void CEntityPhysics::OnChangedPhysics(bool bEnabled)
{
	SEntityEvent evt(ENTITY_EVENT_ENABLE_PHYSICS);
	evt.nParam[0] = bEnabled;
	m_pEntity->SendEvent(evt);
}

void CEntityPhysics::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	//pSizer->AddObject(m_pPhysicalEntity);
}

void CEntityPhysics::EnableNetworkSerialization(bool enable)
{
	if (enable == false)
	{
		m_nFlags |= FLAG_DISABLE_ENT_SERIALIZATION;
		// Pause
		if (m_pPhysicalEntity)
			m_pPhysicalEntity->SetNetworkAuthority(-1, 1);
	}
	else
	{
		m_nFlags &= ~FLAG_DISABLE_ENT_SERIALIZATION;
		// Unpause
		if (m_pPhysicalEntity)
			m_pPhysicalEntity->SetNetworkAuthority(-1, 0);
	}
}
