// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObjectPhysicsManager.h"
#include "Objects/BaseObject.h"
#include "EntityObject.h"
#include "BrushObject.h"
#include "GameEngine.h"
#include "Util/BoostPythonHelpers.h"
#include "Material/MaterialManager.h"
#include "QT/Widgets/QWaitProgress.h"
#include "FilePathUtil.h"
#include <CryMath/Random.h>

#define MAX_OBJECTS_PHYS_SIMULATION_TIME (5)

namespace
{
void PyPhysicsSimulateObjects()
{
	CObjectPhysicsManager* pPhysicsManager
	  = static_cast<CObjectManager*>(GetIEditorImpl()->GetObjectManager())->GetPhysicsManager();
	if (pPhysicsManager)
	{
		pPhysicsManager->Command_SimulateObjects();
	}
}

void PyPhysicsResetState(const char* objectName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objectName);
	if (pObject)
		pObject->OnEvent(EVENT_PHYSICS_RESETSTATE);
}

void PyPhysicsGetState(const char* objectName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objectName);
	if (pObject)
		pObject->OnEvent(EVENT_PHYSICS_GETSTATE);
}

void PyPhysicsSimulateSelection()
{
	CObjectPhysicsManager* pPhysicsManager
	  = static_cast<CObjectManager*>(GetIEditorImpl()->GetObjectManager())->GetPhysicsManager();
	if (pPhysicsManager)
		pPhysicsManager->SimulateSelectedObjectsPositions();
}

void PyPhysicsResetSelection()
{
	CObjectPhysicsManager* pPhysicsManager
	  = static_cast<CObjectManager*>(GetIEditorImpl()->GetObjectManager())->GetPhysicsManager();
	if (pPhysicsManager)
	{
		pPhysicsManager->Command_ResetPhysicsState();
	}
}

void PyPhysicsGetStateSelection()
{
	CObjectPhysicsManager* pPhysicsManager
	  = static_cast<CObjectManager*>(GetIEditorImpl()->GetObjectManager())->GetPhysicsManager();
	if (pPhysicsManager)
	{
		pPhysicsManager->Command_GetPhysicsState();
	}
}

void PyPhysicsGenerateJoints()
{
	CObjectPhysicsManager* pPhysicsManager
	  = static_cast<CObjectManager*>(GetIEditorImpl()->GetObjectManager())->GetPhysicsManager();
	if (pPhysicsManager)
	{
		pPhysicsManager->Command_GenerateJoints();
	}
}
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyPhysicsSimulateObjects, physics, simulate_objects,
                                     "Applies the physics simulation.",
                                     "physics.simulate_objects()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyPhysicsResetState, physics, reset_state,
                                     "Signals that physics state must be reset on the object.",
                                     "physics.reset_state(str entityName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyPhysicsGetState, physics, get_state,
                                     "Signals that the object should accept its physical state from game.",
                                     "physics.get_state(str entityName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyPhysicsSimulateSelection, physics, simulate_selection,
                                     "Applies the physics simulation to the selected objects.",
                                     "physics.simulate_selection()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyPhysicsResetSelection, physics, reset_state_selection,
                                     "Signals that physics state must be reset on the selected objects.",
                                     "physics.reset_state_selection");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyPhysicsGetStateSelection, physics, get_state_selection,
                                     "Signals that the object should accept its physical state from game.",
                                     "physics.get_state_selection");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyPhysicsGenerateJoints, physics, generate_joints,
                                     "Signals that the object should accept its physical state from game.",
                                     "physics.generate_joints");

//////////////////////////////////////////////////////////////////////////
CObjectPhysicsManager::CObjectPhysicsManager()
{
	m_fStartObjectSimulationTime = 0;
	m_pProgress = 0;
	m_bSimulatingObjects = false;
	m_wasSimObjects = 0;
}

//////////////////////////////////////////////////////////////////////////
CObjectPhysicsManager::~CObjectPhysicsManager()
{
	SAFE_DELETE(m_pProgress);
}

//////////////////////////////////////////////////////////////////////////
void CObjectPhysicsManager::Command_SimulateObjects()
{
	SimulateSelectedObjectsPositions();
}

/////////////////////////////////////////////////////////////////////////
void CObjectPhysicsManager::Command_ResetPhysicsState()
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	for (int i = 0; i < pSelection->GetCount(); i++)
	{
		pSelection->GetObject(i)->OnEvent(EVENT_PHYSICS_RESETSTATE);
	}
}

/////////////////////////////////////////////////////////////////////////
void CObjectPhysicsManager::Command_GetPhysicsState()
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	for (int i = 0; i < pSelection->GetCount(); i++)
	{
		pSelection->GetObject(i)->OnEvent(EVENT_PHYSICS_GETSTATE);
	}
}

/////////////////////////////////////////////////////////////////////////

