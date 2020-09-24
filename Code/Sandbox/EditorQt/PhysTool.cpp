// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "PhysTool.h"
#include "Viewport.h"

// These inclusions must use quotes to be redirected by PhysDebugger, see DummyEditor.h
#include "Objects/DisplayContext.h"
#include "Cry3DEngine/I3DEngine.h"
#include "Cry3DEngine/ISurfaceType.h"
#include "CryAnimation/ICryAnimation.h"
#include "CryEntitySystem/IEntity.h"
#include "CryMath/Cry_Camera.h"
#include "CryPhysics/IPhysics.h"
#include "CrySystem/ISystem.h"
#include "Objects/EntityObject.h"
#include "IEditorImpl.h"
#include "CrySystem/ConsoleRegistration.h"

IMPLEMENT_DYNAMIC(CPhysPullTool, CEditTool)

//////////////////////////////////////////////////////////////////////////

struct SMiniCamera
{
	SMiniCamera(const CViewport* view) : mtx(view->GetViewTM()), fov(view->GetFOV()) { view->GetDimensions(&sz.x, &sz.y); }
	SMiniCamera(const CCamera* cam) : mtx(cam->GetMatrix()), fov(cam->GetFov()), sz(cam->GetViewSurfaceX(), cam->GetViewSurfaceZ()) {}
	const Matrix34& mtx;
	Vec2i           sz;
	float           fov;
	Vec3            Screen2World(const CPoint& point, float dist) const
	{
		float k = 2 * tan(fov * 0.5f) / sz.y;
		return mtx.GetTranslation() + mtx.TransformVector(Vec3((point.x - sz.x * 0.5f) * k, 1, (sz.y * 0.5f - point.y) * k) * dist);
	}
};

void ReleasePhysEnt(IPhysicalEntity*& pent, bool doit = true)
{
	if (pent && doit)
	{
		pent->Release();
		gEnv->pPhysicalWorld->DestroyPhysicalEntity(pent);
		pent = 0;
	}
}

bool CanUseIK(IPhysicalEntity *pent)
{
	pe_status_dynamics sd;
	return pent->GetType() == PE_ARTICULATED && pent->GetStatus(&sd) && sd.mass > 0;
}

float CPhysPullTool::cv_HitVel0 = 1;
float CPhysPullTool::cv_HitVel1 = 15;
float CPhysPullTool::cv_HitProjMass = 0.5f;
float CPhysPullTool::cv_HitProjVel0 = 50;
float CPhysPullTool::cv_HitProjVel1 = 450;
float CPhysPullTool::cv_HitExplR = 4.0f;
float CPhysPullTool::cv_HitExplPress0 = 50;
float CPhysPullTool::cv_HitExplPress1 = 250;
float CPhysPullTool::cv_IKprec = 0.01f;
int   CPhysPullTool::cv_IKStiffMode = 0;

