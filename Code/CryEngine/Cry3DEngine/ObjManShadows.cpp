// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   statobjmanshadows.cpp
//  Version:     v1.00
//  Created:     2/6/2002 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Shadow casters/receivers relations
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "terrain.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "3dEngine.h"
#include <CryMath/AABBSV.h>
#include "Vegetation.h"
#include "Brush.h"
#include "LightEntity.h"
#include "ObjectsTree.h"

#pragma warning(push)
#pragma warning(disable: 4244)

bool IsAABBInsideHull(const SPlaneObject* pHullPlanes, int nPlanesNum, const AABB& aabbBox);

void CObjManager::MakeShadowCastersList(CVisArea* pArea, const AABB& aabbReceiver, int dwAllowedTypes, int32 nRenderNodeFlags, Vec3 vLightPos, SRenderLight* pLight, ShadowMapFrustum* pFr, PodArray<SPlaneObject>* pShadowHull, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	assert(pLight && vLightPos.len() > 1); // world space pos required

	pFr->castersList.Clear();
	pFr->jobExecutedCastersList.Clear();

	CVisArea* pLightArea = pLight->m_pOwner ? (CVisArea*)pLight->m_pOwner->GetEntityVisArea() : NULL;

	if (pArea)
	{
		if (pArea->IsObjectsTreeValid())
		{
			pArea->GetObjectsTree()->FillShadowCastersList(false, pLight, pFr, pShadowHull, nRenderNodeFlags, passInfo);
		}

		if (pLightArea)
		{
			// check neighbor sectors and portals if light and object are not in same area
			if (!(pLight->m_Flags & DLF_THIS_AREA_ONLY))
			{
				for (int pp = 0; pp < pArea->m_lstConnections.Count(); pp++)
				{
					CVisArea* pN = pArea->m_lstConnections[pp];
					if (pN->IsObjectsTreeValid())
					{
						pN->GetObjectsTree()->FillShadowCastersList(false, pLight, pFr, pShadowHull, nRenderNodeFlags, passInfo);
					}

					for (int p = 0; p < pN->m_lstConnections.Count(); p++)
					{
						CVisArea* pNN = pN->m_lstConnections[p];
						if (pNN != pLightArea && pNN->IsObjectsTreeValid())
						{
							pNN->GetObjectsTree()->FillShadowCastersList(false, pLight, pFr, pShadowHull, nRenderNodeFlags, passInfo);
						}
					}
				}
			}
			else if (!pLightArea->IsPortal())
			{
				// visit also portals
				for (int p = 0; p < pArea->m_lstConnections.Count(); p++)
				{
					CVisArea* pN = pArea->m_lstConnections[p];
					if (pN->IsObjectsTreeValid())
					{
						pN->GetObjectsTree()->FillShadowCastersList(false, pLight, pFr, pShadowHull, nRenderNodeFlags, passInfo);
					}
				}
			}
		}
	}
	else
	{
		PodArray<CTerrainNode*>& lstCastingNodes = m_lstTmpCastingNodes;
		lstCastingNodes.Clear();

		Get3DEngine()->m_pObjectsTree->FillShadowCastersList(false, pLight, pFr, pShadowHull, nRenderNodeFlags, passInfo);

		// check also visareas effected by sun
		CVisAreaManager* pVisAreaManager = GetVisAreaManager();
		if (pVisAreaManager)
		{
			{
				PodArray<CVisArea*>& lstAreas = pVisAreaManager->m_lstVisAreas;
				for (int i = 0; i < lstAreas.Count(); i++)
					if (lstAreas[i]->IsAffectedByOutLights() && lstAreas[i]->IsObjectsTreeValid())
					{
						bool bUnused = false;
						if (pFr->IntersectAABB(*lstAreas[i]->GetAABBox(), &bUnused))
							if (!pShadowHull || IsAABBInsideHull(pShadowHull->GetElements(), pShadowHull->Count(), *lstAreas[i]->GetAABBox()))
								lstAreas[i]->GetObjectsTree()->FillShadowCastersList(false, pLight, pFr, pShadowHull, nRenderNodeFlags, passInfo);
					}
			}
			{
				PodArray<CVisArea*>& lstAreas = pVisAreaManager->m_lstPortals;
				for (int i = 0; i < lstAreas.Count(); i++)
					if (lstAreas[i]->IsAffectedByOutLights() && lstAreas[i]->IsObjectsTreeValid())
					{
						bool bUnused = false;
						if (pFr->IntersectAABB(*lstAreas[i]->GetAABBox(), &bUnused))
							if (!pShadowHull || IsAABBInsideHull(pShadowHull->GetElements(), pShadowHull->Count(), *lstAreas[i]->GetAABBox()))
								lstAreas[i]->GetObjectsTree()->FillShadowCastersList(false, pLight, pFr, pShadowHull, nRenderNodeFlags, passInfo);
					}
			}
		}

		bool bNeedRenderTerrain = (GetTerrain() && Get3DEngine()->m_bSunShadowsFromTerrain && (pLight->m_Flags & DLF_SUN) != 0);

		if (bNeedRenderTerrain && passInfo.RenderTerrain() && Get3DEngine()->m_bShowTerrainSurface)
		{
			// find all caster sectors
			GetTerrain()->IntersectWithShadowFrustum(&pFr->castersList, pFr, passInfo);
		}
	}

	// add casters with per object shadow map for point lights
	if ((pLight->m_Flags & DLF_SUN) == 0)
	{
		for (uint i = 0; i < Get3DEngine()->m_lstPerObjectShadows.size(); ++i)
		{
			IShadowCaster* pCaster = Get3DEngine()->m_lstPerObjectShadows[i].pCaster;
			assert(pCaster);

			AABB casterBox = pCaster->GetBBoxVirtual();

			if (!IsRenderNodeTypeEnabled(pCaster->GetRenderNodeType()))
				continue;

			bool bObjCompletellyInFrustum = false;
			if (!pFr->IntersectAABB(casterBox, &bObjCompletellyInFrustum))
				continue;

			pFr->castersList.Add(pCaster);
		}
	}
}