IGeometry* PhysicalizeIdxMesh(IStatObj* pSrcObj, bool bFlip = false)
{
	CMesh* pMesh = pSrcObj->GetIndexedMesh(true)->GetMesh();
	int i, nTris = pMesh->GetIndexCount() / 3;
	if (bFlip)
		for (i = 0; i < nTris; i++)
			std::swap(pMesh->m_pIndices[i * 3 + 1], pMesh->m_pIndices[i * 3 + 2]);
	int* pIdx = new int[nTris];
	for (i = 0; i < nTris; i++)
		pIdx[i] = i;
	char* pMats = new char[nTris];
	memset(pMats, 0, nTris * sizeof(pMats[0]));
	for (i = 0; i < pMesh->m_subsets.size(); i++)
		for (int idx0 = pMesh->m_subsets[i].nFirstIndexId / 3, idx = idx0, nidx = pMesh->m_subsets[i].nNumIndices / 3; idx < idx0 + nidx; pMats[idx++] = pMesh->m_subsets[i].nMatID)
			;
	IGeometry* pGeom = gEnv->pPhysicalWorld->GetGeomManager()->CreateMesh(pMesh->m_pPositions, pMesh->m_pIndices, pMats, pIdx, nTris,
	                                                                      mesh_OBB | mesh_multicontact2 | mesh_keep_vtxmap | mesh_full_serialization, 0);
	pGeom->SetForeignData(pSrcObj, 0);
	pSrcObj->SetFlags(pSrcObj->GetFlags() & ~STATIC_OBJECT_CANT_BREAK);
	delete[] pMats;
	delete[] pIdx;
	pGeom->Lock();
	pGeom->Unlock();
	for (i = 0; i < pMesh->m_subsets.size(); i++)
		pMesh->m_subsets[i].nPhysicalizeType = PHYS_GEOM_TYPE_DEFAULT;
	return pGeom;
}

IGeometry* GetTriMesh(IGeometry* pGeom, bool bClone, bool bFlip, int defaultMatId, IGeometry* pGeomFallback)
{
	IGeomManager* pGeoman = gEnv->pPhysicalWorld->GetGeomManager();
	IGeometry* pMesh;
	if (pGeom->GetType() == GEOM_TRIMESH)
		pMesh = bClone ? pGeoman->CloneGeometry(pGeom) : pGeom;
	else if (pGeom->GetType() == GEOM_BOX)
	{
		primitives::box* pbox = (primitives::box*)pGeom->GetData();
		Vec3 vtx[8];
		unsigned short idx[12 * 3] = { 0, 2, 1, 3, 1, 2, 4, 5, 6, 7, 6, 5, 0, 1, 4, 5, 4, 1, 2, 6, 3, 7, 3, 6, 0, 4, 2, 6, 2, 4, 1, 3, 5, 7, 5, 3 };
		char mats[12];
		memset(mats, defaultMatId, sizeof(mats));
		for (int i = 0; i < 8; i++)
			vtx[i] = pbox->center + (Vec3((i << 1 & 2) - 1, (i & 2) - 1, (i >> 1 & 2) - 1) * Diag33(pbox->size)) * pbox->Basis;
		pMesh = pGeoman->CreateMesh(vtx, idx, mats, 0, 12, mesh_SingleBB | mesh_multicontact2, 0);
		pMesh->Lock();
		pMesh->Unlock();
	}
	else
	{
		pGeomFallback->AddRef();
		return pGeomFallback;
	}
	if (bFlip)
	{
		mesh_data* md = (mesh_data*)pMesh->GetData();
		for (int i = 0; i < md->nTris; i++)
		{
			std::swap(md->pIndices[i * 3 + 1], md->pIndices[i * 3 + 2]);
			std::swap(md->pTopology[i].ibuddy[0], md->pTopology[i].ibuddy[2]);
			md->pNormals[i].Flip();
		}
		md->flags |= mesh_multicontact2;
		pMesh->SetData(md);
	}
	return pMesh;
}

template<class T> inline void remap(T& dst, int* pMap)
{
	T negmask = dst >> 31;
	dst = pMap[dst & ~negmask] | dst & negmask;
}

void RemapStatObjMatids(IStatObj* pStatObj, int* pMap)
{
	CMesh* pMesh = pStatObj->GetIndexedMesh(true)->GetMesh();
	for (int i = 0; i < pMesh->m_subsets.size(); remap(pMesh->m_subsets[i++].nMatID, pMap))
		;
	phys_geometry* pPhysGeom = pStatObj->GetPhysGeom();
	pPhysGeom->surface_idx = pMap[pPhysGeom->surface_idx];
	if (pPhysGeom->pGeom->GetType() == GEOM_TRIMESH)
	{
		mesh_data* md = (mesh_data*)pPhysGeom->pGeom->GetData();
		for (int j = 0; j < md->nTris && md->pMats; remap(md->pMats[j++], pMap))
			;
	}
}

bool VerifySingleProxy(IStatObj* pStatObj)
{
	if (pStatObj->GetSubObjectCount())
	{
		bool res = true;
		for (int i = 0; i < pStatObj->GetSubObjectCount(); i++)
			if (IStatObj::SSubObject* pSubObj = pStatObj->GetSubObject(i))
				if (pSubObj->pStatObj && !VerifySingleProxy(pSubObj->pStatObj))
					gEnv->pLog->LogToConsole("Error: subobject %s has multiple phys proxies", pSubObj->name.c_str()), res = false;
		return res;
	}
	int nprox = 0;
	while (pStatObj->GetPhysGeom(nprox++))
		;
	return !(nprox > 2 || nprox == 2 && (!pStatObj->GetPhysGeom() || !pStatObj->GetPhysGeom(PHYS_GEOM_TYPE_NO_COLLIDE) && pStatObj->GetPhysGeom(PHYS_GEOM_TYPE_NO_COLLIDE)));
}