bool CPhysPullTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	m_lastMousePos = point;
	Vec3 org, dir;
	if (view)
	{
		org = view->GetViewTM().GetTranslation();
		dir = SMiniCamera(view).Screen2World(point, 100) - org;
	}
	ray_hit hit;
	int nHits = event != eMouseLDown && event != eMouseLUp || !view ? 0 :
	            gEnv->pPhysicalWorld->RayWorldIntersection(org, dir, ent_terrain | ent_static | ent_sleeping_rigid | ent_rigid | ent_living | ent_independent,
	                                                       rwi_pierceability(15) | rwi_colltype_any(geom_colltype_solid | geom_colltype_ray) | rwi_auto_grid_start, &hit, 1);
	PhysicsVars *vars = gEnv->pPhysicalWorld->GetPhysVars();

	if (event == eMouseLDown)
	{
		if (flags & MK_SHIFT)
		{
			if (!vars->lastTimeStep)
			{
				bool hitExisting = false;
				if (m_pRagdoll && m_maskConstr)
				{
					pe_status_constraint sc;
					float t = sqr(m_sizeConstr) * dir.len2();
					IterateRagdollConstraints([&](int id)->bool 
					{
						sc.id = id + m_idConstr+1; 
						if (m_pRagdoll->GetStatus(&sc) && (sc.pt[0]-org ^ dir).len2() < t && (!nHits || sqr_signed(hit.pt*dir - sc.pt[0]*dir) > -t))
						{
							pe_action_update_constraint auc;
							auc.idConstraint = sc.id;
							auc.bRemove = 1;
							m_pRagdoll->Action(&auc);
							m_maskConstr &= ~(1u << id);
							hitExisting = true;
							return true;
						}
						return false;
					});
				}
				if (!hitExisting && nHits && CanUseIK(hit.pCollider))
				{
					if (m_pRagdoll != hit.pCollider)
					{
						ClearRagdollConstraints();
						SyncRagdollConstraintsMask(hit.pCollider);
					}
					if (m_maskConstr != ~0u)
					{
						pe_action_add_constraint aac;
						pe_action_update_constraint auc;
						aac.pBuddy = WORLD_ENTITY;
						int id = ilog2(m_maskConstr + 1 ^ m_maskConstr);
						aac.id = id + m_idConstr + 1;
						aac.pt[0] = hit.pt;
						aac.partid[0] = hit.partid;
						aac.flags = flags & MK_CONTROL ? constraint_no_rotation : 0;
						if (flags & MK_ALT)
						{
							aac.flags = constraint_free_position | constraint_instant;
							aac.yzlimits[0] = aac.yzlimits[1] = 0;
							ray_hit hit1;
							hit1.n = Vec3(0,0,1);
							gEnv->pPhysicalWorld->RayWorldIntersection(hit.pt, vars->gravity.GetNormalizedFast()*3, ent_terrain|ent_static, rwi_stop_at_pierceable, &hit1,1, hit.pCollider);
							aac.qframe[0] = Quat::CreateRotationV0V1(Vec3(1,0,0), hit1.n);
							aac.qframe[1] = Quat(IDENTITY);
							pe_status_pos sp;
							sp.partid = hit.partid;
							hit.pCollider->GetStatus(&sp);
							primitives::box bbox;
							sp.pGeom->GetBBox(&bbox);
							Vec3 nloc(ZERO); nloc[idxmin3(bbox.size)] = 1;
							Vec3 n = nloc * bbox.Basis;
							auc.idConstraint = aac.id;
							auc.flags = local_frames_part;
							auc.qframe[0] = Quat::CreateRotationV0V1(Vec3(1,0,0), n * (float)sgnnz(hit1.n*(sp.q*n)));
						}
						if (hit.pCollider->Action(&aac))
						{
							m_maskConstr |= 1ull << id;
							if (m_pRagdoll != hit.pCollider)
								(m_pRagdoll = hit.pCollider)->AddRef();
							if (aac.flags & constraint_free_position)	m_pRagdoll->Action(&auc);
						}
					}
				}
				return true;
			}
			m_timeHit = gEnv->pTimer->GetCurrTime();
			return true;
		}
		if (nHits)
		{
			pe_params_pos pp;
			pp.pos = hit.pt;
			pp.q = Quat(view->GetViewTM());
			if (!m_pEntAttach)
			{
				pe_params_flags pf;
				pf.flagsOR = pef_never_affect_triggers;
				(m_pEntAttach = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_RIGID, &pf))->AddRef();
				primitives::sphere sph;
				sph.center.zero();
				sph.r = 0.05f;
				IGeomManager* pGeoman = gEnv->pPhysicalWorld->GetGeomManager();
				phys_geometry* pGeom = pGeoman->RegisterGeometry(pGeoman->CreatePrimitive(primitives::sphere::type, &sph));
				pe_geomparams gp;
				gp.flags = gp.flagsCollider = 0;
				m_pEntAttach->AddGeometry(pGeom, &gp);
				pGeom->pGeom->Release();
				pGeoman->UnregisterGeometry(pGeom);
				pe_simulation_params sp;
				sp.noMassDecay = 1;
				m_pEntAttach->SetParams(&sp);
			}

			pe_status_dynamics sd;
			hit.pCollider->GetStatus(&sd);
			m_partid = hit.partid;
			pe_status_pos sp;
			sp.pGridRefEnt = m_pEntAttach;
			sp.partid = hit.partid;
			hit.pCollider->GetStatus(&sp);
			m_locAttachPos = (hit.pt - sp.pos) * sp.q;
			m_IKapplied = false;

			switch (hit.pCollider->GetType())
			{
			case PE_SOFT:
			case PE_ROPE:
				m_nAttachPoints = 0;
				if (!(flags & MK_CONTROL))
				{
					pe_action_attach_points aap;
					int ivtx = hit.ipart;
					aap.pEntity = m_pEntAttach;
					aap.nPoints = -1;
					if (hit.pCollider->GetType() == PE_ROPE)
					{
						aap.nPoints = 1;
						aap.piVtx = &ivtx;
						pe_params_rope pr;
						hit.pCollider->GetParams(&pr);
						Vec3 seg = pr.pPoints[ivtx + 1] - pr.pPoints[ivtx];
						ivtx += isneg(seg.len2() * 0.5f - (hit.pt - pr.pPoints[ivtx]) * seg);
						m_partid = ivtx;
					}
					m_nAttachPoints = hit.pCollider->Action(&aap, (1 - m_pEntAttach->SetParams(&pp)) >> 31);
				}
				break;
			case PE_LIVING:
				{
					ReleasePhysEnt(m_pRope);
					(m_pRope = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_ROPE, &pp))->AddRef();
					pe_params_rope pr;
					pr.collTypes = 0;
					pr.nSegments = 1;
					pr.pEntTiedTo[0] = m_pEntAttach;
					pr.pEntTiedTo[1] = hit.pCollider;
					pr.ptTiedTo[0] = pr.ptTiedTo[1] = hit.pt;
					pr.length = 0;
					pr.maxForce = sd.mass * 100;
					pr.penaltyScale = 10;
					m_pRope->SetParams(&pr, (1 - m_pEntAttach->SetParams(&pp)) >> 31);
					pe_params_flags pf;
					pf.flags = rope_subdivide_segs | rope_no_tears;
					m_pRope->SetParams(&pf);
					pe_simulation_params sp;
					sp.minEnergy = -1;
					m_pRope->SetParams(&sp);
					pe_player_dynamics pd;
					pd.timeImpulseRecover = 2;
					hit.pCollider->SetParams(&pd);
				}
				break;
			default:
				{
					pe_action_add_constraint aac;
					aac.pt[0] = pp.pos;
					aac.flags = constraint_no_tears | constraint_no_enforcement | (flags & MK_CONTROL ? constraint_no_rotation : 0);
					aac.pt[0] = hit.pt;
					aac.partid[0] = hit.partid;
					aac.maxPullForce = sd.mass * 100;
					aac.maxBendTorque = sd.mass * 1000;
					aac.hardnessLin = 5;
					aac.id = m_idConstr;
					aac.pBuddy = m_pEntAttach;
					pp.q = Quat(view->GetViewTM());
					hit.pCollider->Action(&aac, (1 - m_pEntAttach->SetParams(&pp)) >> 31);
				}
			}
			if (!vars->lastTimeStep && hit.pCollider != m_pEntPull && hit.pCollider != m_pRagdoll && CanUseIK(hit.pCollider))
			{
				ClearRagdollConstraints();
				(m_pRagdoll = hit.pCollider)->AddRef();
				SyncRagdollConstraintsMask(m_pRagdoll);
			}
			(m_pEntPull = hit.pCollider)->AddRef();
			m_attachDist = view->GetViewTM().GetColumn1() * (pp.pos - org);
			m_timeMove = gEnv->pTimer->GetCurrTime();
			m_lastAttachPos = pp.pos;
		}
		return true;
	}
	else if (event == eMouseLUp)
	{
		if (m_pEntPull)
		{
			pe_action_update_constraint auc;
			auc.idConstraint = m_idConstr;
			auc.bRemove = 1;
			m_pEntPull->Action(&auc);
			pe_action_attach_points aap;
			aap.pEntity = m_pEntAttach;
			aap.nPoints = 0;
			m_pEntPull->Action(&aap);
			if (!m_nAttachPoints && m_pEntPull->GetType() == PE_SOFT && flags & MK_CONTROL)
			{
				pe_status_pos sp;
				sp.pGridRefEnt = m_pEntAttach;
				m_pEntPull->GetStatus(&sp);
				pe_action_slice as;
				Vec3 pt[3] = { org, org + (sp.q * m_locAttachPos + sp.pos - org).normalized() * 100, org + dir };
				as.pt = pt;
				Vec3 ptEdge = pt[1].GetRotated(pt[0], (pt[2] - pt[0] ^ pt[1] - pt[0]).normalized(), DEG2RAD(1));
				ray_hit hit1;
				if (!(gEnv->pPhysicalWorld->RayTraceEntity(m_pEntPull, pt[0], ptEdge - pt[0], &hit1)))
					pt[1] = ptEdge; // slice over the edge if close enough
				m_pEntPull->Action(&as);
			}
			pe_action_awake aa;
			if (!m_IKapplied)
				m_pEntPull->Action(&aa);
			IEntity *pEnt = (IEntity*)m_pEntPull->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
			if (pEnt && m_IKapplied && !gEnv->pPhysicalWorld->GetPhysVars()->lastTimeStep)
				if (CEntityObject *pEditorObj = CEntityObject::FindFromEntityId(pEnt->GetId()))
				{
					CUndo undo("Apply IK");
					pEditorObj->AcceptPhysicsState();
				}
			m_IKapplied = false;
			m_pEntPull->Release();
			m_pEntPull = 0;
			ReleasePhysEnt(m_pRope);
			aa.bAwake = 0;
			m_pEntAttach->Action(&aa, gEnv->pPhysicalWorld->GetPhysVars()->lastTimeStep ? -1 : 0);
			return true;
		}
		else if (m_timeHit)
		{
			float t = gEnv->pTimer->GetCurrTime();
			if (flags & MK_CONTROL)
			{
				pe_explosion expl;
				expl.epicenter = expl.epicenterImp = hit.pt;
				expl.rmin = (expl.r = expl.rmax = cv_HitExplR) * 0.2f;
				expl.impulsivePressureAtR = cv_HitExplPress0 + min(2.0f, t - m_timeHit) * (cv_HitExplPress1 - cv_HitExplPress0) * 0.5f;
				expl.rminOcc = 0.05f;
				expl.nOccRes = 16;
				gEnv->pPhysicalWorld->SimulateExplosion(&expl, 0, 0, ent_static | ent_sleeping_rigid | ent_rigid | ent_living | ent_independent);
				static IGeometry* pSphere = 0;
				if (!pSphere)
				{
					primitives::sphere sph;
					sph.center.zero();
					sph.r = 1;
					pSphere = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::sphere::type, &sph);
				}
				geom_world_data gwd;
				gwd.offset = expl.epicenter;
				gwd.scale = expl.r;
				gEnv->pSystem->GetIPhysRenderer()->DrawGeometry(pSphere, &gwd, 4, 1);
			}
			else if (hit.pCollider && hit.pCollider->GetType() != PE_STATIC && hit.pCollider->GetType() != PE_ROPE)
			{
				pe_player_dynamics pd;
				pd.timeImpulseRecover = 4;
				hit.pCollider->SetParams(&pd);
				pe_status_dynamics sd;
				hit.pCollider->GetStatus(&sd);
				pe_action_impulse ai;
				ai.pGridRefEnt = WORLD_ENTITY;
				ai.impulse = dir.normalized() * sd.mass * (cv_HitVel0 + min(2.0f, max(0.0f, t - m_timeHit - 0.3f)) * (cv_HitVel1 - cv_HitVel0) * 0.5f);
				ai.point = hit.pt;
				if (hit.pCollider->GetType() == PE_LIVING)
				{
					ai.iApplyTime = 0;
					ai.impulse *= 0.5f;
					hit.pCollider->Action(&ai);
					IEntity* pent = (IEntity*)hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
					ICharacterInstance* pChar = pent ? pent->GetCharacter(0) : 0;
					if (!pChar || !(hit.pCollider = pChar->GetISkeletonPose()->GetCharacterPhysics()))
						return true;
				}
				MARK_UNUSED hit.partid;
				if (gEnv->pPhysicalWorld->RayTraceEntity(hit.pCollider, org, dir, &hit))
				{
					ai.partid = hit.partid;
					hit.pCollider->Action(&ai);
				}
			}
			else
			{
				m_timeBullet = 0;
				ReleasePhysEnt(m_pBullet);
				ISurfaceType* pSurf = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeByName("mat_metal");
				pe_params_pos pp;
				pp.pos = org;
				IPhysicalEntity* bullet = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_PARTICLE, &pp);
				pe_params_particle ppt;
				ppt.flags = particle_single_contact | pef_log_collisions;
				ppt.heading = dir.normalized();
				ppt.velocity = cv_HitProjVel0 + min(2.0f, t - m_timeHit) * (cv_HitProjVel1 - cv_HitProjVel0) * 0.5f;
				ppt.mass = cv_HitProjMass;
				ppt.surface_idx = pSurf ? pSurf->GetId() : 0;
				ppt.size = 0.02f;
				bullet->SetParams(&ppt);
				(m_pBullet = bullet)->AddRef();
				m_timeBullet = 2.0f;
			}
			m_timeHit = 0;
			return true;
		}
	}
	else if (event == eMouseMove && m_pEntAttach && m_pEntPull)
	{
		UpdateAttachPos(SMiniCamera(view), point);
		switch (m_pEntPull->GetType())
		{
		case PE_ARTICULATED:
			if (CanUseIK(m_pEntPull) && !gEnv->pPhysicalWorld->GetPhysVars()->lastTimeStep)
			{
				pe_action_resolve_constraints arc;
				arc.lastDist = cv_IKprec;
				arc.mode = cv_IKStiffMode;
				arc.stopOnContact = (GetKeyState(VK_SHIFT) & 0x8000)!=0;
				pe_params_foreign_data pfd;
				if (m_pEntPull->SetParams(&pfd) < 2) // check for queued actions/commands; skip IK request if there are any
				{
					IEntity *pEnt = (IEntity*)m_pEntPull->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
					if (pEnt && !m_IKapplied)
						if (CEntityObject *pEditorObj = CEntityObject::FindFromEntityId(pEnt->GetId()))
							pEditorObj->AcceptPhysicsState();
					m_pEntPull->Action(&arc);
					m_IKapplied = true;
				}
			}
			break;
		case PE_ROPE:
		case PE_SOFT:
			m_pEntPull->Action(&pe_action_awake());
		}
		return true;
	}
	return false;
}

