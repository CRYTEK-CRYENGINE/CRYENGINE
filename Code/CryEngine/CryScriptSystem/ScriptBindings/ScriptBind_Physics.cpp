// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ScriptBind_Physics.cpp
//  Version:     v1.00
//  Created:     22/10/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ScriptBind_Physics.h"

#include <CrySystem/ISystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryPhysics/IPhysics.h>
#include <CryEntitySystem/IEntitySystem.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CScriptBind_Physics::CScriptBind_Physics(IScriptSystem* pScriptSystem, ISystem* pSystem)
{
	m_pSystem = NULL;
	m_p3DEngine = NULL;

	CScriptableBase::Init(pScriptSystem, pSystem);
	SetGlobalName("Physics");
	m_pSystem = pSystem;
	m_p3DEngine = gEnv->p3DEngine;
	m_pPhysicalWorld = m_pSystem->GetIPhysicalWorld();

#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_Physics::

	SCRIPT_REG_TEMPLFUNC(SimulateExplosion, "tableExplosion");
	SCRIPT_REG_TEMPLFUNC(RegisterExplosionShape, "sGeometryFile,fSize,nIdMaterial,fProbability,sSplintersFile,fSplintersOffset,sSplintersCloudEffect");
	SCRIPT_REG_TEMPLFUNC(RegisterExplosionCrack, "sGeometryFile,nIdMaterial");
	SCRIPT_REG_FUNC(RayWorldIntersection);
	SCRIPT_REG_TEMPLFUNC(RayTraceCheck, "vSrc,vTrg,hSkipEntityId1,hSkipEntityId2");
	SCRIPT_REG_TEMPLFUNC(SamplePhysEnvironment, "vPoint,fRadius");
}