void AlignCutTemplate(const IStatObj* pSrcObj, const IStatObj* pObjTpl, int align, const Vec3& tplOffs, float tplScale, Matrix33& R, Vec3& offset, float& scale)
{
	AABB boxTpl = pObjTpl->GetAABB(), boxSrc = pSrcObj->GetAABB();
	Vec3 szSrc = boxSrc.GetSize(), szTpl = boxTpl.GetSize();
	int imaxTpl = idxmax3(szTpl), iminTpl = idxmin3(szTpl), imaxSrc = idxmax3(szSrc), iminSrc = idxmin3(szSrc) != imaxSrc ? idxmin3(szSrc) : 2 - imaxSrc;
	if (align == -1)
	{
		if (szTpl[imaxTpl] - szTpl[iminTpl] > szTpl[imaxTpl] * 0.1f)
		{
			R.SetZero();
			R(imaxTpl, imaxSrc) = 1;
			R(iminTpl, iminSrc) = 1;
			R(3 - imaxTpl - iminTpl, 3 - imaxSrc - iminSrc) = 1;
			R(iminTpl, iminSrc) = sgnnz(R.Determinant());
		}
	}
	else if (align >= 0)
	{
		if (align > 23)
			align = cry_random(0, 23);
		R.SetZero();
		int xnew = align >> 3, flipy = align >> 2 & 1, ynew = decm3(xnew) * flipy + incm3(xnew) * (1 - flipy), sgx = align >> 1 & 1, sgy = align & 1;
		R(0, xnew) = 1 - sgx * 2;
		R(1, ynew) = 1 - sgy * 2;
		R(2, 3 - xnew - ynew) = 1 - (sgx ^ sgy ^ flipy) * 2;
	}
	Matrix33 Rabs = R;
	Vec3 rscale = (szTpl * Diag33(Rabs.Fabs() * szSrc * tplScale).invert());
	scale = min(min(rscale.x, rscale.y), rscale.z) * 0.95f;
	offset = boxTpl.GetCenter() - R * (boxSrc.GetCenter() + Diag33(boxSrc.GetSize()) * tplOffs) * scale;
}