void CPhysPullTool::ClearRagdollConstraints()
{
	if (m_pRagdoll)
	{
		pe_action_update_constraint auc;
		auc.bRemove = 1;
		IterateRagdollConstraints([&](int id)->bool { auc.idConstraint = id+m_idConstr+1; m_pRagdoll->Action(&auc); return false; });
		m_maskConstr = 0;
		m_pRagdoll->Release(); 
		m_pRagdoll = nullptr; 
	}
}

void CPhysPullTool::SyncRagdollConstraintsMask(IPhysicalEntity *pent)
{
	if (!pent) return;
	pe_status_constraint sc;
	for(sc.id = m_idConstr+1, m_maskConstr = 0; sc.id < m_idConstr+1 + sizeof(m_maskConstr)*8; sc.id++)
	{
		m_maskConstr |= (uint64)pent->GetStatus(&sc) << (sc.id-m_idConstr-1);
	}
}

void CPhysPullTool::SyncCharacterToRagdoll(IPhysicalEntity *pentPhys)
{
	CRY_DISABLE_WARN_UNUSED_VARIABLES();
	IEntity *pent = (IEntity*)pentPhys->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
	for(int i = 0; i < pent->GetSlotCount(); i++)	if (pent->GetCharacter(i))
	{
		ISkeletonPose *skel = pent->GetCharacter(i)->GetISkeletonPose();
		if (skel->GetCharacterPhysics() == pentPhys) 
		{
			skel->SynchronizeWithPhysicalEntity(pentPhys);
			break;
		}
	}
	CRY_RESTORE_WARN_UNUSED_VARIABLES();
}