//////////////////////////////////////////////////////////////////////////
CScriptBind_Physics::~CScriptBind_Physics()
{
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Physics::SimulateExplosion(IFunctionHandler* pH, SmartScriptTable explisionTable)
{
	{
		// Must be in local scope.
		CScriptSetGetChain params(explisionTable);

		pe_explosion expl;
		params.GetValue("pos", expl.epicenter);
		params.GetValue("radius", expl.r);
		params.GetValue("direction", expl.explDir);
		params.GetValue("impulse_pos", expl.epicenterImp);
		params.GetValue("impulse_pressure", expl.impulsivePressureAtR);
		params.GetValue("rmin", expl.rmin);
		params.GetValue("rmax", expl.rmax);
		params.GetValue("hole_size", expl.holeSize);

		m_pPhysicalWorld->SimulateExplosion(&expl);
	}

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Physics::RegisterExplosionShape(IFunctionHandler* pH, const char* sGeometryFile, float fSize, int nIdMaterial, float fProbability,
                                                const char* sSplintersFile, float fSplintersOffset, const char* sSplintersCloudEffect)
{
	//////////////////////////////////////////////////////////////////////////
	// Remove all this.
	//////////////////////////////////////////////////////////////////////////
	IStatObj* pObj = gEnv->p3DEngine->LoadStatObj(sGeometryFile, "#ForceBreakable", NULL, false);
	if (!pObj || pObj->IsDefaultObject())
	{
		ScriptWarning("<RegisterExplosionShape> Object file %s not found", sGeometryFile);
		return pH->EndFunction();
	}
	pObj->AddRef();
	pObj->GetIndexedMesh(true); // prepare idxMesh now

	if (sSplintersFile && *sSplintersFile != 0)      // if sSplintersFile was specified
	{
		IStatObj* pSplinters = gEnv->p3DEngine->LoadStatObj(sSplintersFile, NULL, NULL, false);
		if (pSplinters)
		{
			pObj->SetSubObjectCount(pObj->GetSubObjectCount() + 1);
			IStatObj::SSubObject* pSubObj = pObj->GetSubObject(pObj->GetSubObjectCount() - 1);
			pSubObj->nType = STATIC_SUB_OBJECT_MESH;
			pSubObj->bHidden = true;
			pSubObj->name = "splinters";
			(pSubObj->pStatObj = pSplinters)->AddRef();
			pSubObj->helperSize.x = fSplintersOffset;
			nIdMaterial |= 1 << 16;

			if (*sSplintersCloudEffect)
			{
				pSplinters->SetSubObjectCount(pSplinters->GetSubObjectCount() + 1);
				pSplinters->SetFlags(pSplinters->GetFlags() & ~STATIC_OBJECT_COMPOUND);
				pSubObj = pSplinters->GetSubObject(pSplinters->GetSubObjectCount() - 1);
				pSubObj->nType = STATIC_SUB_OBJECT_DUMMY;
				pSubObj->bHidden = true;
				pSubObj->name = "splinters_cloud";
				pSubObj->properties = sSplintersCloudEffect;
			}
		}
	}

	phys_geometry* pPhysGeom = pObj->GetPhysGeom();
	if (pPhysGeom)
	{
		m_pPhysicalWorld->AddExplosionShape(pPhysGeom->pGeom, fSize, nIdMaterial, fProbability);
	}
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Physics::RegisterExplosionCrack(IFunctionHandler* pH, const char* sGeometryFile, int nIdMaterial)
{
	IStatObj* pObj = gEnv->p3DEngine->LoadStatObj(sGeometryFile, "#ForceBreakable", NULL, false);
	if (!pObj || pObj->IsDefaultObject())
	{
		ScriptWarning("<RegisterExplosionCrack> Object file %s not found", sGeometryFile);
		return pH->EndFunction();
	}
	pObj->AddRef();
	Vec3 vtx[3] = { pObj->GetHelperPos("1"), pObj->GetHelperPos("2"), pObj->GetHelperPos("3") };
	//@TODO: restore it.
	m_pPhysicalWorld->GetGeomManager()->RegisterCrack(pObj->GetPhysGeom()->pGeom, vtx, 0);
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Physics::RayWorldIntersection(IFunctionHandler* pH)
{
	assert(pH->GetParamCount() >= 3 && pH->GetParamCount() <= 7);

	Vec3 vPos(0, 0, 0);
	Vec3 vDir(0, 0, 0);
	ScriptHandle skipId1;
	ScriptHandle skipId2;
	IEntity* skipEnt1 = 0;
	IEntity* skipEnt2 = 0;

	int nMaxHits, iEntTypes = ent_all;

	SmartScriptTable hitTable(m_pSS);

	if (!pH->GetParams(vPos, vDir, nMaxHits, iEntTypes))
		return pH->EndFunction();

	int nParams = pH->GetParamCount();
	if (nParams >= 5 && pH->GetParamType(5) != svtNull && pH->GetParam(5, skipId1))
		skipEnt1 = gEnv->pEntitySystem->GetEntity((EntityId)skipId1.n);
	if (nParams >= 6 && pH->GetParamType(6) != svtNull && pH->GetParam(6, skipId2))
		skipEnt2 = gEnv->pEntitySystem->GetEntity((EntityId)skipId2.n);

	bool bHitTablePassed = (nParams >= 7);

	if (bHitTablePassed)
		pH->GetParam(7, hitTable);

	if (nMaxHits > 10)
		nMaxHits = 10;

	ray_hit RayHit[10];

	//filippo: added support for skip certain entities.
	int skipIdCount = 0;

	IPhysicalEntity* skipPhys[2] = { 0, 0 };

	if (skipEnt1)
	{
		++skipIdCount;
		skipPhys[0] = skipEnt1->GetPhysics();
	}
	if (skipEnt2)
	{
		++skipIdCount;
		skipPhys[1] = skipEnt2->GetPhysics();
	}

	Vec3 src = vPos;
	Vec3 dir = vDir;

	int nHits = m_pSystem->GetIPhysicalWorld()->RayWorldIntersection(src, dir, iEntTypes,
	                                                                 geom_colltype0 << rwi_colltype_bit | rwi_stop_at_pierceable, RayHit, nMaxHits, skipPhys, skipIdCount);

	for (int i = 0; i < nHits; i++)
	{
		SmartScriptTable pHitObj(m_pSS);
		ray_hit& Hit = RayHit[i];
		pHitObj->SetValue("pos", Hit.pt);
		pHitObj->SetValue("normal", Hit.n);
		pHitObj->SetValue("dist", Hit.dist);
		pHitObj->SetValue("surface", Hit.surface_idx);
		IEntity* pEntity = (IEntity*) Hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
		if (pEntity)
			pHitObj->SetValue("entity", pEntity->GetScriptTable());
		else
		{
			if (Hit.pCollider->GetiForeignData() == PHYS_FOREIGN_ID_STATIC)
			{
				IRenderNode* pRN = (IRenderNode*)Hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC);
				if (pRN)
					pHitObj->SetValue("renderNode", ScriptHandle(pRN));
			}
			else if (Hit.pCollider->GetiForeignData() == PHYS_FOREIGN_ID_FOLIAGE)
			{
				IRenderNode* pRN = ((IFoliage*)Hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_FOLIAGE))->GetIRenderNode();
				if (pRN)
					pHitObj->SetValue("renderNode", ScriptHandle(pRN));
			}
		}
		hitTable->SetAt(i + 1, pHitObj);
	}

	if (bHitTablePassed)
		return pH->EndFunction(nHits);

	return pH->EndFunction(*hitTable);
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Physics::RayTraceCheck(IFunctionHandler* pH, Vec3 src, Vec3 dst, ScriptHandle skipEntityId1, ScriptHandle skipEntityId2)
{
	SCRIPT_CHECK_PARAMETERS(4);

	IEntity* skipEnt1 = gEnv->pEntitySystem->GetEntity((EntityId)skipEntityId1.n);
	IEntity* skipEnt2 = gEnv->pEntitySystem->GetEntity((EntityId)skipEntityId2.n);
	IPhysicalEntity* skipPhys[2] = { 0, 0 };

	if (skipEnt1) skipPhys[0] = skipEnt1->GetPhysics();
	if (skipEnt2) skipPhys[1] = skipEnt2->GetPhysics();

	ray_hit RayHit;
	//TODO? add an extraparam to specify what kind of objects to check? now its world and static
	int nHits = m_pSystem->GetIPhysicalWorld()->RayWorldIntersection(src, dst - src, ent_static | ent_terrain, rwi_ignore_noncolliding | rwi_stop_at_pierceable, &RayHit, 1, skipPhys, 2);

	return pH->EndFunction((bool)(nHits == 0));
}
//////////////////////////////////////////////////////////////////////////

int CScriptBind_Physics::SamplePhysEnvironment(IFunctionHandler* pH)
{
	int nEnts, i, nHits = 0, objtypes = ent_static | ent_rigid | ent_sleeping_rigid | ent_sort_by_mass | ent_allocate_list | ent_addref_results;
	IPhysicalEntity* ents[128];
	pe_status_nparts snp;
	pe_status_pos sp;
	IPhysicalEntity** pEnts = ents;
	geom_world_data gwd;
	IGeometry* pSphere;
	primitives::sphere sph;
	intersection_params ip;
	geom_contact* pcontacts;
	IPhysicalWorld* pWorld = m_pSystem->GetIPhysicalWorld();
	IEntity* pEntity;
	SmartScriptTable pObj(m_pSS);
	ip.bStopAtFirstTri = ip.bNoBorder = ip.bNoAreaContacts = true;
	ip.bThreadSafe = true;

	if (!pH->GetParams(sph.center, sph.r))
		return pH->EndFunction();
	if (pH->GetParamCount() > 2)
		pH->GetParam(3, objtypes);
	pSphere = pWorld->GetGeomManager()->CreatePrimitive(primitives::sphere::type, &sph);

	nEnts = pWorld->GetEntitiesInBox(sph.center - Vec3(sph.r), sph.center + Vec3(sph.r), pEnts, objtypes, CRY_ARRAY_COUNT(ents));
	for (i = 0; i < nEnts; pEnts[i++]->Release())
		for (sp.ipart = pEnts[i]->GetStatus(&snp) - 1; sp.ipart >= 0; sp.ipart--)
		{
			sp.partid = -1;
			pEnts[i]->GetStatus(&sp);
			gwd.offset = sp.pos;
			gwd.R = Matrix33(sp.q);
			gwd.scale = sp.scale;
			WriteLockCond lock;
			if ((sp.flagsOR & (geom_colltype0 | geom_no_coll_response)) == geom_colltype0 && sp.pGeomProxy->IntersectLocked(pSphere, &gwd, 0, &ip, pcontacts, lock))
			{
				++nHits;
				if (pEntity = (IEntity*)pEnts[i]->GetForeignData(PHYS_FOREIGN_ID_ENTITY))
					pObj->SetAt(nHits * 3 - 2, pEntity->GetScriptTable());
				else
					pObj->SetNullAt(nHits * 3 - 2);
				pObj->SetAt(nHits * 3 - 1, sp.partid);
				pObj->SetAt(nHits * 3, pWorld->GetPhysicalEntityId(pEnts[i]));
				if (pEnts[i]->GetType() != PE_ARTICULATED)
					break;
			}
		}
	pSphere->Release();
	if (pEnts != ents)
		gEnv->pPhysicalWorld->GetPhysUtils()->DeletePointer(pEnts);

	return pH->EndFunction(*pObj);
}