int CObjManager::MakeStaticShadowCastersList(IRenderNode* pIgnoreNode, ShadowMapFrustum* pFrustum, const PodArray<struct SPlaneObject>* pShadowHull, int renderNodeExcludeFlags, int nMaxNodes, const SRenderingPassInfo& passInfo)
{
	int nRemainingNodes = nMaxNodes;

	int nNumTrees = Get3DEngine()->m_pObjectsTree ? 1 : 0;
	if (CVisAreaManager* pVisAreaManager = GetVisAreaManager())
		nNumTrees += pVisAreaManager->m_lstVisAreas.size() + pVisAreaManager->m_lstPortals.size();

	// objects tree first
	{
		Get3DEngine()->m_pObjectsTree->GetShadowCastersTimeSliced(pIgnoreNode, pFrustum, pShadowHull, renderNodeExcludeFlags, nRemainingNodes, 1, passInfo);

		if (nRemainingNodes <= 0)
			return nRemainingNodes;

		pFrustum->pShadowCacheData->mOctreePath[0]++;
	}

	// Vis Areas
	CVisAreaManager* pVisAreaManager = GetVisAreaManager();
	if (pVisAreaManager)
	{
		PodArray<CVisArea*>* lstAreaTypes[] =
		{
			&pVisAreaManager->m_lstVisAreas,
			&pVisAreaManager->m_lstPortals
		};

		const int nNumAreaTypes = CRY_ARRAY_COUNT(lstAreaTypes);
		for (int nAreaType = 0; nAreaType < nNumAreaTypes; ++nAreaType)
		{
			PodArray<CVisArea*>& lstAreas = *lstAreaTypes[nAreaType];

			for (int i = 0; i < lstAreas.Count(); i++)
			{
				if (lstAreas[i]->IsAffectedByOutLights() && lstAreas[i]->IsObjectsTreeValid())
				{
					if (pFrustum->aabbCasters.IsReset() || Overlap::AABB_AABB(pFrustum->aabbCasters, *lstAreas[i]->GetAABBox()))
						lstAreas[i]->GetObjectsTree()->GetShadowCastersTimeSliced(pIgnoreNode, pFrustum, pShadowHull, renderNodeExcludeFlags, nRemainingNodes, 0, passInfo);
				}

				if (nRemainingNodes <= 0)
					return nRemainingNodes;

				pFrustum->pShadowCacheData->mOctreePath[0]++;
			}
		}
	}

	// if we got here we processed every tree, so reset tree index
	pFrustum->pShadowCacheData->mOctreePath[0] = 0;
	return nRemainingNodes;
}

uint64 CObjManager::GetShadowFrustumsList(PodArray<SRenderLight*>* pAffectingLights, const AABB& aabbReceiver,
                                          float fObjDistance, uint32 nDLightMask, bool bIncludeNearFrustums,
                                          const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	assert(passInfo.RenderShadows());

	if (!pAffectingLights || !pAffectingLights->Count())
		return 0;

	const int MAX_MASKED_GSM_LODS_NUM = 4;

	// calculate frustums list id
	uint64 nCastersListId = 0;
	int32 nSunID = 0;
	uint32 nFrustums = 0;
	if (bIncludeNearFrustums)
		for (int i = 0; i < 64 && i < pAffectingLights->Count(); i++)
		{
			SRenderLight* pLight = pAffectingLights->GetAt(i);

			assert(pLight->m_Id < 64);

			bool bSun = (pLight->m_Flags & DLF_SUN) != 0;

			if ((pLight->m_Id >= 0) && (nDLightMask & (1 << pLight->m_Id)) && (pLight->m_Flags & DLF_CASTSHADOW_MAPS))
			{
				if (CLightEntity::ShadowMapInfo* pSMI = ((CLightEntity*)pLight->m_pOwner)->m_pShadowMapInfo)
				{
					const int nLodCount = Get3DEngine()->GetShadowsCascadeCount(pLight);
					for (int nLod = 0; nLod < nLodCount && nLod < MAX_MASKED_GSM_LODS_NUM && pSMI->pGSM[nLod]; nLod++)
					{
						ShadowMapFrustum* pFr = pSMI->pGSM[nLod];
						if (pFr->castersList.Count())
						{
							// take GSM Lod's containing receiver bbox inside shadow frustum
							bool bUnused = false;
							if (pFr->IntersectAABB(aabbReceiver, &bUnused))
							{
								if (bSun)
								{
									nSunID = pLight->m_Id;
									nCastersListId |= (uint64(1) << nLod);
								}
								else
									nCastersListId |= (uint64(1) << pLight->m_Id);
								nFrustums++;
							}

							// todo: if(nCull == CULL_INCLUSION) break;
						}
					}
				}
			}
		}
	assert(nSunID == 0);

	if (!nCastersListId)
		return 0;

	assert(nCastersListId);
	return nCastersListId;
}

#pragma warning(pop)
