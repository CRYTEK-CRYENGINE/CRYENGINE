// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "PhysTool.h"
#include "Viewport.h"
#include "Objects/DisplayContext.h"

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

float CPhysPullTool::cv_HitVel0 = 1;
float CPhysPullTool::cv_HitVel1 = 15;
float CPhysPullTool::cv_HitProjMass = 0.5f;
float CPhysPullTool::cv_HitProjVel0 = 50;
float CPhysPullTool::cv_HitProjVel1 = 450;
float CPhysPullTool::cv_HitExplR = 4.0f;
float CPhysPullTool::cv_HitExplPress0 = 50;
float CPhysPullTool::cv_HitExplPress1 = 250;

bool CPhysPullTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	m_lastMousePos = point;
	Vec3 org = view->GetViewTM().GetTranslation(), dir = SMiniCamera(view).Screen2World(point, 100) - org;
	ray_hit hit;
	int nHits = event != eMouseLDown && event != eMouseLUp ? 0 :
	            gEnv->pPhysicalWorld->RayWorldIntersection(org, dir, ent_terrain | ent_static | ent_sleeping_rigid | ent_rigid | ent_living | ent_independent,
	                                                       rwi_pierceability(15) | rwi_colltype_any(geom_colltype_solid | geom_colltype_ray), &hit, 1);

	if (event == eMouseLDown)
	{
		if (flags & MK_SHIFT)
		{
			m_timeHit = gEnv->pTimer->GetCurrTime();
			return true;
		}
		if (nHits)
		{
			pe_params_pos pp;
			pp.pos = hit.pt;
			if (!m_pEntAttach)
			{
				pe_params_flags pf; pf.flagsOR = pef_never_affect_triggers;
				(m_pEntAttach = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_RIGID,&pf))->AddRef();
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
			}

			pe_status_dynamics sd;
			hit.pCollider->GetStatus(&sd);
			m_partid = hit.partid;
			pe_status_pos sp;
			sp.pGridRefEnt = m_pEntAttach;
			sp.partid = hit.partid;
			hit.pCollider->GetStatus(&sp);
			m_locAttachPos = (hit.pt - sp.pos) * sp.q;

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
					pt[1] = ptEdge;	// slice over the edge if close enough
				m_pEntPull->Action(&as);
			}
			pe_action_awake aa;
			m_pEntPull->Action(&aa);
			m_pEntPull->Release();
			m_pEntPull = 0;
			ReleasePhysEnt(m_pRope);
			aa.bAwake = 0;
			m_pEntAttach->Action(&aa, -1);
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
				;
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
		case PE_ROPE:
		case PE_SOFT:
			m_pEntPull->Action(&pe_action_awake());
		}
		return true;
	}
	return false;
}

void CPhysPullTool::Display(DisplayContext& dc)
{
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
	{
		gEnv->pPhysicalWorld->DrawEntityHelperInformation(gEnv->pSystem->GetIPhysRenderer(), gEnv->pPhysicalWorld->GetPhysicalEntityId(m_pEntPull), 24354);
	}
	float t = gEnv->pTimer->GetCurrTime();
	if (m_timeHit && t > m_timeHit + 0.3f)
	{
		dc.SetColor(RGB(180, 0, 0), 0.5f);
		dc.DrawBall(SMiniCamera(dc.camera).Screen2World(m_lastMousePos, 1), min(2.0f, t - m_timeHit - 0.3f) * 0.01f);
	}
	PhysicsVars* vars = gEnv->pPhysicalWorld->GetPhysVars();
	ReleasePhysEnt(m_pBullet, (m_timeBullet -= vars->lastTimeStep * (1 - vars->bSingleStepMode)) <= 0);

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
		icur = GetKeyState(VK_SHIFT) & 0x8000 ? 2 : (GetKeyState(VK_CONTROL) & 0x8000 ? 1 : 0);
		if (!m_pEntPull && GetKeyState(VK_CONTROL) & 0x8000)
		{
			ray_hit hit;
			Vec3 org = dc.camera->GetPosition(), dir = SMiniCamera(dc.camera).Screen2World(m_lastMousePos, 100) - org;
			if (gEnv->pPhysicalWorld->RayWorldIntersection(org, dir, ent_independent, rwi_pierceability(15), &hit, 1) && hit.pCollider->GetType() == PE_SOFT)
				icur = 3;
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
			asv.v = (pp.pos - m_lastAttachPos) / (t - m_timeMove);
			m_pEntAttach->Action(&asv);
			m_lastAttachPos = pp.pos;
			m_timeMove = t;
		}
	}
}

CPhysPullTool::CPhysPullTool() : m_pEntPull(0), m_pEntAttach(0), m_pBullet(0), m_pRope(0), m_timeHit(0)
{
	m_hcur[0] = AfxGetApp()->LoadCursor(IDC_PHYS_PINCH);
	m_hcur[1] = AfxGetApp()->LoadCursor(IDC_PHYS_GRAB);
	m_hcur[2] = AfxGetApp()->LoadCursor(IDC_PHYS_AIM);
	m_hcur[3] = AfxGetApp()->LoadCursor(IDC_SCISSORS);
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
}

CPhysPullTool::~CPhysPullTool()
{
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
	m_timeBullet = 0;
}