void CPhysPullTool::Display(SDisplayContext& dc)
{
	PhysicsVars* vars = gEnv->pPhysicalWorld->GetPhysVars();
	UpdateAttachPos(dc.camera, m_lastMousePos);
	pe_status_pos sp;
	sp.pGridRefEnt = m_pEntAttach;
	sp.partid = m_partid;
	bool drawPull = m_pEntPull && gEnv->pPhysicalWorld->GetPhysicalEntityId(m_pEntPull) >= 0;
	pe_params_foreign_data pfd;
	SMiniCamera mc(dc.camera);
	dc.SetColor(RGB(0, 160, 0));

	if (m_pEntPull && m_pEntPull->GetType() == PE_ROPE)
	{
		pe_params_rope pr;
		m_pEntPull->GetParams(&pr);
		dc.DrawLine(m_lastAttachPos, pr.pPoints[m_partid], ColorF(0, 0, 1), ColorF(0, 0, 1));
		m_pEntPull->GetParams(&pfd);
		dc.DrawTextLabel(pr.pPoints[m_partid] + mc.Screen2World(CPoint(m_lastMousePos.x, m_lastMousePos.y + 20), m_attachDist) - mc.Screen2World(m_lastMousePos, m_attachDist), 1.2f,
		                 gEnv->pSystem->GetIPhysRenderer()->GetForeignName(pfd.pForeignData, pfd.iForeignData, pfd.iForeignFlags), true);
	}
	else if (m_pEntPull && (m_pEntPull->GetType() != PE_SOFT || GetKeyState(VK_CONTROL) & 0x8000 && !m_nAttachPoints) && drawPull && !is_unused(m_partid) && m_pEntPull->GetStatus(&sp))
	{
		Vec3 ptObj = sp.pos + sp.q * m_locAttachPos;
		dc.DrawLine(m_lastAttachPos, ptObj, ColorF(0, 0, 1), ColorF(0, 0, 1));
		m_pEntPull->GetParams(&pfd);
		pe_status_dynamics sd;
		m_pEntPull->GetStatus(&sd);
		char buf[256];
		cry_sprintf(buf, "%s\n%.2fkg", gEnv->pSystem->GetIPhysRenderer()->GetForeignName(pfd.pForeignData, pfd.iForeignData, pfd.iForeignFlags), sd.mass);
		dc.DrawTextLabel(ptObj + mc.Screen2World(CPoint(m_lastMousePos.x, m_lastMousePos.y + 20), m_attachDist) - mc.Screen2World(m_lastMousePos, m_attachDist), 1.2f, buf, true);
	}
	if (drawPull)
		gEnv->pPhysicalWorld->DrawEntityHelperInformation(gEnv->pSystem->GetIPhysRenderer(), gEnv->pPhysicalWorld->GetPhysicalEntityId(m_pEntPull), 24354);
	float t = gEnv->pTimer->GetCurrTime(ITimer::ETIMER_UI);
	if (m_timeHit && t > m_timeHit + 0.3f)
	{
		dc.SetColor(RGB(180, 0, 0), 0.4f);
		dc.DrawBall(SMiniCamera(dc.camera).Screen2World(m_lastMousePos, 1), min(2.0f, t - m_timeHit - 0.3f) * 0.01f);
	}
	ReleasePhysEnt(m_pBullet, (m_timeBullet -= vars->lastTimeStep * (1 - vars->bSingleStepMode)) <= 0);

	if (m_pRagdoll)
	{
		if (m_pRagdoll != m_pEntPull)
			gEnv->pPhysicalWorld->DrawEntityHelperInformation(gEnv->pSystem->GetIPhysRenderer(), gEnv->pPhysicalWorld->GetPhysicalEntityId(m_pRagdoll), 24354);
		pe_status_constraint sc;
		IterateRagdollConstraints([&](int id) -> bool 
		{
			sc.id = id + m_idConstr+1;
			if (m_pRagdoll->GetStatus(&sc))
			{
				int active = !(sc.flags & constraint_inactive);
				dc.SetColor(RGB(100*active, 160*(1-active), 0), 1.0f);
				switch (sc.flags & (constraint_no_rotation | constraint_free_position))
				{
					case constraint_no_rotation   : dc.DrawSolidBox(sc.pt[active]-Vec3(m_sizeConstr), sc.pt[active]+Vec3(m_sizeConstr)); break;
					case constraint_free_position :	dc.DrawArrow(sc.pt[0], sc.pt[0] + sc.n*m_sizeConstr*2, m_sizeConstr*5); break;
					case 0                        : dc.DrawBall(sc.pt[active], m_sizeConstr); break;
				}
			}
			return false;
		});
	}

	pe_status_constraint sc;
	sc.id = m_idConstr;
	int icur;

	if (m_pEntPull && m_pEntPull->GetStatus(&sc))
		icur = (sc.flags & constraint_no_rotation) != 0;
	else if (!m_nAttachPoints && GetKeyState(VK_CONTROL) & 0x8000 && m_pEntPull && m_pEntPull->GetType() == PE_SOFT)
		icur = 3;
	else if (m_pEntPull && (m_pEntPull->GetType() == PE_SOFT || m_pEntPull->GetType() == PE_ROPE))
		icur = 0;
	else
	{
		bool shift = !!(GetKeyState(VK_SHIFT) & 0x8000), ctrl = !!(GetKeyState(VK_CONTROL) & 0x8000);
		icur = shift ? 2 : (ctrl ? 1 : 0);
		bool checkIK = shift && !vars->lastTimeStep;
		if (!m_pEntPull && (ctrl || checkIK))
		{
			ray_hit hit;
			Vec3 org = dc.camera->GetPosition(), dir = SMiniCamera(dc.camera).Screen2World(m_lastMousePos, 100) - org;
			if (gEnv->pPhysicalWorld->RayWorldIntersection(org, dir, ent_independent | ent_rigid | ent_sleeping_rigid, 
					rwi_pierceability(15) | rwi_colltype_any(geom_colltype_ray | geom_colltype0), &hit, 1))
			{
				switch (hit.pCollider->GetType())
				{
					case PE_SOFT: 
						if (ctrl) icur = 3; 
						break;
					case PE_ARTICULATED: 
						if (checkIK && CanUseIK(hit.pCollider)) icur = ctrl ? 5 : 4;
				}
			}
		}
	}
	SetCursor(m_hcur[icur]);
}

