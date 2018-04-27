// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

	pFr->ResetCasterLists();

	assert(CLightEntity::IsOnePassTraversalFrustum(pFr));
}

int CObjManager::MakeStaticShadowCastersList(IRenderNode* pIgnoreNode, ShadowMapFrustum* pFrustum, const PodArray<struct SPlaneObject>* pShadowHull, int renderNodeExcludeFlags, int nMaxNodes, const SRenderingPassInfo& passInfo)
{
	const int curLevel = 0;
	int nRemainingNodes = nMaxNodes;

	int nNumTrees = Get3DEngine()->m_pObjectsTree ? 1 : 0;
	if (CVisAreaManager* pVisAreaManager = GetVisAreaManager())
		nNumTrees += pVisAreaManager->m_lstVisAreas.size() + pVisAreaManager->m_lstPortals.size();

	// objects tree first
	if (pFrustum->pShadowCacheData->mOctreePath[curLevel] == 0)
	{
		if (!Get3DEngine()->m_pObjectsTree->GetShadowCastersTimeSliced(pIgnoreNode, pFrustum, pShadowHull, renderNodeExcludeFlags, nRemainingNodes, curLevel + 1, passInfo))
			return nRemainingNodes;

		++pFrustum->pShadowCacheData->mOctreePath[curLevel];
	}

	// Vis Areas
	if (CVisAreaManager* pVisAreaManager = GetVisAreaManager())
	{
		auto gatherCastersForVisAreas = [=, &nRemainingNodes](PodArray<CVisArea*>& visAreas)
		{
			for (int i = pFrustum->pShadowCacheData->mOctreePath[curLevel] - 1; i < visAreas.Count(); ++i, ++pFrustum->pShadowCacheData->mOctreePath[curLevel])
			{
				if (visAreas[i]->IsAffectedByOutLights() && visAreas[i]->IsObjectsTreeValid())
				{
					if (pFrustum->aabbCasters.IsReset() || Overlap::AABB_AABB(pFrustum->aabbCasters, *visAreas[i]->GetAABBox()))
					{
						if (!visAreas[i]->GetObjectsTree()->GetShadowCastersTimeSliced(pIgnoreNode, pFrustum, pShadowHull, renderNodeExcludeFlags, nRemainingNodes, curLevel + 1, passInfo))
							return false;
					}
				}
			}

			return true;
		};

		if (!gatherCastersForVisAreas(pVisAreaManager->m_lstVisAreas))
			return nRemainingNodes;

		if (!gatherCastersForVisAreas(pVisAreaManager->m_lstPortals))
			return nRemainingNodes;
	}

	// if we got here we processed every tree, so reset tree index
	pFrustum->pShadowCacheData->mOctreePath[curLevel] = 0;

	return nRemainingNodes;
}

#pragma warning(pop)