void CObjectPhysicsManager::Command_GenerateJoints()
{
	SEntityProximityQuery epq;
	if (!(epq.pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("JointGen")))
		return;
	IGeomManager* pGeoman = gEnv->pPhysicalWorld->GetGeomManager();
	primitives::sphere sph;
	sph.center.zero();
	sph.r = 1;
	primitives::ray aray;
	aray.origin.zero();
	aray.dir = Vec3(0, 0, 1);
	IGeometry* pSph = pGeoman->CreatePrimitive(primitives::sphere::type, &sph), * pRay = pGeoman->CreatePrimitive(primitives::ray::type, &aray);
	geom_world_data gwd[2];
	intersection_params ip;
	ip.bNoAreaContacts = ip.bNoBorder = ip.bStopAtFirstTri = true;
	ip.maxUnproj = 0;
	geom_contact* pcont;
	Matrix34 tm;
	pe_params_structural_joint psj;
	psj.id = joint_impulse;
	psj.partid[1] = -1;
	psj.bBroken = 2;
	psj.partidEpicenter = -1;
	SmartScriptTable props;
	string path, fname;
	int nCutTot = 0;

	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	for (int iobj = 0, nHits; iobj < pSelection->GetCount(); iobj++)
		if (pSelection->GetObject(iobj)->GetType() == OBJTYPE_ENTITY || pSelection->GetObject(iobj)->GetType() == OBJTYPE_BRUSH)
		{
			CBaseObject* pObj = pSelection->GetObject(iobj);
			IStatObj* pTrgObj, * pCutObj = 0;
			IMaterial* pSrcMtl = pObj->GetMaterial() ? pObj->GetMaterial()->GetMatInfo() : 0, * pTplMtl = 0;
			if (pObj->GetType() == OBJTYPE_ENTITY)
			{
				if (pObj->CheckFlags(OBJFLAG_JOINTGEN_USED))
					((CEntityObject*)pObj)->Reload(true);
				IEntity* pEnt = ((CEntityObject*)pObj)->GetIEntity();
				tm = pEnt->GetSlotWorldTM(ENTITY_SLOT_ACTUAL);
				pTrgObj = pEnt->GetStatObj(ENTITY_SLOT_ACTUAL);
			}
			else
			{
				if (pObj->CheckFlags(OBJFLAG_JOINTGEN_USED))
				{
					IRenderNode* pBrush = ((CBrushObject*)pObj)->GetEngineNode();
					string fname = ((CBrushObject*)pObj)->GetIStatObj()->GetFilePath();
					pBrush->SetEntityStatObj(0, 0);
					pBrush->SetEntityStatObj(gEnv->p3DEngine->LoadStatObj(fname));
				}
				pTrgObj = ((CBrushObject*)pObj)->GetIStatObj();
				tm = pObj->GetWorldTM();
			}
			if (!pTrgObj || !VerifySingleProxy(pTrgObj))
				continue;
			pObj->SetFlags(OBJFLAG_JOINTGEN_USED);
			if (!(pTrgObj->GetFlags() & STATIC_OBJECT_COMPOUND))
			{
				IStatObj* pContainerObj = gEnv->p3DEngine->CreateStatObj();
				IStatObj::SSubObject& subObj = pContainerObj->AddSubObject(pTrgObj);
				subObj.name = "default_node";
				subObj.properties = pTrgObj->GetProperties();
				pContainerObj->SetMaterial(pTrgObj->GetMaterial());
				pTrgObj = pContainerObj;
			}
			pObj->GetBoundBox(epq.box);
			epq.box.Expand(Vec3(0.1f));
			gEnv->pEntitySystem->QueryProximity(epq);
			float V = 0;
			if (!pSrcMtl)
				pSrcMtl = pTrgObj->GetMaterial();
			CMaterial* pCSrcMtl = GetIEditorImpl()->GetMaterialManager()->FromIMaterial(pSrcMtl);
			PathUtil::Split(pTrgObj->GetFilePath(), path, fname);
			if (pSrcMtl && !(pSrcMtl->GetFlags() & MTL_FLAG_MULTI_SUBMTL))
				gEnv->pLog->LogToConsole("Warning: object %s doesn't use a multi-sub-material (%s)", fname.c_str(), pSrcMtl->GetName());
			int seed = -1;
			for (int i = 0; i < epq.nCount; i++)
			{
				epq.pEntities[i]->GetScriptTable()->GetValue("Properties", props);
				int seed1 = -1;
				props->GetValue("AlignmentSeed", seed1);
				seed = max(seed, seed1);
			}
			if (seed >= 0)
			{
				gEnv->pLog->LogToConsole("Using random seed %d", seed);
				gEnv->pSystem->GetRandomGenerator().Seed(seed);
			}

			int islot;
			for (islot = pTrgObj->GetSubObjectCount() - 1; islot >= 0 && pTrgObj->GetSubObject(islot)->nType != STATIC_SUB_OBJECT_MESH; islot--)
				;
			pTrgObj->SetSubObjectCount(islot + 1);

			for (; islot >= 0; islot--)
				if (pTrgObj->GetSubObject(islot)->nType == STATIC_SUB_OBJECT_MESH)
				{
					IStatObj::SSubObject* pSubObj0 = pTrgObj->GetSubObject(islot);
					string tplObj, str, effect;
					float tplScale = 1, scale, minSize = 0, timeout = -1, timeoutRand = 0, timeoutInvis = -1, timeoutMaxSize = 0;
					int bHierarchical = 0, align = -1;
					Vec3 tplOffs(ZERO), offs;
					Matrix33 tplR;
					if (!pSubObj0->pStatObj->GetPhysGeom())
						continue;
					V += pSubObj0->pStatObj->GetPhysGeom()->V * cube(pSubObj0->tm.GetColumn0().len());

					gwd[1].offset = tm * pSubObj0->tm.GetTranslation();
					gwd[1].R = Matrix33(tm) * Matrix33(pSubObj0->tm);
					gwd[1].R *= 1 / (gwd[1].scale = gwd[1].R.GetColumn0().len());
					for (int i = nHits = 0; i < epq.nCount; i++)
					{
						epq.pEntities[i]->GetScriptTable()->GetValue("Properties", props);
						if (!props->GetValue("object_BreakTemplate", str) || str.length() <= tplObj.length())
							continue;
						gwd[0].offset = epq.pEntities[i]->GetWorldPos();
						props->GetValue("Radius", gwd[0].scale);
						props->GetValue("Scale", scale = 1);
						props->GetValue("Offset", offs);
						if (pSph->Intersect(pSubObj0->pStatObj->GetPhysGeom()->pGeom, gwd, gwd + 1, &ip, pcont))
						{
							tplObj = str;
							tplScale = scale;
							tplOffs = offs;
							nHits++;
							props->GetValue("Timeout", timeout);
							props->GetValue("TimeoutVariance", timeoutRand);
							props->GetValue("TimeoutMaxVol", timeoutMaxSize);
							props->GetValue("TimeoutInvis", timeoutInvis);
							props->GetValue("file_Material", str);
							props->GetValue("bHierarchical", bHierarchical);
							props->GetValue("MinSize", minSize);
							if (IMaterial* pMtl = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(str))
								pTplMtl = pMtl;
							if (props->GetValue("ParticleEffect", str) && str.length())
								effect = str;
							props->GetValue("ei_Alignment", align);
							tplR = Matrix33(epq.pEntities[i]->GetRotation());
						}
					}
					gEnv->pLog->LogToConsole("Node %s: %d JointGens detected", pSubObj0->name.c_str(), nHits);

					if (tplObj.length())
						if (IStatObj* pObjTpl = gEnv->p3DEngine->LoadStatObj(tplObj, 0, 0, false))
							if (VerifySingleProxy(pObjTpl))
							{
								IStatObj* pCutObj = pObjTpl->Clone(true, true, true), * pSrcObj = pSubObj0->pStatObj;
								AABB boxTpl = pObjTpl->GetAABB(), boxSrc = pSrcObj->GetAABB();
								Vec3 szSrc = boxSrc.GetSize(), szTpl = boxTpl.GetSize();
								int imaxTpl = idxmax3(szTpl), iminTpl = idxmin3(szTpl), imaxSrc = idxmax3(szSrc), iminSrc = idxmin3(szSrc) != imaxSrc ? idxmin3(szSrc) : 2 - imaxSrc;
								gwd[1].R = align == -2 ? tplR.T() * gwd[1].R : Matrix33(IDENTITY);
								AlignCutTemplate(pSrcObj, pObjTpl, align, tplOffs, tplScale, gwd[1].R, gwd[1].offset, gwd[1].scale);
								gEnv->pLog->LogToConsole("Template orientation in source: x-> %c%c, y-> %c%c, z-> %c%c",
								                         "+-"[isneg(gwd[1].R.GetRow(0) * Vec3(1.0f))], "xyz"[idxmax3(gwd[1].R.GetRow(0).abs())],
								                         "+-"[isneg(gwd[1].R.GetRow(1) * Vec3(1.0f))], "xyz"[idxmax3(gwd[1].R.GetRow(1).abs())],
								                         "+-"[isneg(gwd[1].R.GetRow(2) * Vec3(1.0f))], "xyz"[idxmax3(gwd[1].R.GetRow(2).abs())]);
								int nInside = 0, nOutside = 0, nCut = 0, nFailed = 0, nSmall = 0, nTries;
								IGeometry* pGeomSrcFlipped = PhysicalizeIdxMesh(pSrcObj, true), * pGeomSrcProxy = pSrcObj->GetPhysGeom()->pGeom,
								         * pGeomSrcProxyFlipped = GetTriMesh(pGeomSrcProxy, true, true, pSrcObj->GetPhysGeom()->surface_idx, pGeomSrcFlipped), * pPieceGeom, * pPieceGeomProxy;
								PathUtil::Split(tplObj, path, fname);
								IMaterial* pMtl = pTplMtl && !pTplMtl->IsDefault() ? pTplMtl : pObjTpl->GetMaterial();
								if (!(pMtl->GetFlags() & MTL_FLAG_MULTI_SUBMTL))
									gEnv->pLog->LogToConsole("Warning: object %s doesn't use a multi-sub-material (%s)", fname.c_str(), pMtl->GetName());
								float V0 = -pGeomSrcFlipped->GetVolume();

								int nMats = max(1, max(pMtl->GetSubMtlCount(), pSrcMtl->GetSubMtlCount())), * mtlMap = new int[nMats];
								for (int i = 0; i < nMats; mtlMap[i] = i, i++)
									;
								for (int i = 0; i < pMtl->GetSubMtlCount(); i++)
								{
									IMaterial* pPieceMtl = pMtl->GetSafeSubMtl(i);
									int islot = -1;
									for (islot = pSrcMtl->GetSubMtlCount() - 1; islot >= 0 && strcmp(pSrcMtl->GetSubMtl(islot)->GetName(), pPieceMtl->GetName()); islot--)
										;
									if (islot < 0 && pSrcMtl->GetFlags() & MTL_FLAG_MULTI_SUBMTL)
									{
										pSrcMtl->SetSubMtlCount((islot = pSrcMtl->GetSubMtlCount()) + 1);
										pSrcMtl->SetSubMtl(islot, pPieceMtl);
										if (pCSrcMtl)
										{
											pCSrcMtl->SetSubMaterialCount(islot + 1);
											pCSrcMtl->SetSubMaterial(islot, GetIEditorImpl()->GetMaterialManager()->FromIMaterial(pPieceMtl));
										}
									}
									mtlMap[i] = max(0, islot);
								}
								int surfTypes[MAX_SUB_MATERIALS];
								memset(surfTypes, 0, sizeof(surfTypes));
								int nsurfTypes = pSrcMtl->FillSurfaceTypeIds(surfTypes);

								for (int i = pCutObj->GetSubObjectCount() - 1; i >= 0; i--)
								{
									IStatObj::SSubObject* pSubObj = pCutObj->GetSubObject(i);
									gwd[0].offset = pSubObj->localTM.GetTranslation();
									gwd[0].scale = (gwd[0].R = Matrix33(pSubObj->localTM)).GetColumn0().len();
									gwd[0].R *= 1 / gwd[0].scale;
									if (pSubObj->nType == STATIC_SUB_OBJECT_MESH && !pSubObj->bHidden && pSubObj->pStatObj && pSubObj->pStatObj->GetPhysGeom())
									{
										RemapStatObjMatids(pSubObj->pStatObj, mtlMap);
										pSubObj->pStatObj->SetMaterial(pSrcMtl);
										if (!pGeomSrcFlipped->Intersect(pPieceGeomProxy = pSubObj->pStatObj->GetPhysGeom()->pGeom, gwd + 1, gwd, &ip, pcont))
										{
											gwd[0].offset += pSubObj->pStatObj->GetPhysGeom()->pGeom->GetCenter();
											gwd[0].scale = boxTpl.GetRadius() * 2;
											gwd[0].R.SetIdentity();
											if (!((nHits = pRay->Intersect(pGeomSrcFlipped, gwd, gwd + 1, 0, pcont)) && pcont[nHits - 1].n * aray.dir > 0))
												pCutObj->RemoveSubObject(i), nOutside++;
											else nInside++;
										}
										else
										{
											pPieceGeom = PhysicalizeIdxMesh(pSubObj->pStatObj);
											mesh_data* md = (mesh_data*)pPieceGeom->GetData();
											md->flags |= mesh_no_filter;
											if ((nTries = pPieceGeom->Subtract(pGeomSrcFlipped, gwd, gwd + 1)) && pPieceGeom->GetVolume() * cube(gwd[0].scale) > V0 * minSize * cube(gwd[1].scale))
											{
												pPieceGeom->SetForeignData(pSubObj->pStatObj, 0);
												for (bop_meshupdate* pmu = (bop_meshupdate*)pPieceGeom->GetForeignData(DATA_MESHUPDATE); pmu; pmu->relScale = -1.0f, pmu = pmu->next)
													;
												pSubObj->pStatObj = gEnv->p3DEngine->UpdateDeformableStatObj(pPieceGeom);
												phys_geometry* pgeom;
												if (pPieceGeomProxy = GetTriMesh(pSubObj->pStatObj->GetPhysGeom()->pGeom, true, false, pSubObj->pStatObj->GetPhysGeom()->surface_idx, 0))
												{
													md = (mesh_data*)pPieceGeomProxy->GetData();
													md->flags |= mesh_full_serialization;
													pPieceGeomProxy->Subtract(pGeomSrcProxyFlipped, gwd, gwd + 1);
													pPieceGeomProxy->SetForeignData(pSubObj->pStatObj, 0);
													pSubObj->pStatObj->SetPhysGeom(pgeom = pGeoman->RegisterGeometry(pPieceGeomProxy, surfTypes[0], surfTypes, nsurfTypes), PHYS_GEOM_TYPE_DEFAULT);
													pSubObj->pStatObj->SetPhysGeom(pgeom = pGeoman->RegisterGeometry(pPieceGeom, surfTypes[0], surfTypes, nsurfTypes), PHYS_GEOM_TYPE_NO_COLLIDE);
													pPieceGeomProxy->Release();
												}
												else
													pSubObj->pStatObj->SetPhysGeom(pgeom = pGeoman->RegisterGeometry(pPieceGeom, surfTypes[0], surfTypes, nsurfTypes), PHYS_GEOM_TYPE_DEFAULT);
												pSubObj->pStatObj->SetMaterial(pSrcMtl);
												pSubObj->pStatObj->Invalidate();
												nCut++;
											}
											else
												pCutObj->RemoveSubObject(i), (nTries ? nSmall : nFailed)++;
											pSubObj->pStatObj->FreeIndexedMesh();
											pPieceGeom->Release();
										}
									}
								}
								delete[] mtlMap;
								pGeomSrcFlipped->Release();
								pSrcObj->FreeIndexedMesh();
								pGeomSrcProxyFlipped->Release();
								gEnv->pLog->LogToConsole("Applied template %s to node %s: %d fully inside parts, %d outside parts, %d cut parts (but %d of those too small), %d failed cuts; TOTAL: %d",
								                         fname.c_str(), pSubObj0->name.c_str(), nInside, nOutside, nCut + nSmall, nSmall, nFailed, nInside + nCut);
								nCutTot += nCut;

								if (pCutObj->GetSubObjectCount() > 0)
								{
									if (!(pTrgObj->GetFlags() & STATIC_OBJECT_CLONE))
									{
										pTrgObj = pTrgObj->Clone(false, true, false);
										pSubObj0 = pTrgObj->GetSubObject(islot);
									}
									int iparent = -1;
									float kmass = 0;
									char strmass[32];
									if (const char* pmass = strstr(pSubObj0->properties, "mass"))
										kmass = atof(pmass + 5) / (pSubObj0->pStatObj->GetPhysGeom()->V * cube(pSubObj0->tm.GetColumn0().len()));
									if (bHierarchical)
									{
										const char* strgrp = "mass=0\ngroup";
										pSubObj0->properties = strgrp;
										iparent = islot;
										pSubObj0->pStatObj->SetProperties(strgrp);
										pSubObj0->nParent = -1;
									}
									Matrix33 Rtpl2Src = gwd[1].R.T() / gwd[1].scale;
									Matrix34 tmTpl2Src = Matrix34(Rtpl2Src, Rtpl2Src * -gwd[1].offset);
									for (int j = 0; j < pCutObj->GetSubObjectCount(); j++)
										if (pCutObj->GetSubObject(j)->nType == STATIC_SUB_OBJECT_MESH)
										{
											IStatObj::SSubObject& slot0 = *pCutObj->GetSubObject(j), & slot = pTrgObj->AddSubObject(slot0.pStatObj);
											pSubObj0 = pTrgObj->GetSubObject(islot);
											slot.nParent = iparent;
											slot.bIdentityMatrix = 0;
											slot.tm = pSubObj0->tm * (slot.localTM = tmTpl2Src * slot0.localTM);
											slot.name = slot0.name + string("_0000");
											cry_sprintf((char *const)slot.name.end()-4, 5, "%04d", pTrgObj->GetSubObjectCount());
											slot.properties = slot0.properties;
											if (kmass && !strstr(slot0.properties, "mass"))
											{
												cry_sprintf(strmass, sizeof(strmass), "\nmass=%.3f", slot0.pStatObj->GetPhysGeom()->V * cube(slot.tm.GetColumn0().len()) * kmass);
												slot.properties += strmass;
											}
											if (timeoutMaxSize && sqr(slot0.pStatObj->GetPhysGeom()->V) * cube(slot.tm.GetColumn0().len2()) < sqr(timeoutMaxSize))
											{
												if (timeout >= 0.0f)
												{
													cry_sprintf(strmass, "\ntimeout=%.1f", max(0.0f, timeout + cry_random(-timeoutRand, timeoutRand)));
													slot.properties += strmass;
												}
												if (timeoutInvis >= 0.0f)
												{
													cry_sprintf(strmass, "\ninvis=%.1f", timeoutInvis);
													slot.properties += strmass;
												}
											}
											if (effect.length())
												(slot.properties += "\neffect=") += effect;
											slot0.pStatObj->SetGeoName(slot.name);
											slot0.pStatObj->SetProperties(slot.properties.c_str());
										}
									if (iparent < 0)
										pTrgObj->RemoveSubObject(islot);
								}
								pCutObj->Release();
							}
				}

			pTrgObj->FreeIndexedMesh();
			AABB bbox(AABB::RESET);
			for (int i = pTrgObj->GetSubObjectCount() - 1; i >= 0; i--)
				if (pTrgObj->GetSubObject(i)->pStatObj)
				{
					IStatObj::SSubObject* pSubObj = pTrgObj->GetSubObject(i);
					pSubObj->pStatObj->SetFlags(pSubObj->pStatObj->GetFlags() | STATIC_OBJECT_MULTIPLE_PARENTS);
					pSubObj->pStatObj->GetIndexedMesh(true);
					bbox.Add(AABB::CreateTransformedAABB(pSubObj->tm, pSubObj->pStatObj->GetAABB()));
				}
			pTrgObj->SetBBoxMin(bbox.min);
			pTrgObj->SetBBoxMax(bbox.max);

			IPhysicalEntity* pent;
			if (pObj->GetType() == OBJTYPE_ENTITY)
			{
				IEntity* pEntity = ((CEntityObject*)pObj)->GetIEntity();
				if (!pEntity->GetScriptTable() || !pEntity->GetScriptTable()->GetValue("Properties", props))
				{
					gEnv->pLog->LogToConsole("ERROR: entity doesn't have properties table; resetting the object");
					pTrgObj->Release();
					return;
				}
				props->GetValue("Physics", props);
				float mass = 0, density = 800;
				props->GetValue("Mass", mass);
				props->GetValue("Density", density);
				if (mass <= 0)
					mass = V * cube(tm.GetColumn0().len()) * density;
				pEntity->SetStatObj(0, ENTITY_SLOT_ACTUAL, true, mass);
				pEntity->SetStatObj(pTrgObj, ENTITY_SLOT_ACTUAL, true, mass);
				pent = pEntity->GetPhysics();
			}
			else
			{
				IRenderNode* pBrush = ((CBrushObject*)pObj)->GetEngineNode();
				pBrush->SetEntityStatObj(0, 0);
				pBrush->SetEntityStatObj(pTrgObj);
				pent = pBrush->GetPhysics();
			}
			gEnv->pLog->LogToConsole("Total node count: %d", pTrgObj->GetSubObjectCount());

			int nMaxNodes = 0;
			for (hidemaskLoc mask = hidemask1 << 1; !(!mask || !(mask - 1)) && nMaxNodes < 4096; mask = hidemask1 << ++nMaxNodes)
				;
			if (pTrgObj->GetSubObjectCount() > nMaxNodes)
			{
				gEnv->pLog->LogToConsole("ERROR: maximum node count (%d) exceeded; resetting the object", nMaxNodes);
				pTrgObj->Release();
				return;
			}

			for (int i = nHits = 0; i < epq.nCount; i++)
			{
				epq.pEntities[i]->GetScriptTable()->GetValue("Properties", props);
				props->GetValue("Radius", gwd[0].scale);
				gwd[0].offset = epq.pEntities[i]->GetWorldPos();
				pe_status_pos sp;
				for (sp.ipart = 0; pent->GetStatus(&sp); sp.ipart++)
				{
					gwd[1].R = Matrix33(sp.q);
					gwd[1].offset = sp.pos;
					gwd[1].scale = sp.scale;
					geom_contact* pcont;
					if (pTrgObj->GetSubObject(sp.partid) && !strstr(pTrgObj->GetSubObject(sp.partid)->properties, "group") && pSph->Intersect(sp.pGeom, gwd, gwd + 1, 0, pcont))
					{
						psj.partid[0] = sp.partid;
						psj.pt = (pcont->pt - pObj->GetWorldPos()) * pObj->GetRotation();
						psj.n = pcont->n * sp.q;
						props->GetValue("Impulse", psj.maxTorqueBend);
						pent->SetParams(&psj, 1);
						nHits++;
						break;
					}
				}
			}

			if (nHits)
			{
				gEnv->pLog->LogToConsole("Final object: %d JointGens detected", nHits);
				psj.idx = -2;
				pent->SetParams(&psj, 1);
				MARK_UNUSED psj.idx;

				new(&psj)pe_params_structural_joint;
				int nJoints = 0, nGroundJoints = 0;
				for (psj.idx = 0; pent->GetParams(&psj); psj.idx++)
				{
					nJoints += 1 - iszero(psj.id - joint_impulse);
					nGroundJoints += isneg(psj.partid[0] | psj.partid[1]);
				}
				gEnv->pLog->LogToConsole("Generated %d joints, of those %d are ground joints", nJoints, nGroundJoints - (psj.idx - nJoints));

				static char* g_txtBuf = 0;
				static int g_txtBufSize = 0;
				int sz = pent->GetStateSnapshotTxt(0, 0, -1);
				if (sz > g_txtBufSize)
				{
					delete[] g_txtBuf;
					g_txtBuf = new char[g_txtBufSize = sz];
				}
				pent->GetStateSnapshotTxt(g_txtBuf, sz, -1);
				IStatObj::SSubObject& slot = pTrgObj->AddSubObject(pTrgObj);
				slot.pStatObj = 0;
				pTrgObj->Release();
				slot.nType = STATIC_SUB_OBJECT_DUMMY;
				slot.name = "$compiled_joints";
				slot.properties = g_txtBuf;

				string filename;
				if (CFileUtil::SelectSaveFile("CGF Files (*.cgf)|*.cgf", CRY_GEOMETRY_FILE_EXT, PathUtil::Make(PathUtil::GetGameFolder(), "objects").c_str(), filename))
					if (!(filename = PathUtil::AbsolutePathToCryPakPath(filename.GetString())).IsEmpty())
					{
						pTrgObj->SetMaterial(pSrcMtl);
						pTrgObj->SaveToCGF(filename);
						if (nCutTot && pCSrcMtl)
							pCSrcMtl->Save();
					}
			}
			else
				gEnv->pLog->LogToConsole("Error: no JointGens detected on the broken object");
		}
}