void CPhysPullTool::UpdateAttachPos(const SMiniCamera& cam, const CPoint& point)
{
	if (m_pEntAttach && m_pEntPull)
	{
		pe_params_pos pp;
		pp.pos = cam.Screen2World(point, m_attachDist);
		pp.q = Quat(cam.mtx);
		m_pEntAttach->SetParams(&pp);
		float t = gEnv->pTimer->GetCurrTime();
		if (t > m_timeMove + gEnv->pTimer->GetFrameTime() * 0.5f)
		{
			pe_action_set_velocity asv;
			asv.v = gEnv->pPhysicalWorld->GetPhysVars()->lastTimeStep ? (pp.pos - m_lastAttachPos) / (t - m_timeMove) : Vec3(ZERO);
			m_pEntAttach->Action(&asv);
			m_lastAttachPos = pp.pos;
			m_timeMove = t;
		}
	}
}

CPhysPullTool *CPhysPullTool::g_PhysTool = nullptr;

int CPhysPullTool::OnSimFinishedGlobal(const EventPhysSimFinished* epsf)
{
	if (g_PhysTool)
	{
		if (g_PhysTool->m_pEntPull == epsf->pEntity && epsf->numColl && GetKeyState(VK_SHIFT) & 0x8000)
			g_PhysTool->MouseCallback(nullptr, eMouseLUp, g_PhysTool->m_lastMousePos, 0);
	}
	return 1;
}

