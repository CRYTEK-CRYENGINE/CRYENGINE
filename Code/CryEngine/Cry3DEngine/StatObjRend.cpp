// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   statobjrend.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: prepare and add render element into renderer
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "StatObj.h"
#include "../RenderDll/Common/Shadow_Renderer.h"
#include "IndexedMesh.h"
#include "VisAreas.h"
#include <CryMath/GeomQuery.h>
#include "DeformableNode.h"
#include "MatMan.h"

void CStatObj::Render(const SRendParams& rParams, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (m_nFlags & STATIC_OBJECT_HIDDEN)
		return;

#ifndef _RELEASE
	int nMaxDrawCalls = GetCVars()->e_MaxDrawCalls;
	if (nMaxDrawCalls > 0)
	{
		// Don't calculate the number of drawcalls every single time a statobj is rendered.
		// This creates a flickering effect with objects appearing and disappearing indicating that the limit has been reached.
		static int nCurrObjCounter = 0;
		if (((nCurrObjCounter++) & 31) == 1)
		{
			if (GetRenderer()->GetCurrentNumberOfDrawCalls() > nMaxDrawCalls)
				return;
		}
	}
#endif // _RELEASE

	CRenderObject* pObj = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
	FillRenderObject(rParams, rParams.pRenderNode, m_pMaterial, NULL, pObj, passInfo);

	RenderInternal(pObj, rParams.nSubObjHideMask, rParams.lodValue, passInfo);
}

void CStatObj::RenderStreamingDebugInfo(CRenderObject* pRenderObject, const SRenderingPassInfo& passInfo)
{
#ifndef _RELEASE
	//	CStatObj * pStreamable = m_pParentObject ? m_pParentObject : this;

	CStatObj* pStreamable = (m_pLod0 != 0) ? (CStatObj*)m_pLod0 : this;

	int nKB = 0;

	if (pStreamable->m_pRenderMesh)
		nKB += pStreamable->m_nRenderMeshMemoryUsage;

	for (int nLod = 1; pStreamable->m_pLODs && nLod < MAX_STATOBJ_LODS_NUM; nLod++)
	{
		CStatObj* pLod = (CStatObj*)pStreamable->m_pLODs[nLod];

		if (!pLod)
			continue;

		if (pLod->m_pRenderMesh)
			nKB += pLod->m_nRenderMeshMemoryUsage;
	}

	nKB >>= 10;

	if (nKB > GetCVars()->e_StreamCgfDebugMinObjSize)
	{
		//		nKB = GetStreamableContentMemoryUsage(true) >> 10;

		char* pComment = 0;

		pStreamable = pStreamable->m_pParentObject ? pStreamable->m_pParentObject : pStreamable;

		if (!pStreamable->m_bCanUnload)
			pComment = "No stream";
		else if (!pStreamable->m_bLodsAreLoadedFromSeparateFile && pStreamable->m_nLoadedLodsNum)
			pComment = "Single";
		else if (pStreamable->m_nLoadedLodsNum > 1)
			pComment = "Split";
		else
			pComment = "No LODs";

		int nDiff = SATURATEB(int(float(nKB - GetCVars()->e_StreamCgfDebugMinObjSize) / max((int)1, GetCVars()->e_StreamCgfDebugMinObjSize) * 255));
		DrawBBoxLabeled(m_AABB, pRenderObject->GetMatrix(passInfo), ColorB(nDiff, 255 - nDiff, 0, 255),
		                "%.2f mb, %s", 1.f / 1024.f * (float)nKB, pComment);
	}
#endif //_RELEASE
}

//////////////////////////////////////////////////////////////////////
void CStatObj::RenderCoverInfo(CRenderObject* pRenderObject, const SRenderingPassInfo& passInfo)
{
	for (int i = 0; i < GetSubObjectCount(); ++i)
	{
		const IStatObj::SSubObject* subObject = GetSubObject(i);
		if (subObject->nType != STATIC_SUB_OBJECT_DUMMY)
			continue;
		if (strstr(subObject->name, "$cover") == 0)
			continue;

		Vec3 localBoxMin = -subObject->helperSize * 0.5f;
		Vec3 localBoxMax = subObject->helperSize * 0.5f;

		GetRenderer()->GetIRenderAuxGeom()->DrawAABB(
		  AABB(localBoxMin, localBoxMax),
		  pRenderObject->GetMatrix(passInfo) * subObject->localTM,
		  true, ColorB(192, 0, 255, 255),
		  eBBD_Faceted);
	}
}