//////////////////////////////////////////////////////////////////////////
void CObjectPhysicsManager::Update()
{
	if (m_bSimulatingObjects)
		UpdateSimulatingObjects();

}

//////////////////////////////////////////////////////////////////////////
void CObjectPhysicsManager::SimulateSelectedObjectsPositions()
{
	const CSelectionGroup* pSel = GetIEditorImpl()->GetObjectManager()->GetSelection();
	if (pSel->IsEmpty())
		return;

	if (GetIEditorImpl()->GetGameEngine()->GetSimulationMode())
		return;

	m_pProgress = new CWaitProgress("Simulating Objects");

	GetIEditorImpl()->GetGameEngine()->SetSimulationMode(true, true);

	m_simObjects.clear();
	for (int i = 0; i < pSel->GetCount(); i++)
	{
		CBaseObject* pObject = pSel->GetObject(i);
		IPhysicalEntity* pPhysEntity = pObject->GetCollisionEntity();
		if (!pPhysEntity)
			continue;

		pe_action_awake aa;
		aa.bAwake = 1;
		pPhysEntity->Action(&aa);

		m_simObjects.push_back(pSel->GetObject(i));
	}
	m_wasSimObjects = m_simObjects.size();

	m_fStartObjectSimulationTime = GetISystem()->GetITimer()->GetAsyncCurTime();
	m_bSimulatingObjects = true;
}