int CPhysPullTool::OnPostStepGlobal(const EventPhysPostStep* epps)
{
	if (g_PhysTool && g_PhysTool->m_pRagdoll == epps->pEntity)
		g_PhysTool->SyncCharacterToRagdoll(epps->pEntity);
	return 1;
}

CPhysPullTool::CPhysPullTool() : m_pEntPull(0), m_pEntAttach(0), m_pBullet(0), m_pRope(0), m_pRagdoll(0), m_timeHit(0)
{
	m_hcur[0] = AfxGetApp()->LoadCursor(IDC_PHYS_PINCH);
	m_hcur[1] = AfxGetApp()->LoadCursor(IDC_PHYS_GRAB);
	m_hcur[2] = AfxGetApp()->LoadCursor(IDC_PHYS_AIM);
	m_hcur[3] = AfxGetApp()->LoadCursor(IDC_SCISSORS);
	m_hcur[4] = AfxGetApp()->LoadCursor(IDC_PHYS_PIN);
	m_hcur[5] = AfxGetApp()->LoadCursor(IDC_PHYS_FORK);
	GetIEditorImpl()->RegisterNotifyListener(this);
	if (QAction* pAction = GetIEditorImpl()->GetICommandManager()->GetAction("physics.set_physics_tool"))
	{
		pAction->setCheckable(true);
		pAction->setChecked(true);
	}
	if (gEnv->pPhysicalWorld)
	{
		gEnv->pPhysicalWorld->AddEventClient(EventPhysSimFinished::id, (int(*)(const EventPhys*))OnSimFinishedGlobal, 1, 1000);
		gEnv->pPhysicalWorld->AddEventClient(EventPhysPostStep::id, (int(*)(const EventPhys*))OnPostStepGlobal, 1, -1000);
	}
	g_PhysTool = this;
}