//////////////////////////////////////////////////////////////////////
void CStatObj::FillRenderObject(const SRendParams& rParams, IRenderNode* pRenderNode, IMaterial* pMaterial,
                                SInstancingInfo* pInstInfo, CRenderObject*& pObj, const SRenderingPassInfo& passInfo)
{

	//  FUNCTION_PROFILER_3DENGINE;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Specify transformation
	////////////////////////////////////////////////////////////////////////////////////////////////////

	IRenderer* pRend = GetRenderer();

	assert(pObj);
	if (!pObj)
		return;

	pObj->m_pRenderNode = pRenderNode;
	pObj->m_editorSelectionID = rParams.nEditorSelectionID;

	SRenderObjData* pOD = NULL;
	if (rParams.pFoliage || rParams.pInstance || rParams.m_pVisArea || pInstInfo || rParams.nVisionParams || rParams.nHUDSilhouettesParams ||
	    rParams.pLayerEffectParams || rParams.nSubObjHideMask != 0)
	{
		pOD = pObj->GetObjData();

		pOD->m_pSkinningData = rParams.pFoliage ? rParams.pFoliage->GetSkinningData(*rParams.pMatrix, passInfo) : NULL;
		if (pOD->m_pSkinningData)
			pObj->m_ObjFlags |= FOB_SKINNED;

		//if (pInstInfo)
		//  pOD->m_pInstancingInfo = &pInstInfo->arrMats;

		pOD->m_uniqueObjectId = reinterpret_cast<uintptr_t>(rParams.pInstance);
		pOD->m_pLayerEffectParams = rParams.pLayerEffectParams;
		pOD->m_nVisionParams = rParams.nVisionParams;
		pOD->m_nHUDSilhouetteParams = rParams.nHUDSilhouettesParams;

		if (rParams.nSubObjHideMask != 0 && (m_pMergedRenderMesh != NULL))
		{
			// Only pass SubObject hide mask for merged objects, because they have a correct correlation between Hide Mask and Render Chunks.
			pOD->m_nSubObjHideMask = rParams.nSubObjHideMask;
			pObj->m_ObjFlags |= FOB_MESH_SUBSET_INDICES;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Set flags
	////////////////////////////////////////////////////////////////////////////////////////////////////

	pObj->m_ObjFlags |= FOB_TRANS_MASK;
	pObj->m_ObjFlags |= rParams.dwFObjFlags;
	//  SRenderObjData *pObjData = NULL;

	if (rParams.nTextureID >= 0)
		pObj->m_nTextureID = rParams.nTextureID;

	assert(rParams.pMatrix);
	{
		pObj->SetMatrix(*rParams.pMatrix, passInfo);
	}

	pObj->SetAmbientColor(rParams.AmbientColor, passInfo);
	pObj->m_nClipVolumeStencilRef = rParams.nClipVolumeStencilRef;

	pObj->m_ObjFlags |= FOB_INSHADOW;
	pObj->m_fAlpha = rParams.fAlpha;
	pObj->m_DissolveRef = rParams.nDissolveRef;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Process bending
	////////////////////////////////////////////////////////////////////////////////////////////////////
	if (pRenderNode && pRenderNode->GetRndFlags() & ERF_RECVWIND)
	{
		// This can be different for CVegetation class render nodes
		pObj->SetBendingData({ 1.0f, GetRadiusVert() }, passInfo);
	}
	else
	{
		pObj->SetBendingData({ 0.0f, 0.0f }, passInfo);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Set render quality
	////////////////////////////////////////////////////////////////////////////////////////////////////

	pObj->m_fDistance = rParams.fDistance;

	{
		//clear, when exchange the state of pLightMapInfo to NULL, the pObj parameters must be update...
		pObj->m_nSort = fastround_positive(rParams.fDistance * 2.0f);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Add render elements
	////////////////////////////////////////////////////////////////////////////////////////////////////
	if (rParams.pMaterial)
		pMaterial = rParams.pMaterial;

	// prepare multi-layer stuff to render object
	if (!rParams.nMaterialLayersBlend && rParams.nMaterialLayers)
	{
		uint8 nFrozenLayer = (rParams.nMaterialLayers & MTL_LAYER_FROZEN) ? MTL_LAYER_FROZEN_MASK : 0;
		uint8 nWetLayer = (rParams.nMaterialLayers & MTL_LAYER_WET) ? MTL_LAYER_WET_MASK : 0;
		pObj->m_nMaterialLayers = (uint32) (nFrozenLayer << 24) | (nWetLayer << 16);
	}
	else
		pObj->m_nMaterialLayers = rParams.nMaterialLayersBlend;

	if (rParams.pTerrainTexInfo && (rParams.dwFObjFlags & (FOB_BLEND_WITH_TERRAIN_COLOR /* | FOB_AMBIENT_OCCLUSION*/)))
	{
		pObj->m_nTextureID = rParams.pTerrainTexInfo->nTex0;
		//pObj->m_nTextureID1 = rParams.pTerrainTexInfo->nTex1;
		if (!pOD)
		{
			pOD = pObj->GetObjData();
		}
		pOD->m_fTempVars[0] = rParams.pTerrainTexInfo->fTexOffsetX;
		pOD->m_fTempVars[1] = rParams.pTerrainTexInfo->fTexOffsetY;
		pOD->m_fTempVars[2] = rParams.pTerrainTexInfo->fTexScale;
		pOD->m_fTempVars[3] = 0;
		pOD->m_fTempVars[4] = 0;
	}

	if (rParams.nCustomData || rParams.nCustomFlags)
	{
		if (!pOD)
		{
			pOD = pObj->GetObjData();
		}

		pOD->m_nCustomData = rParams.nCustomData;
		pOD->m_nCustomFlags = rParams.nCustomFlags;

		if (rParams.nCustomFlags & COB_CLOAK_HIGHLIGHT)
		{
			pOD->m_fTempVars[5] = rParams.fCustomData[0];
		}
		else if (rParams.nCustomFlags & COB_POST_3D_RENDER)
		{
			memcpy(&pOD->m_fTempVars[5], &rParams.fCustomData[0], sizeof(float) * 4);
			pObj->m_fAlpha = 1.0f; // Use the alpha in the post effect instead of here
			pOD->m_fTempVars[9] = rParams.fAlpha;
		}
	}

	if (rParams.pFoliage)
		pObj->m_ObjFlags |= FOB_DYNAMIC_OBJECT;

	if (rParams.nAfterWater)
		pObj->m_ObjFlags |= FOB_AFTER_WATER;
	else
		pObj->m_ObjFlags &= ~FOB_AFTER_WATER;

	pObj->m_pRenderNode = rParams.pRenderNode;
	pObj->m_pCurrMaterial = pMaterial;

	if (Get3DEngine()->IsTessellationAllowed(pObj, passInfo))
	{
		// Allow this RO to be tessellated, however actual tessellation will be applied if enabled in material
		pObj->m_ObjFlags |= FOB_ALLOW_TESSELLATION;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::RenderDebugInfo(CRenderObject* pObj, const SRenderingPassInfo& passInfo)
{
#ifndef _RELEASE

	if (!passInfo.IsGeneralPass())
		return false;

	IMaterial* pMaterial = pObj->m_pCurrMaterial;

	IRenderAuxGeom* pAuxGeom = GetRenderer()->GetIRenderAuxGeom();
	if (!pAuxGeom)
		return false;

	Matrix34 tm = pObj->GetMatrix(passInfo);

	// Convert "camera space" to "world space"
	if (pObj->m_ObjFlags & FOB_NEAREST)
	{
		tm.AddTranslation(passInfo.GetCamera().GetPosition());
	}

	bool bOnlyBoxes = GetCVars()->e_DebugDraw == -1;

	int e_DebugDraw = GetCVars()->e_DebugDraw;
	string e_DebugDrawFilter = GetCVars()->e_DebugDrawFilter->GetString();
	bool bHasHelperFilter = e_DebugDrawFilter != "";
	bool bFiltered = false;

	if (e_DebugDraw == 1)
	{
		string name;
		if (!m_szGeomName.empty())
			name = m_szGeomName.c_str();
		else
			name = PathUtil::GetFile(m_szFileName.c_str());

		bFiltered = name.find(e_DebugDrawFilter) == string::npos;
	}

	if (pObj->m_fDistance > GetCVars()->e_DebugDrawMaxDistance)
		return false;

	if ((GetCVars()->e_DebugDraw == 1 || bOnlyBoxes) && !bFiltered)
	{
		if (!m_bMerged)
			pAuxGeom->DrawAABB(m_AABB, tm, false, ColorB(0, 255, 255, 128), eBBD_Faceted);
		else
			pAuxGeom->DrawAABB(m_AABB, tm, false, ColorB(255, 200, 0, 128), eBBD_Faceted);
	}

	bool bNoText = e_DebugDraw < 0;
	if (e_DebugDraw < 0)
		e_DebugDraw = -e_DebugDraw;

	if (m_nRenderTrisCount > 0 && !bOnlyBoxes && !bFiltered)
	{
		// cgf's name and tris num

		int nThisLod = 0;
		if (m_pLod0 && m_pLod0->m_pLODs)
		{
			for (int i = 0; i < MAX_STATOBJ_LODS_NUM; i++)
			{
				if (m_pLod0->m_pLODs[i] == this)
				{
					nThisLod = i;
					break;
				}
			}
		}

		const int nMaxUsableLod = (m_pLod0 != 0) ? m_pLod0->m_nMaxUsableLod : m_nMaxUsableLod;
		const int nRealNumLods = (m_pLod0 != 0) ? m_pLod0->m_nLoadedLodsNum : m_nLoadedLodsNum;

		int nNumLods = nRealNumLods;
		if (nNumLods > nMaxUsableLod + 1)
			nNumLods = nMaxUsableLod + 1;

		int nLod = nThisLod;
		if (nLod > nNumLods - 1)
			nLod = nNumLods - 1;

		Vec3 pos = tm.TransformPoint(m_AABB.GetCenter());
		float color[4] = { 1, 1, 1, 1 };
		int nMats = m_pRenderMesh ? m_pRenderMesh->GetChunks().size() : 0;
		int nRenderMats = 0;

		if (nMats)
		{
			for (int i = 0; i < nMats; ++i)
			{
				CRenderChunk& rc = m_pRenderMesh->GetChunks()[i];
				if (rc.pRE && rc.nNumIndices && rc.nNumVerts && ((rc.m_nMatFlags & MTL_FLAG_NODRAW) == 0))
					++nRenderMats;
			}
		}

		switch (e_DebugDraw)
		{
		case 1:
			{
				float fontSize = 1.3f;
				ColorB clr;

				if (nLod == 0)
				{
					clr = ColorB(255, 0, 0, 255);
					fontSize = 1.3f;
				}
				else if (nLod == 1)
				{
					clr = ColorB(0, 255, 0, 255);
					fontSize = 1.5f;
				}
				else if (nLod == 2)
				{
					clr = ColorB(200, 100, 255, 255);
					fontSize = 1.7f;
				}
				else if (nLod == 3)
				{
					clr = ColorB(0, 255, 255, 255);
					fontSize = 1.9f;
				}
				else if (nLod == 4)
				{
					clr = ColorB(255, 255, 0, 255);
					fontSize = 2.1f;
				}
				else if (nLod == 5)
				{
					clr = ColorB(255, 0, 255, 255);
					fontSize = 2.3f;
				}
				else
				{
					clr = ColorB(255, 255, 255, 255);
					fontSize = 2.5f;
				}
				
				const char* shortName = "";
				if (!m_szGeomName.empty())
					shortName = m_szGeomName.c_str();
				else
					shortName = PathUtil::GetFile(m_szFileName.c_str());
				if (nNumLods > 1)
					IRenderAuxText::DrawLabelExF(pos, fontSize, clr, true, true, "%s\n%d (LOD %d/%d)", shortName, m_nRenderTrisCount, nLod, nNumLods);
				else
					IRenderAuxText::DrawLabelExF(pos, fontSize, clr, true, true, "%s\n%d", shortName, m_nRenderTrisCount);
			}
			break;

		case 2:
			{
				//////////////////////////////////////////////////////////////////////////
				// Show colored poly count.
				//////////////////////////////////////////////////////////////////////////
				int fMult = 1;
				int nTris = m_nRenderTrisCount;
				ColorB clr = ColorB(0, 0, 0, 255);
				if (nTris >= 20000 * fMult)
					clr = ColorB(255, 0, 0, 255);
				else if (nTris >= 10000 * fMult)
					clr = ColorB(255, 255, 0, 255);
				else if (nTris >= 5000 * fMult)
					clr = ColorB(0, 255, 0, 255);
				else if (nTris >= 2500 * fMult)
					clr = ColorB(0, 255, 255, 255);
				else if (nTris > 1250 * fMult)
					clr = ColorB(0, 0, 255, 255);

				if (pMaterial)
					pMaterial = GetMatMan()->GetDefaultHelperMaterial();
				if (pObj)
				{
					pObj->m_nMaterialLayers = 0;
					pObj->m_ObjFlags |= FOB_SELECTED;
					pObj->m_data.m_nHUDSilhouetteParams = RGBA8(255.0f, clr.b, clr.g, clr.r);
				}

				if (!bNoText)
					IRenderAuxText::DrawLabelExF(pos, 1.3f, color, true, true, "%d", m_nRenderTrisCount);

				return false;
				//////////////////////////////////////////////////////////////////////////
			}
		case 3:
			{
				//////////////////////////////////////////////////////////////////////////
				// Show Lods
				//////////////////////////////////////////////////////////////////////////
				ColorB clr;
				if (nNumLods < 2)
				{
					if (m_nRenderTrisCount <= GetCVars()->e_LodMinTtris || nRealNumLods > 1)
					{
						clr = ColorB(50, 50, 50, 255);
					}
					else
					{
						clr = ColorB(255, 0, 0, 255);
						float fAngle = gEnv->pTimer->GetFrameStartTime().GetPeriodicFraction(1.0f) * gf_PI2;
						clr.g = 127 + (int)(sinf(fAngle) * 120);  // flashing color
					}
				}
				else
				{
					if (nLod == 0)
						clr = ColorB(255, 0, 0, 255);
					else if (nLod == 1)
						clr = ColorB(0, 255, 0, 255);
					else if (nLod == 2)
						clr = ColorB(0, 0, 255, 255);
					else if (nLod == 3)
						clr = ColorB(0, 255, 255, 255);
					else if (nLod == 4)
						clr = ColorB(255, 255, 0, 255);
					else if (nLod == 5)
						clr = ColorB(255, 0, 255, 255);
					else
						clr = ColorB(255, 255, 255, 255);
				}

				if (pMaterial)
					pMaterial = GetMatMan()->GetDefaultHelperMaterial();
				if (pObj)
				{
					pObj->m_nMaterialLayers = 0;
					pObj->m_ObjFlags |= FOB_SELECTED;
					pObj->m_data.m_nHUDSilhouetteParams = RGBA8(255.0f, clr.b, clr.g, clr.r);
				}

				if (pObj && nNumLods > 1 && !bNoText)
				{
					const int nLod0 = GetMinUsableLod();
					const int maxLod = GetMaxUsableLod();
					const bool bRenderNodeValid(pObj && pObj->m_pRenderNode && ((UINT_PTR)(void*)(pObj->m_pRenderNode) > 0));
					IRenderNode* pRN = (IRenderNode*)pObj->m_pRenderNode;
					IRenderAuxText::DrawLabelExF(pos, 1.3f, color, true, true, "%d [%d;%d] (%d/%.1f)",
					                   nLod, nLod0, maxLod,
					                   bRenderNodeValid ? pRN->GetLodRatio() : -1, pObj->m_fDistance);
				}

				return false;
				//////////////////////////////////////////////////////////////////////////
			}
		case 4:
			{
				// Show texture usage.
				if (m_pRenderMesh)
				{
					int nTexMemUsage = m_pRenderMesh->GetTextureMemoryUsage(pMaterial);
					IRenderAuxText::DrawLabelExF(pos, 1.3f, color, true, true, "%d", nTexMemUsage / 1024);    // in KByte
				}
			}
			break;

		case 5:
			{
				//////////////////////////////////////////////////////////////////////////
				// Show Num Render materials.
				//////////////////////////////////////////////////////////////////////////
				ColorB clr(0, 0, 0, 0);
				if (nRenderMats == 1)
					clr = ColorB(0, 0, 255, 255);
				else if (nRenderMats == 2)
					clr = ColorB(0, 255, 255, 255);
				else if (nRenderMats == 3)
					clr = ColorB(0, 255, 0, 255);
				else if (nRenderMats == 4)
					clr = ColorB(255, 0, 255, 255);
				else if (nRenderMats == 5)
					clr = ColorB(255, 255, 0, 255);
				else if (nRenderMats >= 6)
					clr = ColorB(255, 0, 0, 255);
				else if (nRenderMats >= 11)
					clr = ColorB(255, 255, 255, 255);

				if (pMaterial)
					pMaterial = GetMatMan()->GetDefaultHelperMaterial();
				if (pObj)
				{
					pObj->m_nMaterialLayers = 0;
					pObj->m_ObjFlags |= FOB_SELECTED;
					pObj->m_data.m_nHUDSilhouetteParams = RGBA8(255.0f, clr.b, clr.g, clr.r);
				}

				if (!bNoText)
					IRenderAuxText::DrawLabelExF(pos, 1.3f, color, true, true, "%d", nRenderMats);
			}
			break;

		case 6:
			{
				if (pMaterial)
					pMaterial = GetMatMan()->GetDefaultHelperMaterial();

				ColorF col(1, 1, 1, 1);
				if (pObj)
				{
					pObj->m_nMaterialLayers = 0;
					col = pObj->GetAmbientColor(passInfo);
				}

				IRenderAuxText::DrawLabelExF(pos, 1.3f, color, true, true, "%d,%d,%d,%d", (int)(col.r * 255.0f), (int)(col.g * 255.0f), (int)(col.b * 255.0f), (int)(col.a * 255.0f));
			}
			break;

		case 7:
			if (m_pRenderMesh)
			{
				int nTexMemUsage = m_pRenderMesh->GetTextureMemoryUsage(pMaterial);
				IRenderAuxText::DrawLabelExF(pos, 1.3f, color, true, true, "%d,%d,%d", m_nRenderTrisCount, nRenderMats, nTexMemUsage / 1024);
			}
			break;
		case 8:
			{
				if (pObj && pObj->m_pRenderNode)
				{
					IRenderNode* pRenderNode = (IRenderNode*)pObj->m_pRenderNode;
					float perObjectMaxViewDist = pRenderNode->m_ucViewDistRatio;

					ColorF clr = ColorF(0.f, 1.f, 0.f, 1.0f);
					Vec3 red, green;
					ColorF(1.f, 0.f, 0.f, 1.0f).toHSV(red.x, red.y, red.z);
					ColorF(0.f, 1.f, 0.f, 1.0f).toHSV(green.x, green.y, green.z);

					float interpValue = perObjectMaxViewDist / 255.0f;
					Vec3 c = Vec3::CreateLerp(green, red, interpValue);
					clr.fromHSV(c.x, c.y, c.z);

					float fontSize = 1.3f;
					fontSize = LERP(fontSize, 2.3f, interpValue);

					IRenderAuxText::DrawLabelExF(pos, fontSize, clr, true, true, "%.1f", perObjectMaxViewDist);
				}
			}
			break;

		case 16:
			{
				// Draw stats for object selected by debug gun
				if (GetRenderer()->IsDebugRenderNode((IRenderNode*)pObj->m_pRenderNode))
				{
					const char* shortName = PathUtil::GetFile(m_szFileName.c_str());

					int texMemUsage = 0;

					if (m_pRenderMesh)
					{
						texMemUsage = m_pRenderMesh->GetTextureMemoryUsage(pMaterial);
					}

					pAuxGeom->DrawAABB(m_AABB, tm, false, ColorB(0, 255, 255, 128), eBBD_Faceted);

					float yellow[4] = { 1.f, 1.f, 0.f, 1.f };

					const float yOffset = 165.f;
					const float xOffset = 970.f;

					if (m_pParentObject == NULL)
					{
						IRenderAuxText::Draw2dLabel(xOffset, 40.f, 1.5f, yellow, false, "%s", shortName);

						IRenderAuxText::Draw2dLabel(xOffset, yOffset, 1.5f, color, false,
						                   //"Mesh: %s\n"
						                   "LOD: %d/%d\n"
						                   "Num Instances: %d\n"
						                   "Num Tris: %d\n"
						                   "Tex Mem usage: %.2f kb\n"
						                   "Mesh Mem usage: %.2f kb\n"
						                   "Num Materials: %d\n"
						                   "Mesh Type: %s\n",
						                   //shortName,
						                   nLod, nNumLods,
						                   m_nUsers,
						                   m_nRenderTrisCount,
						                   texMemUsage / 1024.f,
						                   m_nRenderMeshMemoryUsage / 1024.f,
						                   nRenderMats,
						                   m_pRenderMesh->GetTypeName());
					}
					else
					{
						for (int i = 0; i < m_pParentObject->SubObjectCount(); i++)
						{
							//find subobject position
							if (m_pParentObject->SubObject(i).pStatObj == this)
							{
								//only render the header once
								if (i == 0)
								{
									IRenderAuxText::Draw2dLabel(600.f, 40.f, 2.f, yellow, false, "Debug Gun: %s", shortName);
								}
								float y = yOffset + ((i % 4) * 150.f);
								float x = xOffset - (floor(i / 4.f) * 200.f);

								IRenderAuxText::Draw2dLabel(x, y, 1.5f, color, false,
								                   "Sub Mesh: %s\n"
								                   "LOD: %d/%d\n"
								                   "Num Instances: %d\n"
								                   "Num Tris: %d\n"
								                   "Tex Mem usage: %.2f kb\n"
								                   "Mesh Mem usage: %.2f kb\n"
								                   "Num Materials: %d\n"
								                   "Mesh Type: %s\n",
								                   m_szGeomName.c_str() ? m_szGeomName.c_str() : "UNKNOWN",
								                   nLod, nNumLods,
								                   m_nUsers,
								                   m_nRenderTrisCount,
								                   texMemUsage / 1024.f,
								                   m_nRenderMeshMemoryUsage / 1024.f,
								                   nRenderMats,
								                   m_pRenderMesh->GetTypeName());

								break;
							}
						}
					}
				}
			}
			break;

		case 19:  // Displays the triangle count of physic proxies.
			if (!bNoText)
			{
				int nPhysTrisCount = 0;
				for (int j = 0; j < MAX_PHYS_GEOMS_TYPES; ++j)
				{
					if (GetPhysGeom(j))
						nPhysTrisCount += GetPhysGeom(j)->pGeom->GetPrimitiveCount();
				}

				if (nPhysTrisCount == 0)
					color[3] = 0.1f;

				IRenderAuxText::DrawLabelExF(pos, 1.3f, color, true, true, "%d", nPhysTrisCount);
			}
			return false;

		case 22:
			{
				// Show texture usage.
				if (m_pRenderMesh)
				{
					IRenderAuxText::DrawLabelExF(pos, 1.3f, color, true, true, "[LOD %d: %d]", nLod, m_pRenderMesh->GetVerticesCount());
				}
			}
			break;
		case 23:
			{
				if (pObj && pObj->m_pRenderNode)
				{
					IRenderNode* pRenderNode = (IRenderNode*)pObj->m_pRenderNode;
					const bool bCastsShadow = (pRenderNode->GetRndFlags() & ERF_CASTSHADOWMAPS) != 0;
					ColorF clr = bCastsShadow ? ColorF(1.f, 0.f, 0.f, 1.0f) : ColorF(0.f, 1.f, 0.f, 1.f);

					int nIndices = 0;
					int nIndicesNoShadow = 0;

					// figure out how many primitives actually cast shadows
					if (pMaterial && bCastsShadow)
					{
						for (int i = 0; i < nMats; ++i)
						{
							CRenderChunk& rc = m_pRenderMesh->GetChunks()[i];
							if (rc.pRE && rc.nNumIndices && rc.nNumVerts && ((rc.m_nMatFlags & MTL_FLAG_NODRAW) == 0))
							{
								SShaderItem& ShaderItem = pMaterial->GetShaderItem(rc.m_nMatID);
								IRenderShaderResources* pR = ShaderItem.m_pShaderResources;

								if (pR && (pR->GetResFlags() & MTL_FLAG_NOSHADOW))
									nIndicesNoShadow += rc.nNumIndices;

								nIndices += rc.nNumIndices;
							}
						}

						Vec3 red, green;
						ColorF(1.f, 0.f, 0.f, 1.0f).toHSV(red.x, red.y, red.z);
						ColorF(0.f, 1.f, 0.f, 1.0f).toHSV(green.x, green.y, green.z);

						Vec3 c = Vec3::CreateLerp(red, green, (float)nIndicesNoShadow / max(nIndices, 1));
						clr.fromHSV(c.x, c.y, c.z);

						pMaterial = GetMatMan()->GetDefaultHelperMaterial();
					}
					
					pObj->m_nMaterialLayers = 0;
					pObj->m_ObjFlags |= FOB_SELECTED;
					pObj->m_data.m_nHUDSilhouetteParams = RGBA8(255.0f, clr.b * 255.0f, clr.g * 255.0f, clr.r * 255.0f);
				}
				return false;
			}
		}
	}

	if (GetCVars()->e_DebugDraw == 15 && !bOnlyBoxes)
	{
		// helpers
		for (int i = 0; i < (int)m_subObjects.size(); i++)
		{
			SSubObject* pSubObject = &(m_subObjects[i]);
			if (pSubObject->nType == STATIC_SUB_OBJECT_MESH && pSubObject->pStatObj)
				continue;

			if (bHasHelperFilter)
			{
				if (pSubObject->name.find(e_DebugDrawFilter) == string::npos)
					continue;
			}

			// make object matrix
			Matrix34 tMat = tm * pSubObject->tm;
			Vec3 pos = tMat.GetTranslation();

			// draw axes
			float s = 0.02f;
			ColorB col(0, 255, 255, 255);
			pAuxGeom->DrawAABB(AABB(Vec3(-s, -s, -s), Vec3(s, s, s)), tMat, false, col, eBBD_Faceted);
			pAuxGeom->DrawLine(pos + s * tMat.GetColumn1(), col, pos + 3.f * s * tMat.GetColumn1(), col);

			// text
			float color[4] = { 0, 1, 1, 1 };
			IRenderAuxText::DrawLabelEx(pos, 1.3f, color, true, true, pSubObject->name.c_str());
		}
	}

	if (Get3DEngine()->IsDebugDrawListEnabled())
	{
		I3DEngine::SObjectInfoToAddToDebugDrawList objectInfo;
		if (pObj->m_pRenderNode)
		{
			objectInfo.pName = ((IRenderNode*)pObj->m_pRenderNode)->GetName();
			objectInfo.pClassName = ((IRenderNode*)pObj->m_pRenderNode)->GetEntityClassName();
		}
		else
		{
			objectInfo.pName = "";
			objectInfo.pClassName = "";
		}
		objectInfo.pFileName = m_szFileName.c_str();
		if (m_pRenderMesh && pObj->m_pCurrMaterial)
			objectInfo.texMemory = m_pRenderMesh->GetTextureMemoryUsage(pObj->m_pCurrMaterial);
		else
			objectInfo.texMemory = 0;
		objectInfo.numTris = m_nRenderTrisCount;
		objectInfo.numVerts = m_nLoadedVertexCount;
		objectInfo.meshMemory = m_nRenderMeshMemoryUsage;
		objectInfo.pMat = &tm;
		objectInfo.pBox = &m_AABB;
		objectInfo.type = I3DEngine::DLOT_STATOBJ;
		objectInfo.pRenderNode = (IRenderNode*)(pObj->m_pRenderNode);
		Get3DEngine()->AddObjToDebugDrawList(objectInfo);
	}

#endif //_RELEASE
	return false;
}

//
// StatObj functions.
//

float CStatObj::GetExtent(EGeomForm eForm)
{
	int nSubCount = m_subObjects.size();
	if (!nSubCount)
	{
		return m_pRenderMesh ? m_pRenderMesh->GetExtent(eForm) : 0.f;
	}

	CGeomExtent& ext = m_Extents.Make(eForm);
	if (!ext || ext.TotalExtent() == 0.0f)
	{
		ext.Clear();

		// Create parts for main and sub-objects.
		ext.ReserveParts(1 + nSubCount);

		ext.AddPart(m_pRenderMesh ? m_pRenderMesh->GetExtent(eForm) : 0.f);

		// Evaluate sub-objects.
		for (int i = 0; i < nSubCount; i++)
		{
			IStatObj::SSubObject* pSub = &m_subObjects[i];
			if (pSub->nType == STATIC_SUB_OBJECT_MESH && pSub->pStatObj)
			{
				float fExt = pSub->pStatObj->GetExtent(eForm);
				fExt *= ScaleExtent(eForm, pSub->tm);
				ext.AddPart(fExt);
			}
			else
				ext.AddPart(0.f);
		}
	}
	return ext.TotalExtent();
}

void CStatObj::GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const
{
	if (!m_subObjects.empty())
	{
		CGeomExtent const& ext = m_Extents[eForm];
		for (auto part : ext.RandomPartsAliasSum(points, seed))
		{
			if (part.iPart > 0)
			{
				IStatObj::SSubObject const* pSub = &m_subObjects[part.iPart - 1];
				assert(pSub && pSub->pStatObj);
				pSub->pStatObj->GetRandomPoints(part.aPoints, seed, eForm);
				for (auto& point : part.aPoints)
					point <<= pSub->tm;
			}
			else if (m_pRenderMesh)
				m_pRenderMesh->GetRandomPoints(part.aPoints, seed, eForm);
			else
				part.aPoints.fill(ZERO);
		}
	}
	else if (m_pRenderMesh)
		m_pRenderMesh->GetRandomPoints(points, seed, eForm);
	else
		points.fill(ZERO);
}

SMeshLodInfo CStatObj::ComputeAndStoreLodDistances()
{
	SMeshLodInfo lodInfo;
	lodInfo.Clear();

	lodInfo.fGeometricMean = m_fGeometricMeanFaceArea;
	lodInfo.nFaceCount = m_nRenderTrisCount;

	if (GetFlags() & STATIC_OBJECT_COMPOUND)
	{
		for (uint i = 0; i < m_subObjects.size(); ++i)
		{
			if (m_subObjects[i].nType == STATIC_SUB_OBJECT_MESH && m_subObjects[i].bShadowProxy == false && m_subObjects[i].pStatObj != NULL)
			{
				CStatObj* pStatObj = static_cast<CStatObj*>(m_subObjects[i].pStatObj);

				SMeshLodInfo subLodInfo = pStatObj->ComputeAndStoreLodDistances();

				lodInfo.Merge(subLodInfo);
			}
		}
	}

	m_fLodDistance = sqrt(lodInfo.fGeometricMean);
	return lodInfo;
}

SMeshLodInfo CStatObj::ComputeGeometricMean() const
{
	SMeshLodInfo lodInfo;
	lodInfo.Clear();

	lodInfo.fGeometricMean = m_fGeometricMeanFaceArea;
	lodInfo.nFaceCount = m_nRenderTrisCount;

	if (GetFlags() & STATIC_OBJECT_COMPOUND)
	{
		for (uint i = 0; i < m_subObjects.size(); ++i)
		{
			if (m_subObjects[i].nType == STATIC_SUB_OBJECT_MESH && m_subObjects[i].bShadowProxy == false && m_subObjects[i].pStatObj != NULL)
			{
				SMeshLodInfo subLodInfo = static_cast<const CStatObj*>(m_subObjects[i].pStatObj)->ComputeGeometricMean();

				lodInfo.Merge(subLodInfo);
			}
		}
	}

	return lodInfo;
}

int CStatObj::ComputeLodFromScale(float fScale, float fLodRatioNormalized, float fEntDistance, bool bFoliage, bool bForPrecache)
{
	assert(!m_pLod0);

	int nNewLod = 0;

	// use legacy LOD calculation, without an IRenderNode
	nNewLod = (int)(fEntDistance * (fLodRatioNormalized * fLodRatioNormalized) / (max(GetCVars()->e_LodRatio * min(GetRadius() * fScale, GetFloatCVar(e_LodCompMaxSize)), 0.001f)));

	const int nMinLod = GetMinUsableLod();
	nNewLod += nMinLod;

	int nMaxUsableLod = min((int)m_nMaxUsableLod, GetCVars()->e_LodMax);
	if (!nMaxUsableLod && bForPrecache)
		nMaxUsableLod = 1;

	nNewLod = bFoliage ? 0 : CLAMP(nNewLod, 0, nMaxUsableLod);

	return nNewLod;
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::DebugDraw(const SGeometryDebugDrawInfo& info)
{
	if (m_nFlags & STATIC_OBJECT_COMPOUND && !m_bMerged)
	{
		// Draw sub objects.
		for (int i = 0; i < (int)m_subObjects.size(); i++)
		{
			if (!m_subObjects[i].pStatObj || m_subObjects[i].bHidden || m_subObjects[i].nType != STATIC_SUB_OBJECT_MESH)
				continue;

			SGeometryDebugDrawInfo subInfo = info;
			subInfo.tm = info.tm * m_subObjects[i].tm;
			m_subObjects[i].pStatObj->DebugDraw(subInfo);
		}
	}
	else if (m_pRenderMesh)
	{
		m_pRenderMesh->DebugDraw(info, ~0);
	}
	else
	{
		// No RenderMesh in here, so probably no geometry in highest LOD, find it in lower LODs
		if (m_pLODs)
		{
			assert(m_nMaxUsableLod < MAX_STATOBJ_LODS_NUM);
			for (int nLod = 0; nLod <= (int)m_nMaxUsableLod; nLod++)
			{
				if (m_pLODs[nLod] && m_pLODs[nLod]->m_pRenderMesh)
				{
					m_pLODs[nLod]->m_pRenderMesh->DebugDraw(info, ~0);
					break;
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void CStatObj::RenderInternal(CRenderObject* pRenderObject, hidemask nSubObjectHideMask, const CLodValue& lodValue, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	if (m_nFlags & STATIC_OBJECT_HIDDEN)
		return;

	m_nLastDrawMainFrameId = passInfo.GetMainFrameID();
	if (m_pParentObject)
		m_pParentObject->m_nLastDrawMainFrameId = passInfo.GetMainFrameID();

	hidemask *pMask = !nSubObjectHideMask ? &m_nInitialSubObjHideMask : &nSubObjectHideMask;
	if (!!*pMask)
	{
		if ((m_pMergedRenderMesh != NULL) && !(pRenderObject->m_ObjFlags & FOB_MESH_SUBSET_INDICES)) // If not already set by per-instance hide mask.
		{
			SRenderObjData* pOD = pRenderObject->GetObjData();

			// Only pass SubObject hide mask for merged objects, because they have a correct correlation between Hide Mask and Render Chunks.
			pOD->m_nSubObjHideMask = *pMask;
			pRenderObject->m_ObjFlags |= FOB_MESH_SUBSET_INDICES;
		}
	}

	if (pRenderObject->m_pRenderNode)
	{
		IRenderNode* pRN = (IRenderNode*)pRenderObject->m_pRenderNode;
		if (m_bEditor)
		{
			if (pRN->m_dwRndFlags & ERF_SELECTED)
			{
				m_nSelectedFrameId = passInfo.GetMainFrameID();
				if (m_pParentObject)
					m_pParentObject->m_nSelectedFrameId = passInfo.GetMainFrameID();
				pRenderObject->m_ObjFlags |= FOB_SELECTED;
			}
			else
				pRenderObject->m_ObjFlags &= ~FOB_SELECTED;

			if (!gEnv->IsEditing() && pRN->m_dwRndFlags & ERF_RAYCAST_PROXY)
			{
				return;
			}
		}
		else
		{
			if (pRN->m_dwRndFlags & ERF_RAYCAST_PROXY)
			{
				return;
			}
		}
	}

	if ((m_nFlags & STATIC_OBJECT_COMPOUND) && !m_bMerged)
	{
		//////////////////////////////////////////////////////////////////////////
		// Render SubMeshes if present.
		//////////////////////////////////////////////////////////////////////////
		if (m_nSubObjectMeshCount > 0)
		{
			CRenderObject* pRenderObjectB = NULL;

			if (lodValue.DissolveRefB() != 255)
			{
				pRenderObject->m_DissolveRef = lodValue.DissolveRefA();

				if (pRenderObject->m_DissolveRef)
				{
					if (!(pRenderObject->m_ObjFlags & FOB_DISSOLVE))
						pRenderObject->m_ObjFlags &= ~FOB_UPDATED_RTMASK;
					pRenderObject->m_ObjFlags |= FOB_DISSOLVE;

					pRenderObject->m_ObjFlags |= FOB_DISSOLVE_OUT;
				}
				else
				{
					if ((pRenderObject->m_ObjFlags & FOB_DISSOLVE))
						pRenderObject->m_ObjFlags &= ~FOB_UPDATED_RTMASK;
					pRenderObject->m_ObjFlags &= ~FOB_DISSOLVE;
				}

				if (lodValue.LodB() != -1)
				{
					pRenderObjectB = GetRenderer()->EF_DuplicateRO(pRenderObject, passInfo);
					pRenderObjectB->m_ObjFlags &= ~FOB_DISSOLVE_OUT;
				}
			}
			else
			{
				pRenderObject->m_DissolveRef = 0;
				if ((pRenderObject->m_ObjFlags & FOB_DISSOLVE))
					pRenderObject->m_ObjFlags &= ~FOB_UPDATED_RTMASK;
				pRenderObject->m_ObjFlags &= ~(FOB_DISSOLVE | FOB_DISSOLVE_OUT);
			}

			hidemaskOneBit nBitIndex = hidemask1;
			Matrix34A renderTM = pRenderObject->GetMatrix(passInfo);
			for (int32 i = 0, subObjectsSize = m_subObjects.size(); i < subObjectsSize; ++i, nBitIndex <<= 1)
			{
				const SSubObject& subObj = m_subObjects[i];
				if (subObj.nType == STATIC_SUB_OBJECT_MESH)  // all the meshes are at the beginning of the array.
				{
					CStatObj* const __restrict pStatObj = (CStatObj*)subObj.pStatObj;

					if (pStatObj &&
						(pStatObj->m_nRenderTrisCount >= 2) &&
						!(pStatObj->m_nFlags & STATIC_OBJECT_HIDDEN) &&
						!(*pMask & nBitIndex)
						)
					{
						PrefetchLine(pRenderObject, 0);
						
						if (lodValue.LodA() >= 0)
						{
							RenderSubObject(pRenderObject, lodValue.LodA(), i, renderTM, passInfo);
						}

						if (pRenderObjectB && lodValue.LodB()>=0)
						{
							PrefetchLine(pRenderObjectB, 0);
							RenderSubObject(pRenderObjectB, lodValue.LodB(), i, renderTM, passInfo);
						}
					}
				}
				else
				{
					break;
				}
			}

			if (GetCVars()->e_DebugDraw)
			{
				RenderDebugInfo(pRenderObject, passInfo);
			}
		}

		//////////////////////////////////////////////////////////////////////////
	}
	else
	{
		// draw mesh, don't even try to render childs
		if (lodValue.LodA() >= 0)
		{
			RenderObjectInternal(pRenderObject, lodValue.LodA(), lodValue.DissolveRefA(), true, passInfo);
		}

		if (lodValue.DissolveRefB() != 255 && lodValue.LodB() >= 0) // check here since we're passing in A's ref.
		{
			pRenderObject = GetRenderer()->EF_DuplicateRO(pRenderObject, passInfo);
			RenderObjectInternal(pRenderObject, lodValue.LodB(), lodValue.DissolveRefA(), false, passInfo);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
void CStatObj::RenderSubObject(CRenderObject* pRenderObject, int nLod,
	int nSubObjId, const Matrix34A& renderTM, const SRenderingPassInfo& passInfo)
{
	const SSubObject& subObj = m_subObjects[nSubObjId];

	CStatObj* const __restrict pStatObj = (CStatObj*)subObj.pStatObj;

	if (pStatObj == NULL)
		return;

	SRenderObjData* pOD = 0;
	if (subObj.pFoliage)
	{
		pRenderObject = GetRenderer()->EF_DuplicateRO(pRenderObject, passInfo);
		pOD = pRenderObject->GetObjData();
		pOD->m_pSkinningData = subObj.pFoliage->GetSkinningData(pRenderObject->GetMatrix(passInfo), passInfo);
		pOD->m_uniqueObjectId = reinterpret_cast<uintptr_t>(subObj.pFoliage);
		pRenderObject->m_ObjFlags |= FOB_SKINNED | FOB_DYNAMIC_OBJECT;
		((CStatObjFoliage*)subObj.pFoliage)->m_pRenderObject = pRenderObject;
	}

	if (subObj.bIdentityMatrix)
	{
		if ((pRenderObject->m_ObjFlags & FOB_DYNAMIC_OBJECT) && (pRenderObject->m_ObjFlags & FOB_NEAREST))
		{
			SRenderObjData* pRenderObjectData = pRenderObject->GetObjData();
			pRenderObjectData->m_uniqueObjectId = pRenderObjectData->m_uniqueObjectId + nSubObjId;
		}
	}
	else
	{
		pRenderObject = GetRenderer()->EF_DuplicateRO(pRenderObject, passInfo);
		pRenderObject->SetMatrix(renderTM * subObj.tm, passInfo);

		SRenderObjData* pRenderObjectData = pRenderObject->GetObjData();
		pRenderObjectData->m_uniqueObjectId = pRenderObjectData->m_uniqueObjectId + nSubObjId;
	}

	pStatObj->RenderSubObjectInternal(pRenderObject, nLod, passInfo);
}

///////////////////////////////////////////////////////////////////////////////
void CStatObj::RenderSubObjectInternal(CRenderObject* pRenderObject, int nLod, const SRenderingPassInfo& passInfo)
{
	assert(!(m_nFlags & STATIC_OBJECT_HIDDEN));
	assert(m_nRenderTrisCount);

	m_nLastDrawMainFrameId = passInfo.GetMainFrameID();
	if (m_pParentObject && (m_nFlags & STATIC_OBJECT_MULTIPLE_PARENTS))
		m_pParentObject->m_nLastDrawMainFrameId = passInfo.GetMainFrameID();

	assert(!m_pParentObject || m_pParentObject->m_nLastDrawMainFrameId == passInfo.GetMainFrameID());

	assert(!(m_nFlags & STATIC_OBJECT_COMPOUND));

	nLod = CLAMP(nLod, GetMinUsableLod(), (int)m_nMaxUsableLod);
	CRY_ASSERT(nLod >= 0 && nLod < MAX_STATOBJ_LODS_NUM);

	// Skip rendering of this suboject if it is marked as deformable
	if (GetCVars()->e_MergedMeshes == 1 && nLod == 0 && m_isDeformable)
		return;

	// try next lod's if selected one is not ready
	if ((!nLod && m_pRenderMesh && m_pRenderMesh->CanUpdate()) || !GetCVars()->e_Lods)
	{
		PrefetchLine(pRenderObject, 0);
		RenderRenderMesh(pRenderObject, NULL, passInfo);
	}
	else
	{
		if (m_pLODs && m_pLODs[nLod])
		{
			m_pLODs[nLod]->m_nLastDrawMainFrameId = passInfo.GetMainFrameID();
			if (m_pLODs[nLod]->m_pParentObject)
				m_pLODs[nLod]->m_pParentObject->m_nLastDrawMainFrameId = passInfo.GetMainFrameID();

			if ((nLod + 1) < MAX_STATOBJ_LODS_NUM && m_pLODs[nLod + 1])
			{
				m_pLODs[nLod + 1]->m_nLastDrawMainFrameId = passInfo.GetMainFrameID();
				if (m_pLODs[nLod + 1]->m_pParentObject)
					m_pLODs[nLod + 1]->m_pParentObject->m_nLastDrawMainFrameId = passInfo.GetMainFrameID();
			}
		}

		if (m_pLODs)
			for (; nLod <= (int)m_nMaxUsableLod; nLod++)
			{
				if (m_pLODs[nLod] && m_pLODs[nLod]->m_pRenderMesh && m_pLODs[nLod]->m_pRenderMesh->CanUpdate())
				{
					PrefetchLine(pRenderObject, 0);
					m_pLODs[nLod]->RenderRenderMesh(pRenderObject, NULL, passInfo);
					break;
				}
			}
	}
}

///////////////////////////////////////////////////////////////////////////////
void CStatObj::RenderObjectInternal(CRenderObject* pRenderObject, int nTargetLod, uint8 uLodDissolveRef, bool dissolveOut, const SRenderingPassInfo& passInfo)
{
	if (nTargetLod == -1 || uLodDissolveRef == 255)
	{
		return;
	}

	int nLod = CLAMP(nTargetLod, GetMinUsableLod(), (int)m_nMaxUsableLod);
	CRY_ASSERT(nLod >= 0 && nLod < MAX_STATOBJ_LODS_NUM);

	// Skip rendering of this suboject if it is marked as deformable
	if (GetCVars()->e_MergedMeshes == 1 && nTargetLod == 0 && m_isDeformable)
		return;

	if (passInfo.IsShadowPass() && passInfo.GetShadowMapType() == SRenderingPassInfo::SHADOW_MAP_CACHED && pRenderObject->m_pRenderNode)
	{
		CRY_ASSERT(passInfo.ShadowCacheLod() < MAX_GSM_CACHED_LODS_NUM);
		IShadowCaster* pCaster = static_cast<IShadowCaster*>(pRenderObject->m_pRenderNode);
		pCaster->m_shadowCacheLod[passInfo.ShadowCacheLod()] = nLod;
	}

	pRenderObject->m_DissolveRef = uLodDissolveRef;

	if (pRenderObject->m_DissolveRef)
	{
		if (!(pRenderObject->m_ObjFlags & FOB_DISSOLVE))
			pRenderObject->m_ObjFlags &= ~FOB_UPDATED_RTMASK;
		pRenderObject->m_ObjFlags |= FOB_DISSOLVE;

		if (dissolveOut)
			pRenderObject->m_ObjFlags |= FOB_DISSOLVE_OUT;
	}
	else
	{
		if ((pRenderObject->m_ObjFlags & FOB_DISSOLVE))
			pRenderObject->m_ObjFlags &= ~FOB_UPDATED_RTMASK;
		pRenderObject->m_ObjFlags &= ~FOB_DISSOLVE;
	}

	// try next lod's if selected one is not ready
	if ((!nLod && m_pRenderMesh && m_pRenderMesh->CanUpdate()) || !GetCVars()->e_Lods)
	{
		PrefetchLine(pRenderObject, 0);
		RenderRenderMesh(pRenderObject, NULL, passInfo);
	}
	else
	{
		if (m_pLODs && m_pLODs[nLod])
		{
			m_pLODs[nLod]->m_nLastDrawMainFrameId = passInfo.GetMainFrameID();
			if (m_pLODs[nLod]->m_pParentObject)
				m_pLODs[nLod]->m_pParentObject->m_nLastDrawMainFrameId = passInfo.GetMainFrameID();

			if ((nLod + 1) < MAX_STATOBJ_LODS_NUM && m_pLODs[nLod + 1])
			{
				m_pLODs[nLod + 1]->m_nLastDrawMainFrameId = passInfo.GetMainFrameID();
				if (m_pLODs[nLod + 1]->m_pParentObject)
					m_pLODs[nLod + 1]->m_pParentObject->m_nLastDrawMainFrameId = passInfo.GetMainFrameID();
			}
		}

		if (m_pLODs)
			for (; nLod <= (int)m_nMaxUsableLod; nLod++)
			{
				if (m_pLODs[nLod] && m_pLODs[nLod]->m_pRenderMesh && m_pLODs[nLod]->m_pRenderMesh->CanUpdate())
				{
					PrefetchLine(pRenderObject, 0);
					m_pLODs[nLod]->RenderRenderMesh(pRenderObject, NULL, passInfo);
					break;
				}
			}
	}
}

///////////////////////////////////////////////////////////////////////////////
void CStatObj::RenderRenderMesh(CRenderObject* pRenderObject, SInstancingInfo* pInstInfo, const SRenderingPassInfo& passInfo)
{
#if !defined(_RELEASE)
	//DEBUG - filter which stat objs are rendered
	if (GetCVars()->e_statObjRenderFilterMode && GetCVars()->e_pStatObjRenderFilterStr && GetCVars()->e_pStatObjRenderFilterStr[0])
	{
		if (GetCVars()->e_statObjRenderFilterMode == 1)
		{
			//only render elems containing str
			if (!strstr(m_szFileName.c_str(), GetCVars()->e_pStatObjRenderFilterStr))
			{
				return;
			}
		}
		else if (GetCVars()->e_statObjRenderFilterMode == 2)
		{
			//exclude elems containing str
			if (strstr(m_szFileName.c_str(), GetCVars()->e_pStatObjRenderFilterStr))
			{
				return;
			}
		}
	}

	if (GetCVars()->e_DebugDraw && (!GetCVars()->e_DebugDrawShowOnlyCompound || (m_bSubObject || m_pParentObject)))
	{
		int nLod = 0;
		if (m_pLod0 && m_pLod0->m_pLODs)
			for (; nLod < MAX_STATOBJ_LODS_NUM; nLod++)
			{
				if (m_pLod0->m_pLODs[nLod] == this)
				{
					m_pRenderMesh->SetMeshLod(nLod);
					break;
				}
			}

		if (GetCVars()->e_DebugDrawShowOnlyLod >= 0)
			if (GetCVars()->e_DebugDrawShowOnlyLod != nLod)
				return;

		if (RenderDebugInfo(pRenderObject, passInfo))
			return;

		if (m_bSubObject)
			pRenderObject = gEnv->pRenderer->EF_DuplicateRO(pRenderObject, passInfo);
	}

	if (!passInfo.IsShadowPass())
	{
		if (GetCVars()->e_StreamCgfDebug == 1)
		{
			RenderStreamingDebugInfo(pRenderObject, passInfo);
		}

		if (GetCVars()->e_CoverCgfDebug == 1)
		{
			RenderCoverInfo(pRenderObject, passInfo);
		}
	}
#endif

	if (m_pRenderMesh)
	{
		CRenderObject* pObj = pRenderObject;
#if !defined(_RELEASE)
		if (m_isProxyTooBig)
		{
			pObj = GetRenderer()->EF_DuplicateRO(pRenderObject, passInfo);
			pObj->m_pCurrMaterial = m_pMaterial;
		}
#endif
		m_pRenderMesh->Render(pObj, passInfo);
	}
}

///////////////////////////////////////////////////////////////////////////////
int CStatObj::GetMaxUsableLod()
{
	auto lodMax = GetCVars()->e_LodMax;
	lodMax = CLAMP(lodMax, 0, MAX_STATOBJ_LODS_NUM - 1);

	int maxUsable = m_pLod0 ? max((int)m_nMaxUsableLod, (int)m_pLod0->m_nMaxUsableLod) : (int)m_nMaxUsableLod;
	return min(maxUsable, lodMax);
}

///////////////////////////////////////////////////////////////////////////////
int CStatObj::GetMinUsableLod()
{
	auto lodMin = GetCVars()->e_LodMin;
	lodMin = CLAMP(lodMin, 0, MAX_STATOBJ_LODS_NUM - 1);

	int minUsable = m_pLod0 ? max((int)m_nMinUsableLod0, (int)m_pLod0->m_nMinUsableLod0) : (int)m_nMinUsableLod0;
	return max(minUsable, lodMin);
}

///////////////////////////////////////////////////////////////////////////////
int CStatObj::FindNearestLoadedLOD(int nLodIn, bool bSearchUp)
{
	// make sure requested lod is loaded
	/*  if(CStatObj * pObjForStreamIn = nLodIn ? m_pLODs[nLodIn] : this)
	{
	bool bRenderNodeValid(rParams.pRenderNode && ((int)(void*)(rParams.pRenderNode)>0) && rParams.pRenderNode->m_fWSMaxViewDist);
	float fImportance = bRenderNodeValid ? (1.f - (rParams.fDistance / rParams.pRenderNode->m_fWSMaxViewDist)) : 0.5f;
	pObjForStreamIn->UpdateStreamingPrioriryInternal(fImportance);
	}*/

	// if requested lod is not ready - find nearest ready one
	int nLod = nLodIn;

	if (nLod == 0 && (!GetRenderMesh() || !GetRenderMesh()->CanUpdate()))
		nLod++;

	while (nLod && nLod < MAX_STATOBJ_LODS_NUM && (!m_pLODs || !m_pLODs[nLod] || !m_pLODs[nLod]->GetRenderMesh() || !m_pLODs[nLod]->GetRenderMesh()->CanUpdate()))
		nLod++;

	if (nLod >(int)m_nMaxUsableLod)
	{
		if (bSearchUp)
		{
			nLod = min((int)m_nMaxUsableLod, nLodIn);

			while (nLod && (!m_pLODs || !m_pLODs[nLod] || !m_pLODs[nLod]->GetRenderMesh() || !m_pLODs[nLod]->GetRenderMesh()->CanUpdate()))
				nLod--;

			if (nLod == 0 && (!GetRenderMesh() || !GetRenderMesh()->CanUpdate()))
				nLod--;
		}
		else
		{
			nLod = -1;
		}
	}

	return nLod;
}

//////////////////////////////////////////////////////////////////////////
int CStatObj::AddRef()
{
	return CryInterlockedIncrement(&m_nUsers);
}