//////////////////////////////////////////////////////////////////////////
void CObjectPhysicsManager::UpdateSimulatingObjects()
{
	{
		CUndo undo("Simulate");
		for (int i = 0; i < m_simObjects.size(); )
		{
			CBaseObject* pObject = m_simObjects[i];
			IPhysicalEntity* pPhysEntity = pObject->GetCollisionEntity();
			if (pPhysEntity)
			{
				pe_status_awake awake_status;
				if (!pPhysEntity->GetStatus(&awake_status))
				{
					// When object went sleeping, record his position.
					pObject->OnEvent(EVENT_PHYSICS_GETSTATE);

					m_simObjects.erase(m_simObjects.begin() + i);
					continue;
				}
			}
			i++;
		}
	}

	float curTime = GetISystem()->GetITimer()->GetAsyncCurTime();
	float runningTime = (curTime - m_fStartObjectSimulationTime);

	m_pProgress->Step(100 * runningTime / MAX_OBJECTS_PHYS_SIMULATION_TIME);

	if (m_simObjects.empty() || (runningTime > MAX_OBJECTS_PHYS_SIMULATION_TIME))
	{
		m_fStartObjectSimulationTime = 0;
		m_bSimulatingObjects = false;
		SAFE_DELETE(m_pProgress);
		GetIEditorImpl()->GetGameEngine()->SetSimulationMode(false, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectPhysicsManager::PrepareForExport()
{
	// Clear the collision class set, ready for objects to register
	// their collision classes
	m_collisionClasses.clear();
	m_collisionClassExportId = 0;
	// First collision-class IS always the default one
	RegisterCollisionClass(SCollisionClass(0, 0));
}

//////////////////////////////////////////////////////////////////////////
bool operator==(const SCollisionClass& lhs, const SCollisionClass& rhs)
{
	return lhs.type == rhs.type && lhs.ignore == rhs.ignore;
}

//////////////////////////////////////////////////////////////////////////
int CObjectPhysicsManager::RegisterCollisionClass(const SCollisionClass& collclass)
{
	TCollisionClassVector::iterator it = std::find(m_collisionClasses.begin(), m_collisionClasses.end(), collclass);
	if (it == m_collisionClasses.end())
	{
		m_collisionClasses.push_back(collclass);
		return m_collisionClasses.size() - 1;
	}
	return it - m_collisionClasses.begin();
}

//////////////////////////////////////////////////////////////////////////
int CObjectPhysicsManager::GetCollisionClassId(const SCollisionClass& collclass)
{
	TCollisionClassVector::iterator it = std::find(m_collisionClasses.begin(), m_collisionClasses.end(), collclass);
	if (it == m_collisionClasses.end())
		return 0;
	return it - m_collisionClasses.begin();
}

//////////////////////////////////////////////////////////////////////////
void CObjectPhysicsManager::SerializeCollisionClasses(CXmlArchive& xmlAr)
{
	if (!xmlAr.bLoading)
	{
		// Storing
		CLogFile::WriteLine("Storing Collision Classes ...");

		XmlNodeRef root = xmlAr.root->newChild("CollisionClasses");
		int count = m_collisionClasses.size();
		for (int i = 0; i < count; i++)
		{
			SCollisionClass& cc = m_collisionClasses[i];
			XmlNodeRef xmlCC = root->newChild("CollisionClass");
			xmlCC->setAttr("type", cc.type);
			xmlCC->setAttr("ignore", cc.ignore);
		}
	}
}