void CPhysPullTool::RegisterCVars()
{
	REGISTER_CVAR2("ed_PhysToolHitVelMin", &cv_HitVel0, cv_HitVel0, VF_NULL, "Minimal velocity in insta-hit mode");
	REGISTER_CVAR2("ed_PhysToolHitVelMax", &cv_HitVel1, cv_HitVel1, VF_NULL, "Maximal velocity in insta-hit mode");
	REGISTER_CVAR2("ed_PhysToolHitProjMass", &cv_HitProjMass, cv_HitProjMass, VF_NULL, "Projectile mass in projectile hit mode (on statics)");
	REGISTER_CVAR2("ed_PhysToolHitProjVel0", &cv_HitProjVel0, cv_HitProjVel0, VF_NULL, "Minimal projectile velocity in projectile hit mode (on statics)");
	REGISTER_CVAR2("ed_PhysToolHitProjVel1", &cv_HitProjVel1, cv_HitProjVel1, VF_NULL, "Maximal projectile velocity in projectile hit mode (on statics)");
	REGISTER_CVAR2("ed_PhysToolHitExplR", &cv_HitExplR, cv_HitExplR, VF_NULL, "Hit explosion radius");
	REGISTER_CVAR2("ed_PhysToolHitExplPress0", &cv_HitExplPress0, cv_HitExplPress0, VF_NULL, "Hit explosion minimal pressure");
	REGISTER_CVAR2("ed_PhysToolHitExplPress1", &cv_HitExplPress1, cv_HitExplPress1, VF_NULL, "Hit explosion maximal pressure");
	REGISTER_CVAR2("ed_PhysToolIKPrec", &cv_IKprec, cv_IKprec, VF_NULL, "Precision ok ragdoll IK posing (in meters)");
	REGISTER_CVAR2("ed_PhysToolIKStiffMode", &cv_IKStiffMode, cv_IKStiffMode, VF_NULL, "Use stiff mode for IK posing (favors pose preservation more than energy minimization)");
}

CPhysPullTool::~CPhysPullTool()
{
	g_PhysTool = nullptr;
	if (m_pEntPull)
	{
		pe_action_update_constraint auc;
		auc.idConstraint = m_idConstr;
		auc.bRemove = 1;
		m_pEntPull->Action(&auc);
		m_pEntPull->Release();
		m_pEntPull = 0;
	}
	ReleasePhysEnt(m_pRope);
	ReleasePhysEnt(m_pBullet);
	ReleasePhysEnt(m_pEntAttach);
	ClearRagdollConstraints();
	m_timeBullet = 0;
	GetIEditorImpl()->UnregisterNotifyListener(this);
	if (QAction* pAction = GetIEditorImpl()->GetICommandManager()->GetAction("physics.set_physics_tool"))
		pAction->setChecked(false);
	gEnv->pPhysicalWorld->RemoveEventClient(EventPhysSimFinished::id, (int(*)(const EventPhys*))OnSimFinishedGlobal, 1);
	gEnv->pPhysicalWorld->RemoveEventClient(EventPhysPostStep::id, (int(*)(const EventPhys*))OnPostStepGlobal, 1);
}
