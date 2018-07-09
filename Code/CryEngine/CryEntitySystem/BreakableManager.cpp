// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <CryAnimation/ICryAnimation.h>
#include "BreakableManager.h"
#include "BreakablePlane.h"
#include "Entity.h"
#include "EntitySystem.h"
#include "EntitySlot.h"
#include <CryMath/GeomQuery.h>
#include <CryGame/IGameFramework.h>

#include <Cry3DEngine/I3DEngine.h>
#include <CryParticleSystem/ParticleParams.h>
#include <CryAction/IMaterialEffects.h>
#include "RopeProxy.h"

//////////////////////////////////////////////////////////////////////////
float ExtractFloatKeyFromString(const char* key, const char* props)
{
	if (const char* ptr = strstr(props, key))
	{
		ptr += strlen(key);
		if (*ptr++ == '=')
			return (float)atof(ptr);
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
string ExtractStringKeyFromString(const char* key, const string& props)
{
	const char* ptr = strstr(props, key);
	if (ptr)
	{
		ptr += strlen(key);
		while (*ptr && *ptr != '\n' && !isalnum((unsigned char)*ptr))
			++ptr;
		const char* start = ptr;
		while (*ptr && *ptr != '\n')
			++ptr;
		const char* end = ptr;
		return string(start, end);
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
void AssociateStatObjWithPhysics(IStatObj* pStatObj, IPhysicalEntity* pent, int partid)
{
	pe_params_part pp;
	pp.partid = partid;
	if (pStatObj && !pStatObj->GetPhysGeom() && pent && pent->GetParams(&pp) && pp.pPhysGeom && pp.pPhysGeom->pGeom)
	{
		gEnv->pPhysicalWorld->GetGeomManager()->AddRefGeometry(pp.pPhysGeom);
		pStatObj->SetPhysGeom(pp.pPhysGeom);
	}
}

//////////////////////////////////////////////////////////////////////////
// Get the physical surface type used on the render mesh.
// Stat object can use multiple surface types, so this one will select 1st that matches.
ISurfaceType* GetSurfaceType(IRenderMesh* pMesh, IMaterial* pMtl)
{
	if (!pMesh)
		return NULL;

	if (!pMtl)
		return 0;

	TRenderChunkArray& meshChunks = pMesh->GetChunks();

	int numChunks = meshChunks.size();
	for (int nChunk = 0; nChunk < numChunks; nChunk++)
	{
		if ((meshChunks[nChunk].m_nMatFlags & MTL_FLAG_NODRAW))
			continue;

		IMaterial* pSubMtl = pMtl->GetSafeSubMtl(meshChunks[nChunk].m_nMatID);
		if (pSubMtl)
		{
			ISurfaceType* pSrfType = pSubMtl->GetSurfaceType();
			if (pSrfType && pSrfType->GetId() != 0) // If not default surface type
				return pSubMtl->GetSurfaceType();
		}
	}
	return NULL;
}

ISurfaceType* GetSurfaceType(phys_geometry* pGeom)
{
	if (pGeom && pGeom->surface_idx < pGeom->nMats)
	{
		ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
		int nMat = pGeom->pMatMapping[pGeom->surface_idx];
		ISurfaceType* pSrfType = pSurfaceTypeManager->GetSurfaceType(nMat);
		if (pSrfType && pSrfType->GetId() != 0) // If not default surface type
			return pSrfType;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
// Get the physical surface type used on the static object.
// Stat object can use multiple surface types, so this one will select 1st that matches.
ISurfaceType* GetSurfaceType(IMeshObj* pMeshObj)
{
	if (!pMeshObj)
		return NULL;

	ISurfaceType* pSurface = GetSurfaceType(pMeshObj->GetRenderMesh(), pMeshObj->GetMaterial());

	if (!pSurface)
		pSurface = GetSurfaceType(pMeshObj->GetPhysGeom());

	return pSurface;
}

ISurfaceType* GetSurfaceType(IPhysicalEntity* pPhysEnt)
{
	if (pPhysEnt)
	{
		pe_params_part part;
		part.ipart = 0;
		if (pPhysEnt->GetParams(&part))
			return GetSurfaceType(part.pPhysGeom);
	}
	return NULL;
}

ISurfaceType* GetSurfaceType(GeomRef const& geo)
{
	ISurfaceType* pSurface;
	(pSurface = GetSurfaceType(geo.m_pMeshObj))
	|| (pSurface = GetSurfaceType(geo.m_pPhysEnt))
	|| (pSurface = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetSurfaceType(0));
	return pSurface;
}

void CBreakableManager::AddBreakEventListener(IBreakEventListener* pListener)
{
	assert(m_pBreakEventListener == NULL);
	m_pBreakEventListener = pListener;
}

void CBreakableManager::RemoveBreakEventListener(IBreakEventListener* pListener)
{
	assert(m_pBreakEventListener == pListener);
	m_pBreakEventListener = NULL;
}

void CBreakableManager::EntityDrawSlot(CEntity* pEntity, int32 slot, int32 flags)
{
	if (m_pBreakEventListener)
	{
		m_pBreakEventListener->OnEntityDrawSlot(pEntity, slot, flags);
	}
}

IParticleEffect* GetSurfaceEffect(ISurfaceType* pSurface, const char* sType, SpawnParams& params)
{
	// Extract property group from surface's script.
	if (!pSurface)
		return NULL;

	ISurfaceType::SBreakageParticles* pBreakParticles = pSurface->GetBreakageParticles(sType);
	if (!pBreakParticles)
	{
		return NULL;
	}

	const char* sEffect = pBreakParticles->particle_effect.c_str();

	if (sEffect && sEffect[0])
	{
		if (IParticleEffect* pEffect = gEnv->pParticleManager->FindEffect(sEffect, string("Surface Effect ") + pSurface->GetName()))
		{
			// Get effect spawn params, override from script.
			params.fSizeScale = pBreakParticles->scale;
			params.fCountScale = pBreakParticles->count_scale;
			params.bCountPerUnit = pBreakParticles->count_per_unit != 0;

			return pEffect;
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
ISurfaceType* CBreakableManager::GetFirstSurfaceType(IStatObj* pStatObj)
{
	return GetSurfaceType(pStatObj);
}

//////////////////////////////////////////////////////////////////////////
ISurfaceType* CBreakableManager::GetFirstSurfaceType(ICharacterInstance* pCharacter)
{
	return GetSurfaceType(pCharacter);
}

//////////////////////////////////////////////////////////////////////////
bool CBreakableManager::CanShatterRenderMesh(IRenderMesh* pRenderMesh, IMaterial* pMtl)
{
	// Check If all non-default surface types can shatter.

	if (!pMtl)
		return false;

	TRenderChunkArray& meshChunks = pRenderMesh->GetChunks();

	bool bCanShatter = true;
	int numChunks = meshChunks.size();
	for (int nChunk = 0; nChunk < numChunks; nChunk++)
	{
		if ((meshChunks[nChunk].m_nMatFlags & MTL_FLAG_NODRAW))
			continue;

		IMaterial* pSubMtl = pMtl->GetSafeSubMtl(meshChunks[nChunk].m_nMatID);
		if (pSubMtl)
		{
			ISurfaceType* pSrfType = pSubMtl->GetSurfaceType();
			if (pSrfType && pSrfType->GetId() != 0) // If not default surface type
			{
				if (pSrfType->GetFlags() & SURFACE_TYPE_CAN_SHATTER)
				{
					bCanShatter = true;
					continue;
				}
				return false; // If any material doesn`t support shatter, we cannot shatter it.
			}
		}
	}
	return bCanShatter;
}

//////////////////////////////////////////////////////////////////////////
bool CBreakableManager::CanShatter(IStatObj* pStatObj)
{
	if (!pStatObj)
		return false;
	IRenderMesh* pRenderMesh = pStatObj->GetRenderMesh();
	if (!pRenderMesh)
		return false;

	// Check If all non-default surface types can shatter.

	IMaterial* pMtl = pStatObj->GetMaterial();
	if (!pMtl)
		return false;

	bool bCanShatter = CanShatterRenderMesh(pRenderMesh, pMtl);

	if (!bCanShatter)
	{
		phys_geometry* pGeom = pStatObj->GetPhysGeom();
		if (pGeom)
		{
			if (pGeom && pGeom->surface_idx < pGeom->nMats)
			{
				ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
				int nMat = pGeom->pMatMapping[pGeom->surface_idx];
				ISurfaceType* pSrfType = pSurfaceTypeManager->GetSurfaceType(nMat);
				if (pSrfType && pSrfType->GetId() != 0) // If not default surface type
				{
					if (pSrfType->GetFlags() & SURFACE_TYPE_CAN_SHATTER)
					{
						bCanShatter = true;
					}
				}
			}
		}
	}
	return bCanShatter; // Can shatter.
}

//////////////////////////////////////////////////////////////////////////
bool CBreakableManager::CanShatterEntity(IEntity* pEntity, int nSlot)
{
	if (nSlot < 0)
		nSlot = 0;
	IStatObj* pStatObj = pEntity->GetStatObj(nSlot | ENTITY_SLOT_ACTUAL);
	if (pStatObj)
		return CanShatter(pStatObj);
	else
	{
		ICharacterInstance* pCharacter = pEntity->GetCharacter(0 | ENTITY_SLOT_ACTUAL);
		if (pCharacter)
		{
			IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
			{
				IRenderMesh* pRenderMesh = rIDefaultSkeleton.GetIRenderMesh();
				if (pRenderMesh)
				{
					// Only test first found mesh.
					if (CanShatterRenderMesh(pRenderMesh, pCharacter->GetIMaterial()))
						return true;
					return false;
				}
			}
		}
		return false;
	}
}

namespace
{
void GetPhysicsStatus(IPhysicalEntity* pPhysEnt, float& fMass, float& fVolume, float& fDensity, Vec3& vCM, Vec3& vLinVel, Vec3& vAngVel)
{
	fMass = 0.f;
	fVolume = 0.f;
	fDensity = 0.0f;
	vCM = Vec3(ZERO);
	vLinVel = Vec3(ZERO);
	vAngVel = Vec3(ZERO);

	pe_status_nparts statusTmp;
	int nparts = pPhysEnt->GetStatus(&statusTmp);
	for (int p = 0; p < nparts; p++)
	{
		pe_params_part pp;
		pp.ipart = p;

		if (pPhysEnt->GetParams(&pp) && pp.density > 0.0f)
		{
			fMass += pp.mass;
			fVolume += pp.mass / pp.density;
		}
	}

	if (fVolume > 0.f)
		fDensity = fMass / fVolume;

	pe_status_dynamics pd;
	if (pPhysEnt->GetStatus(&pd))
	{
		vCM = pd.centerOfMass;
		vLinVel = pd.v;
		vAngVel = pd.w;
	}
}
};

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::BreakIntoPieces(GeomRef& geoOrig, const Matrix34& mxSrcTM,
                                        IStatObj* pPiecesObj, const Matrix34& mxPiecesTM,	IStatObj* pRemovedObj,
                                        BreakageParams const& Breakage, int nMatLayers)
{
	ENTITY_PROFILER;

	struct MFXExec
	{
		MFXExec(GeomRef& geoOrig, const Matrix34& mxSrcTM, const BreakageParams& Breakage, const int& nMatLayers) :
			geoOrig(geoOrig),
			mxSrcTM(mxSrcTM),
			Breakage(Breakage),
			nMatLayers(nMatLayers),
			vLinVel(ZERO),
			fTotalMass(0.0f)
		{
		}
		const GeomRef&        geoOrig;
		const Matrix34&       mxSrcTM;
		const BreakageParams& Breakage;
		const int&            nMatLayers;
		Vec3                  vLinVel;
		float                 fTotalMass;

		~MFXExec()
		{
			// Spawn any destroy effect.
			if (Breakage.bMaterialEffects)
			{
				SpawnParams paramsDestroy;
				paramsDestroy.eAttachForm = GeomForm_Volume;
				paramsDestroy.eAttachType = GeomType_Render;

				const char* sType = SURFACE_BREAKAGE_TYPE("destroy");
				if (Breakage.type == BREAKAGE_TYPE_FREEZE_SHATTER)
				{
					paramsDestroy.eAttachForm = GeomForm_Surface;
					sType = SURFACE_BREAKAGE_TYPE("freeze_shatter");
				}
				ISurfaceType* pSurfaceType = GetSurfaceType(geoOrig);
				IParticleEffect* pEffect = GetSurfaceEffect(pSurfaceType, sType, paramsDestroy);
				if (pEffect)
				{
					IParticleEmitter* pEmitter = pEffect->Spawn(true, mxSrcTM);
					if (pEmitter)
					{
						pEmitter->SetMaterialLayers(nMatLayers);
						pEmitter->SetSpawnParams(paramsDestroy, geoOrig);
					}
				}

				if (gEnv->pMaterialEffects && pSurfaceType && fTotalMass > 0.0f) // fTotalMass check to avoid calling
				{
					SMFXBreakageParams mfxBreakageParams;
					mfxBreakageParams.SetMatrix(mxSrcTM);
					mfxBreakageParams.SetHitPos(Breakage.vHitPoint);
					mfxBreakageParams.SetHitImpulse(Breakage.vHitImpulse);
					mfxBreakageParams.SetExplosionImpulse(Breakage.fExplodeImpulse);
					mfxBreakageParams.SetVelocity(vLinVel);
					mfxBreakageParams.SetMass(fTotalMass);
					gEnv->pMaterialEffects->PlayBreakageEffect(pSurfaceType, sType, mfxBreakageParams);
				}
			}
		}
	};

	MFXExec exec(geoOrig, mxSrcTM, Breakage, nMatLayers);

	if (!pPiecesObj)
		return;

	int nSubObjs = pPiecesObj->GetSubObjectCount();
	if (pPiecesObj == geoOrig.m_pMeshObj && nSubObjs == 0)
	{
		// Do not spawn itself as pieces.
		return;
	}

	IEntityClass* pClass = g_pIEntitySystem->GetClassRegistry()->FindClass("Breakage");
	I3DEngine* p3DEngine = gEnv->p3DEngine;
	IPhysicalWorld* pPhysicalWorld = gEnv->pPhysicalWorld;
	PhysicsVars* pVars = pPhysicalWorld->GetPhysVars();

	//////////////////////////////////////////////////////////////////////////
	Vec3 vCM = mxPiecesTM.GetTranslation();
	Vec3 vLinVel(0), vAngVel(0);
	float fTotalMass = 0.0f;

	float fDefaultDensity = 0.f;
	if (geoOrig.m_pPhysEnt)
	{
		float fMass = 0.f, fVolume = 0.f;

		pe_status_nparts statusTmp;
		int nparts = geoOrig.m_pPhysEnt->GetStatus(&statusTmp);
		for (int p = 0; p < nparts; p++)
		{
			pe_params_part pp;
			pp.ipart = p;

			if (geoOrig.m_pPhysEnt->GetParams(&pp) && pp.density > 0.0f)
			{
				fMass += pp.mass;
				fVolume += pp.mass / pp.density;
			}
		}

		fTotalMass = fMass;

		if (fVolume > 0.f)
			fDefaultDensity = fMass / fVolume;

		pe_status_dynamics pd;
		if (geoOrig.m_pPhysEnt->GetStatus(&pd))
		{
			vCM = pd.centerOfMass;
			vLinVel = pd.v;
			vAngVel = pd.w;
		}
	}

	if (fDefaultDensity <= 0.f && pRemovedObj)
	{
		// Compute default density from main piece.
		float fMass, fDensity;
		pRemovedObj->GetPhysicalProperties(fMass, fDensity);
		if (fDensity > 0)
			fDefaultDensity = fDensity * 1000.f;
		else
		{
			if (fMass > 0 && geoOrig.m_pMeshObj->GetPhysGeom())
			{
				float V = geoOrig.m_pMeshObj->GetPhysGeom()->V;
				if (V != 0.f)
					fDefaultDensity = fMass / V;
			}
		}
		fTotalMass = fMass;
	}

	// set parameters to delayed Material Effect execution
	exec.fTotalMass = fTotalMass;
	exec.vLinVel = vLinVel;

	if (fDefaultDensity <= 0.f)
		fDefaultDensity = 1000.f;

	IParticleEmitter* pEmitter = 0;
	QuatTS entityQuatTS(mxPiecesTM);

	const char* sSourceGeometryProperties = "";
	if (pRemovedObj)
		sSourceGeometryProperties = pRemovedObj->GetProperties();

	//////////////////////////////////////////////////////////////////////////
	for (int i = 0; i < max(nSubObjs, 1); i++)
	{
		IStatObj* pSubObj;
		Matrix34 mxPiece = mxPiecesTM;
		const char* sProperties = "";

		if (nSubObjs > 0)
		{
			IStatObj::SSubObject* pSubObjInfo = pPiecesObj->GetSubObject(i);
			pSubObj = pSubObjInfo->pStatObj;
			if (!pSubObj || pSubObj == geoOrig.m_pMeshObj)
				continue;
			if (pSubObjInfo->nType != STATIC_SUB_OBJECT_MESH && pSubObjInfo->nType != STATIC_SUB_OBJECT_HELPER_MESH)
				continue;

			if (Breakage.bOnlyHelperPieces)
			{
				// If we only spawn pieces.
				if (strstr(sSourceGeometryProperties, pSubObjInfo->name) == 0)
					continue;
			}

			if (!strcmpi(pSubObjInfo->name, "Main") || !strcmpi(pSubObjInfo->name, "Remain"))
				continue;
			mxPiece = mxPiecesTM * pSubObjInfo->tm;
			sProperties = pSubObjInfo->properties.c_str();
		}
		else
		{
			pSubObj = pPiecesObj;
			sProperties = pSubObj->GetProperties();
		}

		phys_geometry* pPhysGeom = pSubObj->GetPhysGeom();
		if (pPhysGeom)
		{
			// Extract properties.
			float fMass, fDensity = fDefaultDensity;
			;
			pSubObj->GetPhysicalProperties(fMass, fDensity);
			if (fMass > 0)
				fDensity = 0;
			else if (fDensity < 0)
				fDensity = fDefaultDensity;

			bool bEntity = Breakage.bForceEntity || strstr(sProperties, "entity") != 0;

			// Extract bone affinity.
#ifdef FIX
			if (geoOrig.m_pChar)
			{
				string bone = ExtractStringKeyFromString("bone", sProperties);
				int boneId = geoOrig.m_pChar->GetIDefaultSkeleton().GetJointIDByName(bone.c_str());
				if (boneId >= 0)
					mxPiece = mxPiecesTM * Matrix34(geoOrig.m_pChar->GetISkeletonPose()->GetAbsJointByID(boneId));
			}
#endif

			// Extract generic count.
			int nCount = (int)ExtractFloatKeyFromString("count", sProperties);
			if (nCount <= 0)
				nCount = (int)ExtractFloatKeyFromString("generic", sProperties);
			float fSizeVar = ExtractFloatKeyFromString("sizevar", sProperties);
			string sRotAxes = ExtractStringKeyFromString("rotaxes", sProperties);
			pe_params_buoyancy pb;
			pb.kwaterDensity = ExtractFloatKeyFromString("kwater", sProperties);
			nCount = max(nCount, Breakage.nGenericCount);

			if (nCount)
			{
				if (geoOrig.m_pMeshObj)
					geoOrig.m_pMeshObj->GetExtent(GeomForm_Volume);
			}

			CRndGen seed = cry_random_next();
			DynArray<PosNorm> points(max(nCount, 1));
			if (geoOrig.m_pMeshObj)
				geoOrig.m_pMeshObj->GetRandomPoints(points, seed, GeomForm_Volume);

			for (int n = 0; n < max(nCount, 1); n++)
			{
				// Compute initial position.
				if (nCount)
				{
					// Position randomly in parent.
					Vec3 vPos = points[n].vPos;
					vPos = entityQuatTS * vPos;
					mxPiece.SetTranslation(vPos);

					// Random rotation and size.
					if (!sRotAxes.empty())
					{
						Ang3 angRot;
						angRot.x = strchr(sRotAxes, 'x') ? DEG2RAD(cry_random(-180.0f, 180.0f)) : 0.f;
						angRot.y = strchr(sRotAxes, 'y') ? DEG2RAD(cry_random(-180.0f, 180.0f)) : 0.f;
						angRot.z = strchr(sRotAxes, 'z') ? DEG2RAD(cry_random(-180.0f, 180.0f)) : 0.f;
						mxPiece = mxPiece * Matrix33::CreateRotationXYZ(angRot);
					}

					if (fSizeVar != 0.f)
					{
						float fScale = 1.f + cry_random(-fSizeVar, fSizeVar);
						if (fScale <= 0.01f)
							fScale = 0.01f;
						mxPiece = mxPiece * Matrix33::CreateScale(Vec3(fScale, fScale, fScale));
					}
				}

				IPhysicalEntity* pPhysEnt = 0;

				if (!bEntity)
				{
					if (!pEmitter)
					{
						ParticleParams params;
						params.fCount = 0;
						params.ePhysicsType = params.ePhysicsType.RigidBody;
						params.fParticleLifeTime.Set(Breakage.fParticleLifeTime * CVar::pDebrisLifetimeScale->GetFVal(), 0.25f);
						params.bRemainWhileVisible = true;
						pEmitter = gEnv->pParticleManager->CreateEmitter(mxSrcTM, params);
						if (!pEmitter)
							continue;
						pEmitter->SetMaterialLayers(nMatLayers);
					}

					// Spawn as particle.
					pe_params_pos ppos;
					QuatTS qts(mxPiece);
					ppos.pos = qts.t;
					ppos.q = qts.q;
					ppos.scale = qts.s;
					pPhysEnt = pPhysicalWorld->CreatePhysicalEntity(PE_RIGID, &ppos);
					if (pPhysEnt)
					{
						pe_status_pos spos;
						pPhysEnt->GetStatus(&spos);

						pe_geomparams pgeom;
						pgeom.mass = fMass;
						pgeom.density = fDensity;
						pgeom.flags = geom_collides | geom_floats;
						if (spos.scale > 0.f)
							pgeom.scale = ppos.scale / spos.scale;
						pPhysEnt->AddGeometry(pPhysGeom, &pgeom);

						pEmitter->EmitParticle(pSubObj, pPhysEnt);
					}
				}
				else if (pClass)
				{
					// Spawn as entity.
					SEntitySpawnParams params;
					params.nFlags = ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_CASTSHADOW | ENTITY_FLAG_SPAWNED | ENTITY_FLAG_MODIFIED_BY_PHYSICS;
					params.pClass = pClass;
					CEntity* pNewEntity = static_cast<CEntity*>(g_pIEntitySystem->SpawnEntity(params, false));
					if (!pNewEntity)
						continue;

					pNewEntity->SetStatObj(pSubObj, 0, false);
					pNewEntity->SetWorldTM(mxPiece);
					g_pIEntitySystem->InitEntity(pNewEntity, params);

					SEntityPhysicalizeParams physparams;
					physparams.type = PE_RIGID;
					physparams.mass = fMass;
					physparams.density = fDensity;
					physparams.nSlot = 0;
					pNewEntity->Physicalize(physparams);

					pPhysEnt = pNewEntity->GetPhysics();
					if (pPhysEnt)
					{
						pe_action_awake aa;
						aa.bAwake = 1;
						pPhysEnt->Action(&aa);
					}

					SpawnParams paramsBreak;
					paramsBreak.bCountPerUnit = true;
					paramsBreak.eAttachForm = GeomForm_Edges;
					paramsBreak.eAttachType = GeomType_Physics;

					if (Breakage.type != BREAKAGE_TYPE_FREEZE_SHATTER)
					{
						// Find breakage effect.
						if (Breakage.bMaterialEffects)
						{
							ISurfaceType* pSurfaceType = GetSurfaceType(pSubObj);
							if (pSurfaceType)
							{
								IParticleEffect* pBreakEffect = GetSurfaceEffect(pSurfaceType, SURFACE_BREAKAGE_TYPE("breakage"), paramsBreak);
								if (pBreakEffect)
									pNewEntity->LoadParticleEmitter(-1, pBreakEffect, &paramsBreak);

								if (gEnv->pMaterialEffects)
								{
									SMFXBreakageParams mfxBreakageParams;
									mfxBreakageParams.SetMatrix(mxPiece);
									mfxBreakageParams.SetMass(fTotalMass); // total mass of object
									mfxBreakageParams.SetHitImpulse(Breakage.vHitImpulse);
									mfxBreakageParams.SetExplosionImpulse(Breakage.fExplodeImpulse);
									mfxBreakageParams.SetVelocity(vLinVel);
									gEnv->pMaterialEffects->PlayBreakageEffect(pSurfaceType, SURFACE_BREAKAGE_TYPE("breakage"), mfxBreakageParams);
								}
							}
						}
					}
				}

				if (pPhysEnt)
				{
					pe_params_flags pf;
					pf.flagsOR = pef_log_collisions;
					pPhysEnt->SetParams(&pf);
					pe_simulation_params sp;
					sp.maxLoggedCollisions = 1;
					pPhysEnt->SetParams(&sp);
					if (fMass <= pVars->massLimitDebris)
					{
						pe_params_part pp;
						if (!is_unused(pVars->flagsColliderDebris))
							pp.flagsColliderAND = pp.flagsColliderOR = pVars->flagsColliderDebris;
						pp.flagsAND = pVars->flagsANDDebris;
						pPhysEnt->SetParams(&pp);
					}
					pe_params_foreign_data pfd;
					pfd.iForeignFlagsOR = PFF_UNIMPORTANT;
					pPhysEnt->SetParams(&pfd);

					// Propagate parent velocity.
					Vec3 vPartCM = mxPiece.GetTranslation();
					pe_status_dynamics pd;
					if (pPhysEnt->GetStatus(&pd))
					{
						vPartCM = pd.centerOfMass;
						if (pd.mass <= 0)
							vLinVel.zero(), vAngVel.zero();
					}

					pe_action_set_velocity sv;
					sv.v = vLinVel;
					sv.w = vAngVel;
					sv.v += vAngVel.Cross(vPartCM - vCM);
					pPhysEnt->Action(&sv);

					// Apply impulses, randomising point on each piece to provide variation & rotation.
					float fHalfRadius = pSubObj->GetRadius() * 0.5f;
					Vec3 vObj = vPartCM + Vec3(cry_random(-fHalfRadius, fHalfRadius), cry_random(-fHalfRadius, fHalfRadius), cry_random(-fHalfRadius, fHalfRadius));
					float fHitRadius = pPiecesObj->GetRadius();

					pe_action_impulse imp;

					if (Breakage.fExplodeImpulse != 0.f)
					{
						// Radial explosion impulse, largest at perimeter.
						imp.point = vCM;
						imp.impulse = vObj - imp.point;
						imp.impulse *= Breakage.fExplodeImpulse / fHitRadius;
						pPhysEnt->Action(&imp);
					}
					if (!Breakage.vHitImpulse.IsZero())
					{
						// Impulse scales down by distance from hit point.
						imp.point = Breakage.vHitPoint;
						float fDist = (vObj - imp.point).GetLength();
						if (fDist <= fHitRadius)
						{
							imp.point = vObj;
							imp.impulse = Breakage.vHitImpulse * (1.f - fDist / fHitRadius);
							pPhysEnt->Action(&imp);
						}
					}

					pPhysEnt->SetParams(&pb);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::BreakIntoPieces(IEntity* pEntity, int nOrigSlot, int nPiecesSlot, BreakageParams const& Breakage)
{
	IEntityRender* pIEntityRender = pEntity->GetRenderInterface();

	{
		IRenderNode* pRenderNode = pIEntityRender->GetRenderNode();
		if (pRenderNode)
		{
			gEnv->p3DEngine->DeleteEntityDecals(pRenderNode);

			Matrix34 mx1 = pEntity->GetSlotWorldTM(nOrigSlot);
			Matrix34 mx2 = pEntity->GetSlotWorldTM(nPiecesSlot);

			IStatObj* pPiecesObj = pEntity->GetStatObj(nPiecesSlot | ENTITY_SLOT_ACTUAL);

			if (pPiecesObj && pPiecesObj->GetParentObject())
				pPiecesObj = pPiecesObj->GetParentObject();

			GeomRef geoOrig;
			geoOrig.Set(pEntity, nOrigSlot);

			BreakIntoPieces(geoOrig, mx1, pPiecesObj, mx2, nullptr, Breakage, pRenderNode->GetMaterialLayers());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::CreateSurfaceEffect(IStatObj* pStatObj, const Matrix34& tm, const char* sType)
{
	GeomRef geo;
	geo.m_pMeshObj = pStatObj;

	SpawnParams params;

	ISurfaceType* pSurfaceType = GetSurfaceType(geo);
	if (pSurfaceType)
	{
		IParticleEffect* pEffect = GetSurfaceEffect(pSurfaceType, sType, params);
		if (pEffect)
		{
			pEffect->Spawn(true, tm);
		}
		if (gEnv->pMaterialEffects)
		{
			float fMass = 0.0f;
			float fDensity = 0.0f;
			if (pStatObj->GetPhysicalProperties(fMass, fDensity))
			{
				if (fMass <= 0.0f)
				{
					if (phys_geometry* pPhysGeom = pStatObj->GetPhysGeom())
					{
						fMass = fDensity * pPhysGeom->V;
					}
				}
			}
			if (fMass < 0.0f)
				fMass = 0.0f;

			SMFXBreakageParams mfxBreakageParams;
			mfxBreakageParams.SetMatrix(tm);
			mfxBreakageParams.SetMass(fMass);
			gEnv->pMaterialEffects->PlayBreakageEffect(pSurfaceType, sType, mfxBreakageParams);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::AttachSurfaceEffect(IEntity* pEntity, int nSlot, const char* sType, SpawnParams const& paramsIn)
{
	SpawnParams params = paramsIn;

	GeomRef geo;
	geo.Set(pEntity, nSlot);

	if (IParticleEffect* pEffect = GetSurfaceEffect(GetSurfaceType(geo), sType, params))
	{
		pEntity->LoadParticleEmitter(-1, pEffect, &params);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::CreateObjectAsParticles(IStatObj* pStatObj, IPhysicalEntity* pPhysEnt, IBreakableManager::SCreateParams& createParams)
{
	if (!pPhysEnt)
		return;

	ParticleParams params;
	params.fCount = 0;
	params.fParticleLifeTime.Set(15.f, 0.25f);
	params.bRemainWhileVisible = true;
	params.fSize = createParams.fScale;
	params.ePhysicsType = params.ePhysicsType.RigidBody;
	params.nMaxCollisionEvents = 1;

	Matrix34 tm = createParams.worldTM * createParams.slotTM;
	tm.OrthonormalizeFast();
	IParticleEmitter* pEmitter = gEnv->pParticleManager->CreateEmitter(tm, params);
	if (!pEmitter)
		return;

	pEmitter->SetMaterialLayers(createParams.nMatLayers);

	pe_action_reset_part_mtx arpm;
	pPhysEnt->Action(&arpm);
	pStatObj->AddRef();

	pEmitter->EmitParticle(pStatObj, pPhysEnt);

	pe_params_part pp;
	pp.idmatBreakable = -1;
	pPhysEnt->SetParams(&pp);

	// enable sounds generation for the new parts
	pe_params_flags pf;
	pf.flagsOR = pef_log_collisions;
	pPhysEnt->SetParams(&pf);
	pe_simulation_params sp;
	sp.maxLoggedCollisions = 1;
	pPhysEnt->SetParams(&sp);
	// By default such entities are marked as unimportant.
	pe_params_foreign_data pfd;
	pfd.iForeignFlagsOR = PFF_UNIMPORTANT;
	pPhysEnt->SetParams(&pfd);
}

//////////////////////////////////////////////////////////////////////////
IEntity* CBreakableManager::CreateObjectAsEntity(IStatObj* pStatObj, IPhysicalEntity* pPhysEnt, IPhysicalEntity* pSrcPhysEnt, IBreakableManager::SCreateParams& createParams, bool bCreateSubstProxy)
{
	// Create new Rigid body entity.
	IEntityClass* pClass = NULL;

	bool bDefault = false;
	//
	if (!createParams.pSrcStaticRenderNode)
	{
		BreakLogAlways("BREAK: Using 'Breakage' class");
		pClass = g_pIEntitySystem->GetClassRegistry()->FindClass("Breakage");
	}
	else
	{
		if (createParams.overrideEntityClass)
		{
			pClass = createParams.overrideEntityClass;
		}
		else
		{
			BreakLogAlways("BREAK: Using 'Breakage' class with SrcStaticRenderNode");
			bDefault = true;
			pClass = g_pIEntitySystem->GetClassRegistry()->FindClass("Breakage");
		}
	}
	if (!pClass)
		return 0;

	if (pPhysEnt)
	{
		CreateObjectCommon(pStatObj, pPhysEnt, createParams);
	}

	CEntityPhysics* pPhysProxy = nullptr;
	CEntityRender* pIEntityRender = nullptr;

	SEntitySpawnParams params;
	bool bNewEntitySpawned = false;

	CEntity* pEntity = static_cast<CEntity*>(pPhysEnt ? static_cast<CEntity*>(pPhysEnt->GetForeignData(PHYS_FOREIGN_ID_ENTITY)) : nullptr);
	if (!pEntity || !(pEntity->GetFlags() & ENTITY_FLAG_SPAWNED))
	{
		params.nFlags = ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_CASTSHADOW |
		                ENTITY_FLAG_SPAWNED | createParams.nEntityFlagsAdd;
		params.pClass = pClass;
		params.vScale = Vec3(createParams.fScale, createParams.fScale, createParams.fScale);
		params.sName = createParams.pName;

		//We don't want this auto-initialised as we want to init it when the statobj is set,
		//	allowing entity events to access it
		pEntity = static_cast<CEntity*>(g_pIEntitySystem->SpawnEntity(params, false));
		if (!pEntity)
			return 0;

		if ((createParams.nEntitySlotFlagsAdd & ENTITY_SLOT_BREAK_AS_ENTITY_MP) == 0)
		{
			// Inform that a new holding entity was created
			gEnv->pGameFramework->OnBreakageSpawnedEntity(pEntity, pPhysEnt, pSrcPhysEnt);
		}

		BreakLogAlways("BREAK: Creating entity, ptr: 0x%p   ID: 0x%08X", pEntity, pEntity->GetId());

		bNewEntitySpawned = true;

		pIEntityRender = pEntity->GetEntityRender();

		if (pPhysEnt)
		{
			pPhysProxy = pEntity->GetPhysicalProxy();
			pPhysProxy->AttachToPhysicalEntity(pPhysEnt);
		}

		// Copy custom material from src render proxy to this render proxy.
		//if (createParams.pCustomMtl)
		//	pEntity->SetMaterial( createParams.pCustomMtl );
		//if (createParams.nMatLayers)
		//pIEntityRender->SetMaterialLayers(createParams.nMatLayers);
		//pIEntityRender->SetRndFlags(createParams.nRenderNodeFlags);
	}
	else
	{
		BreakLogAlways("BREAK: Entity Already exists, ptr: 0x%p   ID: 0x%08X", pEntity, pEntity->GetId());
		pPhysProxy = pEntity->GetPhysicalProxy();
		pIEntityRender = pEntity->GetEntityRender();
	}

	if (pPhysEnt)
	{
		pe_params_foreign_data pfd;
		pfd.iForeignFlagsOR = PFF_UNIMPORTANT;
		pPhysEnt->SetParams(&pfd);
	}

	//pCreateEvent->partidNew
	pIEntityRender->SetSlotGeometry(createParams.nSlotIndex, pStatObj);
	if (createParams.pCustomMtl && createParams.pCustomMtl != pStatObj->GetMaterial())
		pEntity->SetSlotMaterial(createParams.nSlotIndex, createParams.pCustomMtl);

	if (!((Matrix33)createParams.slotTM).IsIdentity() || !createParams.slotTM.GetTranslation().IsZero())
	{
		pEntity->SetSlotLocalTM(createParams.nSlotIndex, createParams.slotTM);
	}

	if (!pPhysEnt)
	{
		pEntity->SetWorldTM(createParams.worldTM);
	}

	if (createParams.nEntitySlotFlagsAdd)
		pEntity->SetSlotFlags(createParams.nSlotIndex, pEntity->GetSlotFlags(createParams.nSlotIndex) | createParams.nEntitySlotFlagsAdd);

	//pIEntityRender->InvalidateBounds(true,true);

	/*
	   // Attach breakage effect.
	   SpawnParams paramsBreak;
	   paramsBreak.eAttachType = GeomType_Physics;
	   paramsBreak.eAttachForm = GeomForm_Edges;
	   IParticleEffect* pBreakEffect = GetSurfaceEffect( GetSurfaceType(pStatObj), SURFACE_BREAKAGE_TYPE("breakage"), paramsBreak );
	   if (pBreakEffect)
	   {
	   paramsBreak.nAttachSlot = createParams.nSlotIndex;
	   pEntity->LoadParticleEmitter( -1, pBreakEffect, &paramsBreak );
	   }
	 */

	if (createParams.pSrcStaticRenderNode && bCreateSubstProxy)/* && gEnv->IsEditor())
	                                                              {
	                                                              //////////////////////////////////////////////////////////////////////////
	                                                              // In editor allow restoring of the broken objects.
	                                                              //////////////////////////////////////////////////////////////////////////
	                                                            */
	{
		IEntitySubstitutionComponent* pSubstProxy = crycomponent_cast<IEntitySubstitutionComponent*>(pEntity->CreateProxy(ENTITY_PROXY_SUBSTITUTION));
		pSubstProxy->SetSubstitute(createParams.pSrcStaticRenderNode);
	}
	//

	if (bNewEntitySpawned)
	{
		g_pIEntitySystem->InitEntity(pEntity, params);
	}

	if (bDefault)
	{
		// pavlo: similar code in ActionGame.cpp does almost the same but only when
		// entity was created - so here is a case when entity is creating first
		IBreakableManager::SBrokenObjRec rec;
		rec.idEnt = pEntity->GetId();
		if (pStatObj->GetCloneSourceObject())
			pStatObj = pStatObj->GetCloneSourceObject();
		rec.pStatObjOrg = pStatObj;
		rec.pSrcRenderNode = createParams.pSrcStaticRenderNode;
		if (rec.pStatObjOrg)
		{
			m_brokenObjs.push_back(rec);
			BreakLogAlways("PR: Added entry %d to m_brokenObjs, pStatObjOrig: 0x%p    SOURCE pRenderNode: 0x%p    entityId: 0x%8X   entityPtr: 0x%p", m_brokenObjs.size() - 1, rec.pStatObjOrg, rec.pSrcRenderNode, rec.idEnt, pEntity);
		}
	}
	//
	return pEntity;
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::ReplayRemoveSubPartsEvent(const EventPhysRemoveEntityParts* pRemoveEvent)
{
	HandlePhysicsRemoveSubPartsEvent(pRemoveEvent);
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::CreateObjectCommon(IStatObj* pStatObj, IPhysicalEntity* pPhysEnt, IBreakableManager::SCreateParams& createParams)
{
	if (createParams.pSrcStaticRenderNode)
	{
		IRenderNode* pRenderNode = createParams.pSrcStaticRenderNode;
		// Make sure the original render node is not rendered anymore.
		gEnv->p3DEngine->DeleteEntityDecals(pRenderNode);
		pRenderNode->PhysicalizeFoliage(false, 0, -1);
		gEnv->p3DEngine->UnRegisterEntityAsJob(pRenderNode);
		pRenderNode->SetPhysics(0); // since the new CEntity hijacked the entity already
	}

	// enable sounds generation for the new parts
	pe_params_flags pf;
	pf.flagsOR = pef_log_collisions;
	pPhysEnt->SetParams(&pf);
	pe_simulation_params sp;
	sp.maxLoggedCollisions = 1;
	pPhysEnt->SetParams(&sp);
}

//////////////////////////////////////////////////////////////////////////
bool CBreakableManager::CheckForPieces(IStatObj* pSrcStatObj, IStatObj::SSubObject* pSubObj, const Matrix34& worldTM, int nMatLayers,
                                       IPhysicalEntity* pPhysEnt)
{
	const char* sProperties = pSubObj->pStatObj->GetProperties();

	bool bMustShatter = (nMatLayers & MTL_LAYER_FROZEN);
	if (bMustShatter)
	{
		// Spawn frozen shatter effect.
		Matrix34 objWorldTM = worldTM * pSubObj->tm;
		GeomRef geoOrig;
		geoOrig.m_pMeshObj = pSubObj->pStatObj;
		geoOrig.m_pPhysEnt = pPhysEnt;
		BreakageParams Breakage;
		Breakage.bMaterialEffects = true;
		Breakage.type = BREAKAGE_TYPE_FREEZE_SHATTER;
		// Only spawn freeze shatter material effect!
		BreakIntoPieces(geoOrig, objWorldTM, NULL, Matrix34(), pSubObj->pStatObj, Breakage, nMatLayers);
	}

	bool bHavePieces = (strstr(sProperties, "pieces=") != 0);
	// if new part is created for the frozen layer or if we have predefined pieces, break object into them.
	if (bHavePieces)
	{
		//////////////////////////////////////////////////////////////////////////
		// Spawn pieces of the object instead.
		GeomRef geoOrig;
		geoOrig.m_pMeshObj = pSubObj->pStatObj;
		geoOrig.m_pPhysEnt = pPhysEnt;

		IStatObj* pPiecesObj = pSrcStatObj;
		if (pPiecesObj->GetCloneSourceObject())
			pPiecesObj = pPiecesObj->GetCloneSourceObject();

		BreakageParams Breakage;
		Breakage.bOnlyHelperPieces = true;
		Breakage.bMaterialEffects = true;
		Breakage.bForceEntity = true;
		Breakage.fExplodeImpulse = 0;
		Breakage.fParticleLifeTime = 30.f;
		Breakage.nGenericCount = 0;
		Breakage.vHitImpulse = Vec3(ZERO); // pCreateEvent->breakImpulse; maybe use real impulse here?
		Breakage.vHitPoint = worldTM.GetTranslation();

		if (nMatLayers & MTL_LAYER_FROZEN)
		{
			// We trying to shatter frozen object.
			Breakage.bMaterialEffects = false; // Shatter effect was already spawned.
		}

		Matrix34 objWorldTM = worldTM * pSubObj->tm;

		BreakIntoPieces(geoOrig, objWorldTM, pPiecesObj, worldTM, pSubObj->pStatObj, Breakage, nMatLayers);

		// New physical entity must be deleted.
		if (pPhysEnt)
		{
			if (!pPhysEnt->GetForeignData(pPhysEnt->GetiForeignData()))
			{
				gEnv->pPhysicalWorld->DestroyPhysicalEntity(pPhysEnt);
			}
			else
			{
				pe_action_remove_all_parts arap;
				pPhysEnt->Action(&arap);
			}
		}

		return true;
		//////////////////////////////////////////////////////////////////////////
	}
	return false;
}

IStatObj::SSubObject* CBreakableManager::CheckSubObjBreak(IStatObj* pStatObj, IStatObj::SSubObject* pSubObj, const EventPhysCreateEntityPart* epcep)
{
	if (pSubObj->nBreakerJoints)
	{
		IStatObj* pAdam = pStatObj;
		while (pAdam->GetCloneSourceObject())
			pAdam = pAdam->GetCloneSourceObject();
		pe_params_structural_joint psj;
		int nDestroyedJoints = pSubObj->nBreakerJoints;
		for (psj.idx = 0; epcep->pEntNew->GetParams(&psj); psj.idx++)
		{
			IStatObj::SSubObject* pJoint;
			if ((psj.partid[0] == epcep->partidNew || psj.partid[1] == epcep->partidNew) &&
			    (pJoint = pAdam->GetSubObject(psj.id)) && strstr(pJoint->properties, "breaker"))
				--nDestroyedJoints;
		}
		const char* ptr;
		if (nDestroyedJoints > 0 && (ptr = strstr(pSubObj->properties, "pieces=")))
			return pAdam->FindSubObject(ptr + 7);
	}
	return 0;
}

void CBreakableManager::GetBrokenObjectIndicesForCloning(int32* pPartRemovalIndices, int32& iNumPartRemovalIndices,
                                                         int32* pOutIndiciesForCloning, int32& iNumEntitiesForCloning,
                                                         const EventPhysRemoveEntityParts* BreakEvents)
{
	int iNumIndices = iNumPartRemovalIndices;
	int iLocalNumEntitiesToClone = 0;

	//Get pointer to part removal event array from break replicator
	IBreakableManager* pBreakableMgr = g_pIEntitySystem->GetBreakableManager();

	int iNumBrokenObjects = 0;
	const IBreakableManager::SBrokenObjRec* pPartBrokenObjects = pBreakableMgr->GetPartBrokenObjects(iNumBrokenObjects);

	//Remove duplicates
	BreakLogAlways("PR:   Getting indicies into the BrokenObject list for cloning, %d present", iNumIndices);

	for (int i = 0; i < iNumIndices; i++)
	{
		//BreakLogAlways("PR:    iter: %d, BreakEvent Index: %d", i, pPartRemovalIndices[i]);
		//Don't use pSearchPhysEnt. Ever. There's a good chance it doesn't even exist any more
		IPhysicalEntity* pSearchPhysEnt = BreakEvents[pPartRemovalIndices[i]].pEntity;

		for (int j = i + 1; j < iNumIndices; j++)
		{
			if (BreakEvents[pPartRemovalIndices[j]].pEntity == pSearchPhysEnt)
			{
				BreakLogAlways("PR:    found duplicate IPhysicalEntity pointer: 0x%p at index %d, removing", pSearchPhysEnt, j);
				pPartRemovalIndices[j] = pPartRemovalIndices[iNumIndices - 1];
				iNumIndices--;
				j--;
			}
		}
	}

	iNumPartRemovalIndices = iNumIndices;

	//CryLogAlways("PR:   All render nodes from break events during kill cam:");
	//  for(int k=iNumIndices-1; k>=0; k--)
	//  {
	//    CryLogAlways("PR:     Idx: %3d > RenderNode: 0x%p", k, BreakEvents[pPartRemovalIndices[k]].pForeignData);
	//  }

	//CryLogAlways("PR: Carrying out clones");

	for (int k = iNumIndices - 1; k >= 0; k--)
	{
		//This is messy. Get the render node, and then use it to look up the entity generated by the event. Ergh.
		IRenderNode* pRenderNode = NULL;

		EntityId originalBrokenId = 0;

		if (BreakEvents[pPartRemovalIndices[k]].iForeignData == PHYS_FOREIGN_ID_ENTITY)
		{
			CEntity* pEnt = static_cast<CEntity*>(BreakEvents[pPartRemovalIndices[k]].pForeignData);
			originalBrokenId = pEnt->GetId();

			BreakLogAlways("PR:   Looking for object with EntityId: 0x%08X", originalBrokenId);

			for (int j = iNumBrokenObjects - 1; j >= 0; j--)
			{
				const IBreakableManager::SBrokenObjRec& brokenObjRec = pPartBrokenObjects[j];

				if (brokenObjRec.idEnt == originalBrokenId)
				{
					pOutIndiciesForCloning[iLocalNumEntitiesToClone] = j;
					iLocalNumEntitiesToClone++;
					break;
				}
			}
		}
		else
		{
			pRenderNode = static_cast<IRenderNode*>(BreakEvents[pPartRemovalIndices[k]].pForeignData);

			BreakLogAlways("PR:   Looking for object with IRenderNode: 0x%p", pRenderNode);

			for (int j = iNumBrokenObjects - 1; j >= 0; j--)
			{
				const IBreakableManager::SBrokenObjRec& brokenObjRec = pPartBrokenObjects[j];

				if (brokenObjRec.pSrcRenderNode == pRenderNode)
				{
					BreakLogAlways("PR:   FOUND object with IRenderNode: 0x%p", pRenderNode);
					pOutIndiciesForCloning[iLocalNumEntitiesToClone] = j;
					iLocalNumEntitiesToClone++;
					break;
				}
			}
		}
	}

	iNumEntitiesForCloning = iLocalNumEntitiesToClone;
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::ClonePartRemovedEntitiesByIndex(int32* pBrokenObjIndicies, int32 iNumBrokenObjectIndices,
                                                        EntityId* pOutClonedEntities, int32& iNumClonedBrokenEntities,
                                                        const EntityId* pRecordingEntities, int32 iNumRecordingEntities,
                                                        SRenderNodeCloneLookup& nodeLookup)
{
	//Get pointer to part removal event array from break replicator
	IBreakableManager* pBreakableMgr = g_pIEntitySystem->GetBreakableManager();

	int iNumClonedEntitiesLocal = 0;

	int iNumBrokenObjects;
	const IBreakableManager::SBrokenObjRec* pPartBrokenObjects = pBreakableMgr->GetPartBrokenObjects(iNumBrokenObjects);

	for (int iBrokenObjectIndex = iNumBrokenObjectIndices - 1; iBrokenObjectIndex >= 0; iBrokenObjectIndex--)
	{

		//Problem: Some entities are tracked by the recording system for the kill cam and so we don't want to clone them in the
		//					breakable manager, as they will already be cloned. We iterate through the list of objects that will be cloned
		//					elsewhere, and don't clone if they are present, but fill out space in the render node clone lookup to allow
		//					the other code to patch it up for later use.

		const IBreakableManager::SBrokenObjRec& brokenObjRec = pPartBrokenObjects[pBrokenObjIndicies[iBrokenObjectIndex]];

		EntityId originalBrokenId = brokenObjRec.idEnt;
		IRenderNode* pRenderNode = brokenObjRec.pSrcRenderNode;

		bool bWillBeClonedElsewhere = false;

		for (int l = 0; l < iNumRecordingEntities; l++)
		{
			if (pRecordingEntities[l] == originalBrokenId)
			{
				bWillBeClonedElsewhere = true;
				break;
			}
		}

		CEntity* pOriginalEntity = g_pIEntitySystem->GetEntityFromID(originalBrokenId);

		if (!bWillBeClonedElsewhere)
		{
			if (pOriginalEntity)
			{
				BreakLogAlways("PR:   Found entity with ID 0x%08X, cloning...", originalBrokenId);

				IBreakableManager::SCreateParams createParams;

				//createParams.nSlotIndex = 0;
				createParams.nSlotIndex = ENTITY_SLOT_ACTUAL;
				//createParams.slotTM;
				const Matrix34& worldTM = pOriginalEntity->GetWorldTM();
				createParams.worldTM = worldTM;
				createParams.fScale = worldTM.GetColumn0().len();
				createParams.pCustomMtl = pRenderNode->GetMaterial();
				createParams.nMatLayers = pRenderNode->GetMaterialLayers();
				createParams.nEntityFlagsAdd = (ENTITY_FLAG_NEVER_NETWORK_STATIC | ENTITY_FLAG_CLIENT_ONLY);
				createParams.nRenderNodeFlags = pRenderNode->GetRndFlags();
				createParams.pSrcStaticRenderNode = pRenderNode;
				createParams.pName = pRenderNode->GetName();
				createParams.overrideEntityClass = pOriginalEntity->GetClass();

				CEntity* pClonedEntity = static_cast<CEntity*>(pBreakableMgr->CreateObjectAsEntity(brokenObjRec.pStatObjOrg, NULL, NULL, createParams));

				assert(pClonedEntity);

				pOutClonedEntities[iNumClonedEntitiesLocal] = pClonedEntity->GetId();
				iNumClonedEntitiesLocal++;

				BreakLogAlways("PR:   Entity cloned, clone ID: 0x%08X, associating with ORIGINAL RENDERNODE 0x%p and ENTITY 0x%p",
				               pClonedEntity->GetId(),
				               reinterpret_cast<IRenderNode*>(pRenderNode),
				               reinterpret_cast<IRenderNode*>(pOriginalEntity));

				//Need to add both, because we don't know if a lookup is going to be for rendernode (first event) or for
				//	an entity (subsequent events)
				nodeLookup.AddNodePair(reinterpret_cast<IRenderNode*>(pRenderNode), reinterpret_cast<IRenderNode*>(pClonedEntity));
				nodeLookup.AddNodePair(reinterpret_cast<IRenderNode*>(pOriginalEntity), reinterpret_cast<IRenderNode*>(pClonedEntity));
			}
			else
			{
				BreakLogAlways("PR:   Failed to find entity with ID 0x%8X", originalBrokenId);
			}
		}
		else
		{
			BreakLogAlways("PR:   ENTITY ID 0x%08X was already lined up to be cloned, not cloning in break manager", originalBrokenId);

			//The order of these node pair lookups are important
			nodeLookup.AddNodePair(reinterpret_cast<IRenderNode*>(pOriginalEntity), NULL);

			IEntityRender* pIEntityRender = (pOriginalEntity->GetRenderInterface());
			if (pIEntityRender && pIEntityRender->GetRenderNode())
			{
				pRenderNode = pIEntityRender->GetRenderNode();
				nodeLookup.AddNodePair(reinterpret_cast<IRenderNode*>(pRenderNode), NULL);
			}
		}
	}

	iNumClonedBrokenEntities = iNumClonedEntitiesLocal;
}

void CBreakableManager::HideBrokenObjectsByIndex(const int32* pBrokenObjectIndices, const int32 iNumBrokenObjectIndices)
{
	for (int k = iNumBrokenObjectIndices - 1; k >= 0; k--)
	{
		EntityId originalBrokenId = m_brokenObjs[pBrokenObjectIndices[k]].idEnt;

		CEntity* pEntity = g_pIEntitySystem->GetEntityFromID(originalBrokenId);

		if (pEntity)
		{
			BreakLogAlways("PR:   Hiding entity id: 0x%08X", originalBrokenId);
			pEntity->Hide(true);

			//At the moment there is no code support for hiding decals for later re-display. This means that to avoid
			//	decals being left floating in mid air, we need to remove them when the killcam starts
			IEntityRender* pIEntityRender = pEntity->GetRenderInterface();

			{
				gEnv->p3DEngine->DeleteEntityDecals(pIEntityRender->GetRenderNode());
			}
		}
		else
		{
			BreakLogAlways("PR:   FAILED TO FIND ENTITY ID: 0x%08X", originalBrokenId);
		}
	}
}

//////////////////////////////////
// UnhidePartRemovedObjectsByIndex() takes a list of indices into the Removed Part Events array, which
//	is used to obtain the original brush pointers, which are then used to unhide the objects
void CBreakableManager::UnhidePartRemovedObjectsByIndex(const int32* pPartRemovalIndices, const int32 iNumPartRemovalIndices, const EventPhysRemoveEntityParts* pBreakEvents)
{
	IBreakableManager* pBreakableMgr = g_pIEntitySystem->GetBreakableManager();
	BreakLogAlways("PR: Unhiding part removed objects");

	int iBrokenObjectCount = 0;
	const IBreakableManager::SBrokenObjRec* pPartBrokenObjects = pBreakableMgr->GetPartBrokenObjects(iBrokenObjectCount);

	for (int k = iNumPartRemovalIndices - 1; k >= 0; k--)
	{
		int iEventIndex = pPartRemovalIndices[k];

		CEntity* pEntity = nullptr;
		EntityId originalBrokenId = 0;

		if (pBreakEvents[pPartRemovalIndices[k]].iForeignData == PHYS_FOREIGN_ID_ENTITY)
		{
			pEntity = static_cast<CEntity*>(pBreakEvents[pPartRemovalIndices[k]].pForeignData);
			BreakLogAlways("PR: ENTITY TYPE: 0x%p", pEntity);
			if (pEntity)
			{
				originalBrokenId = pEntity->GetId();
			}
		}
		else
		{
			IRenderNode* pRenderNode = static_cast<IRenderNode*>(pBreakEvents[pPartRemovalIndices[k]].pForeignData);

			BreakLogAlways("PR: Looking for object with IRenderNode: 0x%p", pRenderNode);

			for (int j = iBrokenObjectCount - 1; j >= 0; j--)
			{
				const IBreakableManager::SBrokenObjRec& brokenObjRec = pPartBrokenObjects[j];

				if (brokenObjRec.pSrcRenderNode == pRenderNode)
				{
					originalBrokenId = brokenObjRec.idEnt;
					break;
				}
			}

			pEntity = g_pIEntitySystem->GetEntityFromID(originalBrokenId);
		}

		if (pEntity)
		{
			BreakLogAlways("PR:   UNHiding entity id: 0x%08X", originalBrokenId);
			pEntity->Hide(false);
		}
		else
		{
			BreakLogAlways("PR:   FAILED TO FIND ENTITY ID: 0x%08X", originalBrokenId);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::ApplySinglePartRemovalFromEventIndex(int32 iPartRemovalEventIndex, const SRenderNodeCloneLookup& renderNodeLookup, const EventPhysRemoveEntityParts* pBreakEvents)
{
	ApplyPartBreakToClonedObjectFromEvent(renderNodeLookup, pBreakEvents[iPartRemovalEventIndex]);
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::ApplyPartRemovalsUntilIndexToObjectList(int32 iFirstIndex, const SRenderNodeCloneLookup& renderNodeLookup, const EventPhysRemoveEntityParts* pBreakEvents)
{
	BreakLogAlways("<<<< Original -> Clone pBrush lookup PART BREAKS >>>>>");
	for (int q = 0; q < renderNodeLookup.iNumPairs; q++)
	{
		BreakLogAlways("  Idx: %3d  -  Original 0x%p - Replay 0x%p", q, renderNodeLookup.pOriginalNodes[q], renderNodeLookup.pClonedNodes[q]);
	}

	int i = 0;

	while (i < iFirstIndex)
	{
		ApplyPartBreakToClonedObjectFromEvent(renderNodeLookup, pBreakEvents[i]);
		i++;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::ApplyPartBreakToClonedObjectFromEvent(const SRenderNodeCloneLookup& renderNodeLookup, const EventPhysRemoveEntityParts& OriginalEvent)
{
	BreakLogAlways("PR:   Applying PartBreak to object");
	EventPhysRemoveEntityParts tempEvent = OriginalEvent;

	int iNodeIndex = -1;
	const int kNumNodes = renderNodeLookup.iNumPairs;

	for (int a = 0; a < kNumNodes; a++)
	{
		if (OriginalEvent.pForeignData == static_cast<void*>(renderNodeLookup.pOriginalNodes[a]))
		{
			iNodeIndex = a;
			break;
		}
	}

	if (iNodeIndex >= 0)
	{
		BreakLogAlways("PR:     Found cloned Entity ptr 0x%p of original %s 0x%p at index %d", renderNodeLookup.pClonedNodes[iNodeIndex], OriginalEvent.iForeignData == PHYS_FOREIGN_ID_ENTITY ? "ENTITY" : "IRENDERNODE", OriginalEvent.pForeignData, iNodeIndex);

		//Any clones will always be entities, with the ID masquerading as a pointer in the renderNodeLookup
		CEntity* pEntity = reinterpret_cast<CEntity*>(renderNodeLookup.pClonedNodes[iNodeIndex]);

		tempEvent.pForeignData = static_cast<void*>(pEntity);
		tempEvent.iForeignData = PHYS_FOREIGN_ID_ENTITY;
		tempEvent.pEntity = NULL;

		//If there are no visible sub-objects on this statobj, we need to reveal them
		SEntitySlotInfo slotInfo;

		pEntity->GetSlotInfo(0, slotInfo);

		IStatObj* pStatObj = slotInfo.pStatObj;
		int iSubObjectCount = pStatObj->GetSubObjectCount();

		bool bRevealAllSubObj = false;

		if (!(pStatObj->GetFlags() & STATIC_OBJECT_GENERATED))
		{
			bRevealAllSubObj = true;
			for (int i = iSubObjectCount - 1; i >= 1; i--)
			{
				IStatObj::SSubObject* pSubObj = pStatObj->GetSubObject(i);
				if (!pSubObj->bHidden && pSubObj->nType == STATIC_SUB_OBJECT_MESH)
				{
					bRevealAllSubObj = false;
					break;
				}
			}
		}

		if (bRevealAllSubObj)
		{
			EventPhysRevealEntityPart RevealEvent;

			RevealEvent.pForeignData = tempEvent.pForeignData;
			RevealEvent.iForeignData = PHYS_FOREIGN_ID_ENTITY;
			RevealEvent.pEntity = NULL;

			for (int i = iSubObjectCount - 1; i >= 1; i--)
			{
				IStatObj::SSubObject* pSubObj = pStatObj->GetSubObject(i);
				if (pSubObj->nType == STATIC_SUB_OBJECT_MESH)
				{
					RevealEvent.partId = i;
					HandlePhysicsRevealSubPartEvent(&RevealEvent);
				}
			}
		}

		ReplayRemoveSubPartsEvent(&tempEvent);
	}
	else
	{
		BreakLogAlways("PR:     FAILED to find cloned version of original %s 0x%p - NOT NECESSARILY ERROR, OBJECT MAY NOT HAVE BEEN BROKEN DURING CURRENT KILLCAM",
		               OriginalEvent.iForeignData == PHYS_FOREIGN_ID_ENTITY ? "ENTITY" : "IRENDERNODE", OriginalEvent.pForeignData);
	}
}

class CTimeoutKillComponent : public IEntityComponent
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CTimeoutKillComponent, "CTimeoutKillComponent", "b55e276d-1ca5-aa14-87fb-ca961e102dff"_cry_guid)

public:
	virtual void ProcessEvent(const SEntityEvent& event) final
	{
		if (event.event == ENTITY_EVENT_TIMER)
		{
			if (!GetEntity()->IsRendered())
			{
				// Remove ourself
				g_pIEntitySystem->RemoveEntity(GetEntity()->GetId());
			}
			else
			{
				// Wait some more to see if still rendered.
				GetEntity()->SetTimer(0, m_timeoutMillis);
			}
		}
	}
	virtual uint64 GetEventMask() const final { return ENTITY_EVENT_BIT(ENTITY_EVENT_TIMER) | ENTITY_EVENT_BIT(ENTITY_EVENT_RENDER_VISIBILITY_CHANGE); }
	virtual void   Initialize() final
	{
		m_pEntity->AddFlags(ENTITY_FLAG_NO_SAVE | ENTITY_FLAG_SEND_RENDER_EVENT);
	}
	virtual void Release() final                     { delete this; }

	virtual void GameSerialize(TSerialize ser) final {};

	void         SetTimeout(int timeoutMillis)
	{
		m_timeoutMillis = timeoutMillis;
		GetEntity()->SetTimer(0, m_timeoutMillis);
	}

private:
	int m_timeoutMillis = 0;
};

void SetEntityLifetime(CEntity* pEntity, const char* props, bool visible)
{
	float timeout = -1, timeoutInvis = -1;
	if (const char* ptr = strstr(props, "timeout"))
		timeout = (float)atof(ptr + 8);
	if (const char* ptr = strstr(props, "invis"))
		timeoutInvis = (float)atof(ptr + 6);

	if (timeout == 0.0f || timeoutInvis == 0.0f && !visible)
	{
		g_pIEntitySystem->RemoveEntity(pEntity->GetId());
		return;
	}
	if (timeout > 0 || timeoutInvis >= 0)
	{
		auto pTimeout = pEntity->GetOrCreateComponent<CTimeoutKillComponent>();
		pTimeout->SetTimeout(static_cast<int>(1000 * std::max(timeout, timeoutInvis)));
	}
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::HandlePhysicsCreateEntityPartEvent(const EventPhysCreateEntityPart* pCreateEvent)
{
	int iForeignData = pCreateEvent->pEntity->GetiForeignData();
	void* pForeignData = pCreateEvent->pEntity->GetForeignData(iForeignData);

	BreakLogAlways("PC: HandlePhysicsCreateEntityPartEvent() triggered, old PhysEnt: 0x%p, ForeignData Type: %d  ForeignData ptr: 0x%p", pCreateEvent->pEntity, pCreateEvent->iForeignData, pCreateEvent->pForeignData);

	CEntity* pSrcEntity = 0;
	CEntity* pNewEntity = 0;
	bool bNewEntity = false;
	bool bAlreadyDeformed = false;
	IStatObj* pSrcStatObj = 0;
	IStatObj* pNewStatObj = 0;
	IRenderNode* pSrcRenderNode = 0;
	IStatObj::SSubObject* pSubObj;
	IStatObj* pCutShape = 0;
	IBreakableManager::SCreateParams createParams;
	int partidSrc = pCreateEvent->partidSrc;

	if (pCreateEvent->pEntity->GetType() == PE_ROPE && iForeignData == PHYS_FOREIGN_ID_ROPE && pForeignData)
	{
		IRopeRenderNode* pRope = (IRopeRenderNode*)pForeignData;
		pSrcEntity = static_cast<CEntity*>(pRope->GetOwnerEntity());
		SEntitySpawnParams params;
		params.pClass = g_pIEntitySystem->GetClassRegistry()->FindClass("RopeEntity");
		params.nFlags = ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_CASTSHADOW | ENTITY_FLAG_SPAWNED;
		params.sName = "rope_piece";
		pNewEntity = static_cast<CEntity*>(g_pIEntitySystem->SpawnEntity(params, false));
		if (!pNewEntity)
			return;

		pNewEntity->SetWorldTM(pSrcEntity->GetWorldTM());
		IRopeRenderNode* pRopeNew = ((IEntityRopeComponent*)pNewEntity->CreateProxy(ENTITY_PROXY_ROPE))->GetRopeRenderNode();
		g_pIEntitySystem->InitEntity(pNewEntity, params);
		pNewEntity->SetMaterial(pSrcEntity->GetMaterial());

		IRopeRenderNode::SRopeParams rparams = pRope->GetParams();
		pe_params_rope pr;
		pCreateEvent->pEntNew->GetParams(&pr);
		rparams.nNumSegments = FtoI(rparams.nNumSegments * pr.nSegments / (float)rparams.nPhysSegments);
		rparams.fTextureTileV = rparams.fTextureTileV * pr.nSegments / rparams.nPhysSegments;
		rparams.nPhysSegments = pr.nSegments;
		pRopeNew->SetParams(rparams);

		pRopeNew->SetMatrix(pSrcEntity->GetWorldTM());
		pRopeNew->SetPhysics(pCreateEvent->pEntNew);

		return;
	}

	PhysicsVars* pVars = gEnv->pPhysicalWorld->GetPhysVars();
	if (pVars->lastTimeStep + 1 - pVars->bLogStructureChanges == 0)
	{
		// undo if physics not running and not loading (bLogStructureChanges is 0 during loading)
		if (pCreateEvent->pEntNew)
			gEnv->pPhysicalWorld->DestroyPhysicalEntity(pCreateEvent->pEntNew);
		return;
	}

	if (iForeignData == PHYS_FOREIGN_ID_ENTITY)
	{
		//CryLogAlways( "* CreateEvent Entity" );

		pSrcEntity = static_cast<CEntity*>(pForeignData);
		pNewEntity = static_cast<CEntity*>(pCreateEvent->pEntNew->GetForeignData(PHYS_FOREIGN_ID_ENTITY));

		int nLevels, nBits;
		EntityPhysicsUtils::ParsePartId(pCreateEvent->partidSrc, nLevels, nBits);
		if (nLevels == 2)
			if (ICharacterInstance* pCharInstance = pSrcEntity->GetCharacter(EntityPhysicsUtils::GetSlotIdx(pCreateEvent->partidSrc)))
			{
				if (ISkeletonPose* pSkelPose = pCharInstance->GetISkeletonPose())
					pSkelPose->SetPhysEntOnJoint(EntityPhysicsUtils::GetSlotIdx(pCreateEvent->partidSrc, 1), pCreateEvent->pEntNew);
				return;
			}

		if (pCreateEvent->partidSrc >= EntityPhysicsUtils::PARTID_LINKED)
			pSrcEntity = static_cast<CEntity*>(pSrcEntity->UnmapAttachedChild(partidSrc));

		//////////////////////////////////////////////////////////////////////////
		// Handle Breakage of Entity.
		//////////////////////////////////////////////////////////////////////////
		if (pCreateEvent->iReason != EventPhysCreateEntityPart::ReasonMeshSplit)
		{
			createParams.nEntityFlagsAdd = ENTITY_FLAG_MODIFIED_BY_PHYSICS;
			pSrcEntity->AddFlags(createParams.nEntityFlagsAdd);
		}
		else
		{
			createParams.nEntityFlagsAdd = pSrcEntity->GetFlags() & ENTITY_FLAG_MODIFIED_BY_PHYSICS;
		}
		//createParams.nSlotIndex = pCreateEvent->partidNew;
		createParams.worldTM = pSrcEntity->GetWorldTM();
		int i = partidSrc;
		if (pSrcEntity->GetEntityRender()->GetCompoundObj())
			i = ENTITY_SLOT_ACTUAL;
		pSrcStatObj = pSrcEntity->GetEntityRender()->GetStatObj(i);
		createParams.slotTM = pSrcEntity->GetSlotLocalTM(i, false);
		/*if (pCreateEvent->partidSrc>=PARTID_CGA)
		   {
		   if (pSrcEntity->GetEntityRender())
		    pSrcEntity->GetEntityRender()->SetSlotGeometry(pCreateEvent->partidSrc,0);
		   }*/

		pSrcRenderNode = pSrcEntity->GetRenderNode(i);
		//createParams.nMatLayers = pSrcRenderProxy->GetMaterialLayersMask();
		createParams.nRenderNodeFlags = pSrcRenderNode ? pSrcRenderNode->GetRndFlags() : (ERF_CASTSHADOWMAPS | ERF_MOVES_EVERY_FRAME);
		createParams.pCustomMtl = pSrcEntity->GetMaterial();
		createParams.fScale = pSrcEntity->GetScale().x;
		createParams.pName = pSrcEntity->GetName();

		if (pNewEntity)
			pNewStatObj = pNewEntity->GetStatObj(ENTITY_SLOT_ACTUAL);
	}
	else if (iForeignData == PHYS_FOREIGN_ID_STATIC && !pCreateEvent->bInvalid)
	{
		//CryLogAlways( "* CreateEvent Static" );

		pNewEntity = static_cast<CEntity*>(pCreateEvent->pEntNew->GetForeignData(PHYS_FOREIGN_ID_ENTITY));
		if (pNewEntity)
		{
			pNewStatObj = pNewEntity->GetStatObj(ENTITY_SLOT_ACTUAL);
		}

		//////////////////////////////////////////////////////////////////////////
		// Handle Breakage of Brush or Vegetation.
		//////////////////////////////////////////////////////////////////////////
		pSrcRenderNode = static_cast<IRenderNode*>(pForeignData);
		if (pSrcRenderNode)
		{
			createParams.pSrcStaticRenderNode = pSrcRenderNode;

			Matrix34A nodeTM;
			pSrcStatObj = pSrcRenderNode->GetEntityStatObj(0, &nodeTM);
			createParams.fScale = nodeTM.GetColumn(0).len();

			createParams.nSlotIndex = pCreateEvent->partidNew;
			createParams.worldTM = nodeTM;

			createParams.nMatLayers = pSrcRenderNode->GetMaterialLayers();
			createParams.nRenderNodeFlags = pSrcRenderNode->GetRndFlags();
			createParams.pCustomMtl = pSrcRenderNode->GetMaterial();
			createParams.pName = pSrcRenderNode->GetName();
		}
	}

	// Handle compound object.
	if (pSrcStatObj)
	{
		if (pSrcStatObj->GetFlags() & STATIC_OBJECT_COMPOUND)
		{
			if (!pSrcStatObj->GetSubObject(partidSrc))
				return;
			if (pCreateEvent->nTotParts > 1 || pNewEntity != 0 && pNewEntity == pSrcEntity)
			{
				// Multi-parts object.
				if (!pNewStatObj)
				{
					pNewStatObj = pSrcStatObj->Clone(false, false, true);
					pNewStatObj->SetFlags(pNewStatObj->GetFlags() | STATIC_OBJECT_GENERATED);
					pNewStatObj->SetSubObjectCount(0);
				}
				//pNewStatObj->SetSubObjectCount(pNewEntity && pNewEntity==pSrcEntity ? pNewStatObj->GetSubObjectCount()+pCreateEvent->nTotParts : 0);
				pNewStatObj->CopySubObject(pCreateEvent->partidNew, pSrcStatObj, partidSrc);
				assert(pCreateEvent->nTotParts > 1 || pCreateEvent->partidNew == pNewStatObj->GetSubObjectCount() - 1);
				IStatObj::SSubObject* pSubObjNew = pNewStatObj->GetSubObject(pCreateEvent->partidNew), * pSubObjBroken;
				if (pSubObjNew)
				{
					pSubObjNew->bHidden = false;
					if (pSubObjBroken = CheckSubObjBreak(pNewStatObj, pSubObjNew, pCreateEvent))
					{
						pSubObjNew->pStatObj->Release();
						(pSubObjNew->pStatObj = pSubObjBroken->pStatObj)->AddRef();
					}
				}

				//////////////////////////////////////////////////////////////////////////
				if (pCreateEvent->pMeshNew)
				{
					bAlreadyDeformed = true;
					IStatObj* pDeformedStatObj = gEnv->p3DEngine->UpdateDeformableStatObj(pCreateEvent->pMeshNew, pCreateEvent->pLastUpdate);
					AssociateStatObjWithPhysics(pDeformedStatObj, pCreateEvent->pEntNew, pCreateEvent->partidNew);
					createParams.nEntitySlotFlagsAdd = ENTITY_SLOT_BREAK_AS_ENTITY;
					if (pCreateEvent->bInvalid && pDeformedStatObj)
					{
						pDeformedStatObj->Release();
						pDeformedStatObj = 0;
					}
					pSubObj = pNewStatObj->GetSubObject(pCreateEvent->partidNew);
					if (pSubObj && (pSubObj->pStatObj = pDeformedStatObj))
						pDeformedStatObj->AddRef();
				}

				//////////////////////////////////////////////////////////////////////////
				if (pNewEntity)
					pNewStatObj = 0; // Do not apply it again on entity.
			}
			else
			{
				pSubObj = pSrcStatObj->GetSubObject(partidSrc);
				if (pSubObj && pSubObj->pStatObj != NULL && pSubObj->nType == STATIC_SUB_OBJECT_MESH)
				{
					// marcok: make sure decals get removed (might need a better place)
					if (pSrcRenderNode)
						gEnv->p3DEngine->DeleteEntityDecals(pSrcRenderNode);

					if (CheckForPieces(pSrcStatObj, pSubObj, createParams.worldTM, createParams.nMatLayers, pCreateEvent->pEntNew))
						return;

					pNewStatObj = pSubObj->pStatObj;
					if (!pSubObj->bIdentityMatrix)
						createParams.slotTM = createParams.slotTM * pSubObj->tm;
				}
			}
		}
		else
		{
			// Not sub object.
			pNewStatObj = pSrcStatObj;
			createParams.nSlotIndex = pCreateEvent->partidNew;
		}

		pe_params_buoyancy pb;
		if (!strstr(pSrcStatObj->GetProperties(), "floats") && !(pCreateEvent->pEntNew->GetParams(&pb) && pb.kwaterDensity != 1.0f))
		{
			new(&pb)pe_params_buoyancy();
			pb.kwaterDensity = 0;
			pCreateEvent->pEntNew->SetParams(&pb);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	if (pCreateEvent->pMeshNew && !bAlreadyDeformed)
	{
		if (pCreateEvent->pLastUpdate && pCreateEvent->pLastUpdate->pMesh[1] &&
		    (pCutShape = static_cast<IStatObj*>(pCreateEvent->pLastUpdate->pMesh[1]->GetForeignData())))
		{
			float cutShapeScale;
			pCutShape = pCutShape->GetLastBooleanOp(cutShapeScale);
		}

		pNewStatObj = gEnv->p3DEngine->UpdateDeformableStatObj(pCreateEvent->pMeshNew, pCreateEvent->pLastUpdate);
		if (pNewStatObj)
		{
			AssociateStatObjWithPhysics(pNewStatObj, pCreateEvent->pEntNew, pCreateEvent->partidNew);
			pNewStatObj->SetFlags(pNewStatObj->GetFlags() | STATIC_OBJECT_GENERATED);
		}
		createParams.nEntitySlotFlagsAdd = ENTITY_SLOT_BREAK_AS_ENTITY;
		if (pCreateEvent->bInvalid && pNewStatObj)
		{
			pNewStatObj->Release();
			pNewStatObj = 0;
		}
	}
	//////////////////////////////////////////////////////////////////////////

	if (pNewStatObj)
	{
		if (gEnv->bMultiplayer && strstr(pNewStatObj->GetProperties(), "entity_mp"))
		{
			createParams.nEntitySlotFlagsAdd |= ENTITY_SLOT_BREAK_AS_ENTITY_MP;
		}

		//if (gEnv->bMultiplayer || strstr(pNewStatObj->GetProperties(),"entity"))
		createParams.nEntitySlotFlagsAdd |= ENTITY_SLOT_BREAK_AS_ENTITY;

		if (!(createParams.nEntitySlotFlagsAdd & ENTITY_SLOT_BREAK_AS_ENTITY) && pCreateEvent->nTotParts == 1 &&
		    pCreateEvent->pEntity != pCreateEvent->pEntNew && !(pNewStatObj->GetFlags() & STATIC_OBJECT_GENERATED))
		{
			CreateObjectAsParticles(pNewStatObj, pCreateEvent->pEntNew, createParams);

			if (pCreateEvent->nTotParts)
			{

			}
		}
		else
		{
			CEntity* pPrevNewEntity = pNewEntity;
			if (pCreateEvent->pEntity->GetType() == PE_ARTICULATED)
			{
				Matrix34 mtx = createParams.worldTM;
				mtx.ScaleTranslation(0);
				createParams.slotTM = mtx * createParams.slotTM;
			}
			pNewEntity = static_cast<CEntity*>(CreateObjectAsEntity(pNewStatObj, pCreateEvent->pEntNew, pCreateEvent->pEntity, createParams));
			bNewEntity = pNewEntity != pPrevNewEntity;

			int lastDrawnFrame = pSrcRenderNode ? pSrcRenderNode->GetDrawFrame() : 0;
			if (pCreateEvent->nTotParts == 1)
				SetEntityLifetime(pNewEntity, pNewStatObj->GetProperties(), pSrcRenderNode && (!lastDrawnFrame || lastDrawnFrame + 10 > gEnv->pRenderer->GetFrameID()));
		}
	}

	if (bNewEntity && pNewEntity && pNewEntity->GetPhysicalProxy()->PhysicalizeFoliage(pCreateEvent->partidNew) && pSrcEntity && (iForeignData == PHYS_FOREIGN_ID_ENTITY))
		pSrcEntity->GetPhysicalProxy()->DephysicalizeFoliage(pCreateEvent->partidSrc);

	//////////////////////////////////////////////////////////////////////////
	// Object have been cut, procedural breaking...
	//////////////////////////////////////////////////////////////////////////
	if (pCutShape && !is_unused(pCreateEvent->cutPtLoc[1]) && pNewStatObj)
	{
		Matrix34 tm = Matrix34::CreateTranslationMat(createParams.worldTM.TransformPoint(pCreateEvent->cutPtLoc[1]));

		// Spawn break destroy effect.
		SpawnParams paramsDestroy;
		paramsDestroy.eAttachForm = GeomForm_Surface;
		paramsDestroy.eAttachType = GeomType_None;
		paramsDestroy.fCountScale = 1.0f;
		paramsDestroy.bCountPerUnit = false;
		const char* sType = SURFACE_BREAKAGE_TYPE("breakage");
		if (createParams.nMatLayers & MTL_LAYER_FROZEN)
		{
			sType = SURFACE_BREAKAGE_TYPE("freeze_shatter");
		}

		ISurfaceType* pSurfaceType = GetSurfaceType(pNewStatObj);
		if (pSurfaceType)
		{
			IParticleEffect* pEffect = GetSurfaceEffect(pSurfaceType, sType, paramsDestroy);
			if (pEffect)
			{
				IParticleEmitter* pEmitter = pEffect->Spawn(true, tm);
				if (pEmitter)
				{
					pEmitter->SetMaterialLayers(createParams.nMatLayers);
					//pEmitter->SetSpawnParams(paramsDestroy);
				}
			}
			if (gEnv->pMaterialEffects)
			{
				/*				if (createParams.pSrcStaticRenderNode &&
				            createParams.pSrcStaticRenderNode->GetRenderNodeType() == eERType_Vegetation)
				        {
				          int a = 0;
				        }
				 */
				float fMass = 0.0f;
				float fDensity = 0.0f;
				Vec3 vCM(ZERO);
				Vec3 vLinVel(ZERO);
				Vec3 vAngVel(ZERO);
				if (pCreateEvent->pEntNew)
				{
					float fVolume;
					GetPhysicsStatus(pCreateEvent->pEntNew, fMass, fVolume, fDensity, vCM, vLinVel, vAngVel);
				}
				if (fDensity <= 0.0f)
					pNewStatObj->GetPhysicalProperties(fMass, fDensity);
				SMFXBreakageParams mfxBreakageParams;
				mfxBreakageParams.SetMatrix(tm);
				mfxBreakageParams.SetHitImpulse(pCreateEvent->breakImpulse);
				mfxBreakageParams.SetMass(fMass);

				gEnv->pMaterialEffects->PlayBreakageEffect(pSurfaceType, sType, mfxBreakageParams);
			}
		}
	}

	if (pCreateEvent->cutRadius > 0 && pCreateEvent->cutDirLoc[1].len2() > 0 && pCutShape && (pSubObj = pCutShape->FindSubObject("splinters")))
	{
		Matrix34 tm;
		float rscale;
		if (pNewEntity)
		{
			rscale = 1.0f / pNewEntity->GetScale().x;
			tm.SetRotation33(Matrix33::CreateRotationV0V1(Vec3(0, 0, 1), pCreateEvent->cutDirLoc[1]));
			tm.ScaleColumn(Vec3(pCreateEvent->cutRadius * rscale * pSubObj->helperSize.x));
			tm.SetTranslation(pCreateEvent->cutPtLoc[1] * rscale);
			pNewEntity->SetSlotLocalTM(pNewEntity->SetStatObj(pSubObj->pStatObj, -1, false), tm);
		}
		if (pSrcEntity)
		{
			rscale = 1.0f / pSrcEntity->GetScale().x;
			tm.SetRotation33(Matrix33::CreateRotationV0V1(Vec3(0, 0, 1), pCreateEvent->cutDirLoc[0]));
			tm.ScaleColumn(Vec3(pCreateEvent->cutRadius * rscale * pSubObj->helperSize.x));
			tm.SetTranslation(pCreateEvent->cutPtLoc[0] * rscale);
			pSrcEntity->SetSlotLocalTM(pSrcEntity->SetStatObj(pSubObj->pStatObj, -1, false), tm);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::HandlePhysicsRevealSubPartEvent(const EventPhysRevealEntityPart* pRevealEvent)
{
	int iForeignData = 0;
	void* pForeignData = NULL;
	int id = pRevealEvent->partId;

	if (pRevealEvent->pEntity)
	{
		iForeignData = pRevealEvent->pEntity->GetiForeignData();
		pForeignData = pRevealEvent->pEntity->GetForeignData(iForeignData);
	}
	else
	{
		iForeignData = pRevealEvent->iForeignData;
		pForeignData = pRevealEvent->pForeignData;
	}

	CEntity* pEntity = NULL;
	IStatObj* pStatObj = NULL;
	IRenderNode* pRenderNode = NULL;
	Matrix34A nodeTM;
	bool bNewObject = false;
	hidemaskLoc nSubObjHideMask = 0;

	if (iForeignData == PHYS_FOREIGN_ID_ENTITY)
	{
		pEntity = static_cast<CEntity*>(pForeignData);
		if (pEntity && id >= EntityPhysicsUtils::PARTID_LINKED)
			pEntity = static_cast<CEntity*>(pEntity->UnmapAttachedChild(id));
		if (pEntity)
		{
			pStatObj = pEntity->GetStatObj(ENTITY_SLOT_ACTUAL);
			if (pEntity->GetEntityRender())
				if (!(nSubObjHideMask = pEntity->GetEntityRender()->GetSubObjHideMask(0)) && pStatObj)
					nSubObjHideMask = pStatObj->GetInitialHideMask();
		}
	}
	else if (iForeignData == PHYS_FOREIGN_ID_STATIC)
		if (pStatObj = (pRenderNode = static_cast<IRenderNode*>(pForeignData))->GetEntityStatObj(0, &nodeTM))
		{
			nSubObjHideMask = pStatObj->GetInitialHideMask();
			bNewObject = true;
		}

	/*if (pStatObj && pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND)
	   {
	   if (!(pStatObj->GetFlags() & STATIC_OBJECT_GENERATED))
	   {
	    // Make Unique compound static object.
	    pStatObj = pStatObj->Clone(false,false,true);
	    pStatObj->SetFlags( pStatObj->GetFlags()|STATIC_OBJECT_GENERATED );
	    bNewObject = true;
	   }
	   if (IStatObj::SSubObject *pSubObj = pStatObj->GetSubObject(id))
	   {
	    pSubObj->bHidden = false;
	    pStatObj->Invalidate(false);
	   }
	   }*/

	if (bNewObject)
		if (pEntity)
			pEntity->SetStatObj(pStatObj, ENTITY_SLOT_ACTUAL, false);
		else
		{
			IBreakableManager::SCreateParams createParams;

			createParams.pSrcStaticRenderNode = pRenderNode;
			createParams.fScale = nodeTM.GetColumn(0).len();

			createParams.nSlotIndex = 0;
			createParams.worldTM = nodeTM;

			createParams.nMatLayers = pRenderNode->GetMaterialLayers();
			createParams.nRenderNodeFlags = pRenderNode->GetRndFlags();
			createParams.pCustomMtl = pRenderNode->GetMaterial();
			if (iForeignData == PHYS_FOREIGN_ID_STATIC)
				createParams.nEntityFlagsAdd = ENTITY_FLAG_MODIFIED_BY_PHYSICS;
			createParams.pName = pRenderNode->GetName();

			pEntity = static_cast<CEntity*>(CreateObjectAsEntity(pStatObj, pRevealEvent->pEntity, pRevealEvent->pEntity, createParams, true));
		}

	if (pEntity && pEntity->GetEntityRender())
		pEntity->GetEntityRender()->SetSubObjHideMask(0, nSubObjHideMask &= ~(hidemask1 << id));
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::HandlePhysicsRemoveSubPartsEvent(const EventPhysRemoveEntityParts* pRemoveEvent)
{
	int iForeignData = 0;
	void* pForeignData = NULL;

	//Handling the pEntity being NULL in the case of killcam playback
	if (pRemoveEvent->pEntity)
	{
		iForeignData = pRemoveEvent->pEntity->GetiForeignData();
		pForeignData = pRemoveEvent->pEntity->GetForeignData(iForeignData);
	}
	else
	{
		iForeignData = pRemoveEvent->iForeignData;
		pForeignData = pRemoveEvent->pForeignData;
	}

	bool bNewObject = false;
	CEntity* pEntity = NULL;
	IStatObj* pStatObj = NULL;
	Matrix34A nodeTM;
	IRenderNode* pRenderNode = NULL;
	hidemaskLoc nSubObjHideMask = 0;

	int j = 0, idOffs = pRemoveEvent->idOffs;
	for (int i = 0; i < sizeof(pRemoveEvent->partIds) / sizeof(pRemoveEvent->partIds[0]); i++)
		j |= pRemoveEvent->partIds[i];
	if (!j)
		return;

	if (iForeignData == PHYS_FOREIGN_ID_ENTITY)
	{
		//CryLogAlways( "* RemoveEvent Entity" );
		pEntity = static_cast<CEntity*>(pForeignData);
		if (pEntity && pRemoveEvent->idOffs >= EntityPhysicsUtils::PARTID_LINKED)
			pEntity = static_cast<CEntity*>(pEntity->UnmapAttachedChild(idOffs));
		if (pEntity)
		{
			pStatObj = pEntity->GetStatObj(ENTITY_SLOT_ACTUAL);
			if (pEntity->GetSlotCount() > 1 && (!pStatObj || !(pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND)))
			{
				for (int i = 0; i < sizeof(pRemoveEvent->partIds) / sizeof(pRemoveEvent->partIds[0]); i++)
					for (j = 0; j < 32; j++)
						if (pRemoveEvent->partIds[i] & 1u << j)
							pEntity->FreeSlot(pRemoveEvent->idOffs + i * 32 + j);
				return;
			}
		}
		if (pStatObj && !(pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND))
		{
			// If entity only hosts a single geometry and nothing remains, delete entity itself.
			pStatObj = 0;
			if (pEntity->GetFlags() & ENTITY_FLAG_SPAWNED)
			{
				pEntity->SetStatObj(0, 0, false);
				pEntity->UnphysicalizeSlot(0);
			}
		}
		if (pEntity)
		{
			nodeTM = pEntity->GetWorldTM();
			pRenderNode = pEntity->GetEntityRender()->Slot(0)->GetRenderNode();
			if (pRenderNode)
			{
				nSubObjHideMask = pEntity->GetEntityRender()->GetSubObjHideMask(0);
			}
		}
	}
	else if (iForeignData == PHYS_FOREIGN_ID_STATIC)
	{
		//CryLogAlways( "* RemoveEvent Entity Static" );
		pRenderNode = static_cast<IRenderNode*>(pForeignData);
		pStatObj = pRenderNode->GetEntityStatObj(0, &nodeTM);
		bNewObject = true;
	}

	if (pStatObj && pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND)
	{
		// Delete specified sub-objects.
		for (int i = sizeof(pRemoveEvent->partIds) * 8 - 1; i >= 0; i--)
		{
			if (pRemoveEvent->partIds[i >> 5] & 1 << (i & 31))
			{
				// This sub-object must be removed.
				uint32 nSubObjIndex = i + idOffs;
				nSubObjHideMask |= (hidemask1 << nSubObjIndex);
			}
		}
		int nVisibleSubObj = 0, iVisibleSubObj = -1;
		hidemaskOneBit nBitmask = hidemask1;
		for (int i = 0, num = pStatObj->GetSubObjectCount(); i < num && nVisibleSubObj < 2; i++)
		{
			int vis = pStatObj->GetSubObject(i)->pStatObj && !(nSubObjHideMask & nBitmask);
			nVisibleSubObj += vis;
			iVisibleSubObj += i - iVisibleSubObj & - vis;
			nBitmask <<= 1;
		}

		if (pEntity)
		{
			if (!nVisibleSubObj)
			{
				pEntity->Hide(true);
			}
			else
			{
				{
					if (!pEntity->GetSubObjHideMask(0))
					{
						// No previous hide mask. then it is a new broken object.
						bNewObject = true;
					}
					pEntity->SetSubObjHideMask(0, nSubObjHideMask);
					if (m_pBreakEventListener)
					{
						m_pBreakEventListener->OnSetSubObjHideMask(pEntity, 0, nSubObjHideMask);
					}
					if (nVisibleSubObj == 1 && pEntity->GetFlags() & ENTITY_FLAG_SPAWNED)
					{
						SetEntityLifetime(pEntity, pStatObj->GetSubObject(iVisibleSubObj)->pStatObj->GetProperties(), (uint32)pEntity->GetRenderNode()->GetDrawFrame() + 10 > gEnv->nMainFrameID);
					}
				}
			}
		}
		/*
		    int nMeshes,iMesh;
		    for(int i=0,nMeshes=0;i<pStatObj->GetSubObjectCount();i++)
		    {
		      IStatObj::SSubObject *pSubObj = pStatObj->GetSubObject(i);
		      if (pSubObj && pSubObj->nType == STATIC_SUB_OBJECT_MESH)
		        nMeshes++,iMesh=i;
		    }
		    if (nMeshes==1 && pRenderNode && CheckForPieces(pStatObj,pSubObj, nodeTM,pRenderNode->GetMaterialLayers(), pRemoveEvent->pEntity))
		      pStatObj->RemoveSubObject(iMesh);
		 */
	}

	// Make an entity for the remaining piece.
	if (bNewObject)
	{
		if (pEntity)
		{
			if (!(pEntity->GetFlags() & ENTITY_FLAG_SPAWNED))
			{
				IBreakableManager::SBrokenObjRec rec;
				rec.idEnt = pEntity->GetId();
				rec.pStatObjOrg = pStatObj;

				if (pEntity->GetEntityRender())
				{
					rec.pSrcRenderNode = pEntity->GetEntityRender()->GetRenderNode();
				}
				else
				{
					rec.pSrcRenderNode = NULL;
				}

				if (rec.pStatObjOrg)
				{
					m_brokenObjs.push_back(rec);
					BreakLogAlways("PR: NO NEW ENT Added entry %d to m_brokenObjs, pStatObjOrig: 0x%p    pRenderNode: 0x%p    entityId: 0x%08X   entityPtr: 0x%p", m_brokenObjs.size() - 1, rec.pStatObjOrg, rec.pSrcRenderNode, rec.idEnt, pEntity);
				}
			}
			else
			{
				BreakLogAlways("PR: Entity 0x%08X marked as spawned, ptr 0x%p", pEntity->GetId(), pEntity);
			}
		}
		else
		{
			IBreakableManager::SCreateParams createParams;

			createParams.pSrcStaticRenderNode = pRenderNode;
			createParams.fScale = nodeTM.GetColumn(0).len();

			createParams.nSlotIndex = 0;
			createParams.worldTM = nodeTM;

			createParams.nMatLayers = pRenderNode->GetMaterialLayers();
			createParams.nRenderNodeFlags = pRenderNode->GetRndFlags();
			createParams.pCustomMtl = pRenderNode->GetMaterial();
			if (iForeignData == PHYS_FOREIGN_ID_STATIC)
				createParams.nEntityFlagsAdd = ENTITY_FLAG_MODIFIED_BY_PHYSICS;
			createParams.pName = pRenderNode->GetName();

			CEntity* pNewHoldingEntity = static_cast<CEntity*>(CreateObjectAsEntity(pStatObj, pRemoveEvent->pEntity, pRemoveEvent->pEntity, createParams, true));

			if (pNewHoldingEntity)
			{
				{
					pNewHoldingEntity->SetSubObjHideMask(0, nSubObjHideMask);
					if (m_pBreakEventListener)
					{
						m_pBreakEventListener->OnSetSubObjHideMask(pNewHoldingEntity, 0, nSubObjHideMask);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int CBreakableManager::HandlePhysics_UpdateMeshEvent(const EventPhysUpdateMesh* pUpdateEvent)
{

	CEntity* pCEntity = 0;
	Matrix34A mtx;
	int iForeignData = pUpdateEvent->pEntity->GetiForeignData();
	void* pForeignData = pUpdateEvent->pEntity->GetForeignData(iForeignData);
	IFoliage* pSrcFoliage = 0;

	if (pUpdateEvent->pEntity->GetType() == PE_ROPE && pUpdateEvent->iForeignData == PHYS_FOREIGN_ID_ROPE && pUpdateEvent->pForeignData)
	{
		IRopeRenderNode* pRope = (IRopeRenderNode*)pUpdateEvent->pForeignData;
		IRopeRenderNode::SRopeParams params = pRope->GetParams();
		pe_params_rope pr;
		pUpdateEvent->pEntity->GetParams(&pr);
		CEntity* pSrcEntity = static_cast<CEntity*>(pRope->GetOwnerEntity());
		CEntityComponentRope* pRopeProxy = static_cast<CEntityComponentRope*>(pSrcEntity->GetProxy(ENTITY_PROXY_ROPE));
		pRopeProxy->PreserveParams();
		params.nNumSegments = FtoI(pr.nSegments * params.nNumSegments / (float)params.nPhysSegments);
		params.fTextureTileV = params.fTextureTileV * pr.nSegments / params.nPhysSegments;
		params.nPhysSegments = pr.nSegments;
		pRope->SetParams(params);
		return 1;
	}

	bool bNewEntity = false;

	if (iForeignData == PHYS_FOREIGN_ID_ENTITY)
	{
		pCEntity = static_cast<CEntity*>(pForeignData);
		if (!pCEntity || !pCEntity->GetPhysicalProxy())
			return 1;
	}
	else if (iForeignData == PHYS_FOREIGN_ID_STATIC)
	{
		IBreakableManager::SCreateParams createParams;

		CBreakableManager* pBreakMgr = static_cast<CBreakableManager*>(GetIEntitySystem()->GetBreakableManager());

		IRenderNode* pRenderNode = static_cast<IRenderNode*>(pUpdateEvent->pForeignData);
		IStatObj* pStatObj = pRenderNode->GetEntityStatObj(0, &mtx);

		PhysicsVars* pVars = gEnv->pPhysicalWorld->GetPhysVars();
		if (pVars->lastTimeStep + 1 - pVars->bLogStructureChanges == 0)
		{
			// undo if physics not running and not loading (bLogStructureChanges is 0 during loading)
			pRenderNode->Physicalize(true);
			return 1;
		}

		createParams.fScale = mtx.GetColumn(0).len();
		createParams.pSrcStaticRenderNode = pRenderNode;
		createParams.worldTM = mtx;
		createParams.nMatLayers = pRenderNode->GetMaterialLayers();
		createParams.pCustomMtl = pRenderNode->GetMaterial();
		createParams.nRenderNodeFlags = pRenderNode->GetRndFlags();
		createParams.pName = pRenderNode->GetName();
		pSrcFoliage = pRenderNode->GetFoliage();
		if (pSrcFoliage)
			pSrcFoliage->AddRef();

		if (pStatObj)
		{
			pCEntity = static_cast<CEntity*>(pBreakMgr->CreateObjectAsEntity(pStatObj, pUpdateEvent->pEntity, pUpdateEvent->pEntity, createParams, true));
			bNewEntity = true;
			if (pUpdateEvent->iReason == EventPhysUpdateMesh::ReasonDeform)
				pCEntity->SetFlags(pCEntity->GetFlags() | ENTITY_FLAG_NO_SAVE);
		}
	}
	else
		return 1;

	bop_meshupdate* pmu = (bop_meshupdate*)pUpdateEvent->pMesh->GetForeignData(DATA_MESHUPDATE);
	IStatObj* pSrcStatObj = static_cast<IStatObj*>(pUpdateEvent->pMesh->GetForeignData());
	if (pmu && pSrcStatObj && GetSurfaceType(pSrcStatObj))
	{
		SpawnParams paramsDestroy;
		paramsDestroy.eAttachForm = GeomForm_Surface;
		paramsDestroy.eAttachType = GeomType_None;
		paramsDestroy.fCountScale = 1.0f;
		paramsDestroy.bCountPerUnit = false;
		IParticleEffect* pEffect = GetSurfaceEffect(GetSurfaceType(pSrcStatObj), "breakage_carve", paramsDestroy);
		if (pEffect)
		{
			Vec3 center(ZERO), normal(ZERO);
			mesh_data* md = (mesh_data*)pUpdateEvent->pMesh->GetData();
			int nTris = 0;
			for (int i = 0; i < pmu->nNewTri; i++)
				if (pmu->pNewTri[i].iop)
				{
					Vec3 vtx[3];
					for (int j = 0; j < 3; j++)
					{
						int ivtx = pmu->pNewTri[i].iVtx[j];
						if (ivtx < 0)
							ivtx = pmu->pNewVtx[-ivtx - 1].idx;
						vtx[j] = md->pVertices[ivtx];
					}
					center += vtx[0] + vtx[1] + vtx[2];
					normal += vtx[1] - vtx[0] ^ vtx[2] - vtx[0];
					nTris++;
				}
			if (nTris > 0)
				center /= (float)(nTris * 3), normal.normalize();
			if (normal.len2() > 0 && pCEntity)
				pEffect->Spawn(true, pCEntity->GetSlotWorldTM(pUpdateEvent->partid) * Matrix34(Vec3(1), Quat::CreateRotationV0V1(Vec3(0, 1, 0), normal), center));
		}
	}

	//if (pUpdateEvent->iReason==EventPhysUpdateMesh::ReasonExplosion)
	//	pCEntity->AddFlags(ENTITY_FLAG_MODIFIED_BY_PHYSICS);

	if (pCEntity)
	{
		CEntityRender* pIEntityRender = pCEntity->GetEntityRender();

		pCEntity->GetPhysicalProxy()->DephysicalizeFoliage(pUpdateEvent->partid);
		IStatObj* pDeformedStatObj = 0;

		if (pUpdateEvent->iReason != EventPhysUpdateMesh::ReasonDeform)
		{
			pDeformedStatObj = gEnv->p3DEngine->UpdateDeformableStatObj(pUpdateEvent->pMesh, pUpdateEvent->pLastUpdate, pSrcFoliage);
			AssociateStatObjWithPhysics(pDeformedStatObj, pUpdateEvent->pEntity, pUpdateEvent->partid);
		}
		else
		{
			mesh_data* md = (mesh_data*)pUpdateEvent->pMeshSkel->GetData();
			IStatObj* pStatObj = pIEntityRender->GetStatObj(pUpdateEvent->partid);
			if (pUpdateEvent->idx == 0)
				pDeformedStatObj = static_cast<IStatObj*>(pUpdateEvent->pMesh->GetForeignData());
			else if (pStatObj)
			{
				Matrix34 mtxSkelToMeshAligned = pUpdateEvent->mtxSkelToMesh;
				pDeformedStatObj = pStatObj->SkinVertices(md->pVertices, mtxSkelToMeshAligned);
				if (pUpdateEvent->pMesh->GetPrimitiveCount() > 1)
					pUpdateEvent->pMesh->SetForeignData(pDeformedStatObj, 0);
			}
		}

		bool bChar = false;
		if (ICharacterInstance* pChar = pIEntityRender->GetCharacter(0))
			if (IAttachmentManager* pAttMan = pChar->GetIAttachmentManager())
				for (int i = pAttMan->GetAttachmentCount() - 1; i >= 0 && !bChar; i--)
					if (bChar = pAttMan->GetInterfaceByIndex(i)->GetIAttachmentObject() && pAttMan->GetInterfaceByIndex(i)->GetIAttachmentObject()->GetIStatObj() == pSrcStatObj)
						((CCGFAttachment*)pAttMan->GetInterfaceByIndex(i)->GetIAttachmentObject())->pObj = pDeformedStatObj;

		if (!bChar)
			pIEntityRender->SetSlotGeometry(pUpdateEvent->partid, pDeformedStatObj);
		pCEntity->GetPhysicalProxy()->PhysicalizeFoliage(pUpdateEvent->partid);

		if (bNewEntity)
		{
			SEntityEvent entityEvent(ENTITY_EVENT_PHYS_BREAK);
			pCEntity->SendEvent(entityEvent);
		}

		if (m_pBreakEventListener && pSrcStatObj != pDeformedStatObj && pUpdateEvent->iReason != EventPhysUpdateMesh::ReasonDeform)
		{
			BreakLogAlways("TB: OnEntityChangeStatObj: EntityPtr: 0x%p   EntityID: 0x%08X, Entity is %s, OLD StatObj: 0x%p    NEW StatObj: 0x%p", pCEntity, pCEntity->GetId(), bNewEntity ? "NEW" : "OLD", pSrcStatObj, pDeformedStatObj);
			m_pBreakEventListener->OnEntityChangeStatObj(pCEntity, m_brokenObjs.size(), pUpdateEvent->partid, pSrcStatObj, pDeformedStatObj);

			IBreakableManager::SBrokenObjRec brokenObj;

			brokenObj.idEnt = pCEntity->GetId();
			brokenObj.pSrcRenderNode = pIEntityRender->GetRenderNode();
			brokenObj.pStatObjOrg = pSrcStatObj;

			m_brokenObjs.push_back(brokenObj);
		}
	}

	if (pSrcFoliage)
		pSrcFoliage->Release();

	return 1;
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::FakePhysicsEvent(EventPhys* pEvent)
{
	switch (pEvent->idval)
	{
	case EventPhysCreateEntityPart::id:
		HandlePhysicsCreateEntityPartEvent(static_cast<EventPhysCreateEntityPart*>(pEvent));
		break;
	case EventPhysRemoveEntityParts::id:
		HandlePhysicsRemoveSubPartsEvent(static_cast<EventPhysRemoveEntityParts*>(pEvent));
		break;
	case EventPhysUpdateMesh::id:
		HandlePhysics_UpdateMeshEvent(static_cast<EventPhysUpdateMesh*>(pEvent));
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::FreezeRenderNode(IRenderNode* pRenderNode, bool bEnable)
{

}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::ResetBrokenObjects()
{
	for (int i = m_brokenObjs.size() - 1; i >= 0; i--)
	{
		CEntity* pEnt = g_pIEntitySystem->GetEntityFromID(m_brokenObjs[i].idEnt);
		if (pEnt)
		{
			for (int j = pEnt->GetSlotCount() - 1; j >= 0; j--)
				pEnt->FreeSlot(j);
			pEnt->SetStatObj(m_brokenObjs[i].pStatObjOrg, ENTITY_SLOT_ACTUAL, true);
		}
	}
	stl::free_container(m_brokenObjs);
}

//////////////////////////////////////////////////////////////////////////
EProcessImpactResult CBreakableManager::ProcessPlaneImpact(const SProcessImpactIn& in, SProcessImpactOut& out)
{
	return (EProcessImpactResult)CBreakablePlane::ProcessImpact(in, out);
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::ExtractPlaneMeshIsland(const SExtractMeshIslandIn& in, SExtractMeshIslandOut& out)
{
	CBreakablePlane::ExtractMeshIsland(in, out);
}

//////////////////////////////////////////////////////////////////////////
void CBreakableManager::GetPlaneMemoryStatistics(void* pPlaneRaw, ICrySizer* pSizer)
{
	if (!pPlaneRaw)
		return;

	CBreakablePlane* pPlane = static_cast<CBreakablePlane*>(pPlaneRaw);
	pPlane->m_pGrid->GetMemoryStatistics(pSizer);
}

//////////////////////////////////////////////////////////////////////////
bool CBreakableManager::IsGeometryBreakable(IPhysicalEntity* pEntity, IStatObj* pStatObj, IMaterial* pMaterial)
{
	IIndexedMesh* pIndexedMesh = pStatObj->GetIndexedMesh(true);
	if (!pIndexedMesh)
		return true;

	int subsetCount = pIndexedMesh->GetSubSetCount();
	for (int i = 0; i < subsetCount; ++i)
	{
		// can we actually have multiple subsets with breakable materials?
		const SMeshSubset& subset = pIndexedMesh->GetSubSet(i);
		if (subset.nNumVerts == 0)
			continue;

		IMaterial* pSubMaterial = pMaterial->GetSubMtl(subset.nMatID);
		if (!pSubMaterial)
			continue;

		if (!pSubMaterial->GetSurfaceType()->GetBreakable2DParams())
			continue;

		SProcessImpactIn in;
		{
			in.pthit = Vec3(0.0f, 0.0f, 0.0f);
			in.hitvel = Vec3(0.0f, 0.0f, 0.0f);
			in.hitnorm = Vec3(0.0f, 0.0f, 1.0f);
			in.hitmass = 0.0f;
			in.hitradius = 0.0f;
			in.itriHit = 0;
			in.pStatObj = pStatObj;
			in.pStatObjAux = 0;
			in.mtx.SetIdentity();
			in.pMat = pSubMaterial->GetSurfaceType();
			in.pRenderMat = pSubMaterial;
			in.processFlags = ePlaneBreak_AutoSmash;
			if (pEntity->GetType() == PE_STATIC)
				in.processFlags |= ePlaneBreak_Static;
			in.eventSeed = 0;
			in.bLoading = false;
			in.glassAutoShatterMinArea = 0.0f;
			in.addChunkFunc = 0;
			in.bDelay = false;
			in.bVerify = true;
			in.pIslandOut = 0;
		}
		SProcessImpactOut out;

		EProcessImpactResult result = (EProcessImpactResult)CBreakablePlane::ProcessImpact(in, out);
		if (result == eProcessImpact_BadGeometry)
			return false;
	}

	int subobjectCount = pStatObj->GetSubObjectCount();
	for (int i = 0; i < subobjectCount; ++i)
	{
		const IStatObj::SSubObject* subobject = pStatObj->GetSubObject(i);
		if (subobject->pStatObj)
		{
			if (!IsGeometryBreakable(pEntity, subobject->pStatObj, pMaterial))
				return false;
		}
	}

	return true;
}
