// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:Renderer.cpp
//  Description: Abstract renderer API
//
//	History:
//	-Feb 05,2001:Originally Created by Marco Corbetta
//	-: taken over by Andrey Honich
//.
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "Shadow_Renderer.h"
#include <Cry3DEngine/IStatObj.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryMovie/IMovieSystem.h>
#include <Cry3DEngine/IIndexedMesh.h>
#include <CryCore/BitFiddling.h>                              // IntegerLog2()
#include <Cry3DEngine/ImageExtensionHelper.h>                 // CImageExtensionHelper
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>      // CResourceCompilerHelper

#include "Textures/Image/CImage.h"
#include "Textures/TextureManager.h"
#include <CryRenderer/branchmask.h>
#include "PostProcess/PostEffects.h"
#include "RendElements/CRELensOptics.h"

#include "RendElements/OpticsFactory.h"
#include "IntroMovieRenderer.h"

#include "XRenderD3D9/CompiledRenderObject.h"
#include "RenderView.h"

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
struct SCompareByShadowFrustumID
{
	bool operator()(const SRendItem& rA, const SRendItem& rB) const
	{
		return rA.rendItemSorter.ShadowFrustumID() < rB.rendItemSorter.ShadowFrustumID();
	}
};

///////////////////////////////////////////////////////////////////////////////
// Sort priority: light id, shadow map type (Dynamic, DynamicDistance, Cached), shadow map lod
struct SCompareByLightIds
{
	bool operator()(const SShadowFrustumToRender& rA, const SShadowFrustumToRender& rB) const
	{
		if (rA.nLightID != rB.nLightID)
		{
			return rA.nLightID < rB.nLightID;
		}
		else
		{
			if (rA.pFrustum->m_eFrustumType != rB.pFrustum->m_eFrustumType)
			{
				return int(rA.pFrustum->m_eFrustumType) < int(rB.pFrustum->m_eFrustumType);
			}
			else
			{
				return rA.pFrustum->nShadowMapLod < rB.pFrustum->nShadowMapLod;
			}
		}
	}
};

///////////////////////////////////////////////////////////////////////////////
static inline void AddEf_HandleOldRTMask(CRenderObject* obj)
{
	const uint64 objFlags = obj->m_ObjFlags;
	obj->m_nRTMask = 0;
	if (objFlags & (FOB_NEAREST | FOB_DECAL_TEXGEN_2D | FOB_DISSOLVE | FOB_SOFT_PARTICLE))
	{
		if (objFlags & FOB_DECAL_TEXGEN_2D)
			obj->m_nRTMask |= g_HWSR_MaskBit[HWSR_DECAL_TEXGEN_2D];

		if (objFlags & FOB_NEAREST)
			obj->m_nRTMask |= g_HWSR_MaskBit[HWSR_NEAREST];

		if (objFlags & FOB_DISSOLVE)
			obj->m_nRTMask |= g_HWSR_MaskBit[HWSR_DISSOLVE];

		if (CRenderer::CV_r_ParticlesSoftIsec && (objFlags & FOB_SOFT_PARTICLE))
			obj->m_nRTMask |= g_HWSR_MaskBit[HWSR_SOFT_PARTICLE];
	}

	obj->m_ObjFlags |= FOB_UPDATED_RTMASK;
}

///////////////////////////////////////////////////////////////////////////////
static inline void AddEf_HandleForceFlags(int& nList, int& nAW, uint32& nBatchFlags, const uint32 nShaderFlags, const uint32 nShaderFlags2, CRenderObject* obj)
{
	// Force rendering in last place
	// FIXME: If object is permanent this is wrong!
	// branchless

	const int32 sort1 = nz2mask(nShaderFlags2 & EF2_FORCE_DRAWLAST);
	const int32 sort2 = nz2one(nShaderFlags2 & EF2_FORCE_DRAWFIRST);
	float fSort = static_cast<float>(100000 * (sort1 + sort2));

	if (nShaderFlags2 & EF2_FORCE_ZPASS && !((nShaderFlags & EF_REFRACTIVE) && (nBatchFlags & FB_MULTILAYERS)))
		nBatchFlags |= FB_Z;

	{
		// below branchlessw version of:
		//if      (nShaderFlags2 & EF2_FORCE_TRANSPASS  ) nList = EFSLIST_TRANSP;
		//else if (nShaderFlags2 & EF2_FORCE_GENERALPASS) nList = EFSLIST_GENERAL;
		//else if (nShaderFlags2 & EF2_FORCE_WATERPASS  ) nList = EFSLIST_WATER;

		uint32 mb1 = nShaderFlags2 & EF2_FORCE_TRANSPASS;
		uint32 mb2 = nShaderFlags2 & EF2_FORCE_GENERALPASS;
		uint32 mb3 = nShaderFlags2 & EF2_FORCE_WATERPASS;

		mb1 = nz2msb(mb1);
		mb2 = nz2msb(mb2) & ~mb1;
		mb3 = nz2msb(mb3) & ~(mb1 ^ mb2);

		mb1 = msb2mask(mb1);
		mb2 = msb2mask(mb2);
		mb3 = msb2mask(mb3);

		const uint32 mask = mb1 | mb2 | mb3;
		mb1 &= EFSLIST_TRANSP;
		mb2 &= EFSLIST_GENERAL;
		mb3 &= EFSLIST_WATER;

		nList = iselmask(mask, mb1 | mb2 | mb3, nList);
	}

	// if (nShaderFlags2 & EF2_AFTERHDRPOSTPROCESS) // now it's branchless
	{
		uint32 predicate = nz2mask(nShaderFlags2 & EF2_AFTERHDRPOSTPROCESS);

		const uint32 mask = nz2mask(nShaderFlags2 & EF2_FORCE_DRAWLAST);
		nList = iselmask(predicate, iselmask(mask, EFSLIST_AFTER_POSTPROCESS, EFSLIST_AFTER_HDRPOSTPROCESS), nList);
	}

	if (nShaderFlags2 & EF2_AFTERPOSTPROCESS)
		nList = EFSLIST_AFTER_POSTPROCESS;

	//if (nShaderFlags2 & EF2_FORCE_DRAWAFTERWATER) nAW = 1;   -> branchless
	nAW |= nz2one(nShaderFlags2 & EF2_FORCE_DRAWAFTERWATER);

	obj->m_fSort += fSort;
}

///////////////////////////////////////////////////////////////////////////////
void CRenderer::EF_AddEf_NotVirtual(CRenderElement* re, SShaderItem& SH, CRenderObject* obj, const SRenderingPassInfo& passInfo, int nList, int nAW)
{
	assert(nList > 0 && nList < EFSLIST_NUM);
	if (!re || !SH.m_pShader)
		return;

	// shader item is not set up yet
	if (SH.m_nPreprocessFlags == -1)
	{
		if (obj->m_bPermanent && obj->m_pRenderNode)
			obj->m_pRenderNode->InvalidatePermanentRenderObject();

		return;
	}

	CShader* const __restrict pSH = (CShader*)SH.m_pShader;
	const uint32 nShaderFlags = pSH->m_Flags;
	if (nShaderFlags & EF_NODRAW)
		return;

	if (passInfo.IsShadowPass())
	{
		if (pSH->m_HWTechniques.Num() && pSH->m_HWTechniques[0]->m_nTechnique[TTYPE_SHADOWGEN] >= 0)
		{
			passInfo.GetRenderView()->AddRenderItem(re, obj, SH, passInfo.ShadowFrustumSide(), FB_GENERAL, passInfo.GetRendItemSorter(), true, false);
		}
		return;
	}

	const uint32 nMaterialLayers = obj->m_nMaterialLayers;

	CShaderResources* const __restrict pShaderResources = (CShaderResources*)SH.m_pShaderResources;

	// Need to differentiate between something rendered with cloak layer material, and sorted with cloak.
	// e.g. ironsight glows on gun should be sorted with cloak to not write depth - can be inconsistent with no depth from gun.
	const uint32 nCloakRenderedMask = mask_nz_zr(nMaterialLayers & MTL_LAYER_BLEND_CLOAK, pShaderResources ? static_cast<CShaderResources*>(pShaderResources)->CShaderResources::GetMtlLayerNoDrawFlags() & MTL_LAYER_CLOAK : 0);
	uint32 nCloakLayerMask = nz2mask(nMaterialLayers & MTL_LAYER_BLEND_CLOAK);

	// Discard 0 alpha blended geometry - this should be discarded earlier on 3dengine side preferably
	if (!obj->m_fAlpha)
		return;
	if (pShaderResources && pShaderResources->::CShaderResources::IsInvisible())
		return;

#ifdef _DEBUG
	static float sMatIdent[12] =
	{
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0
	};

	if (memcmp(sMatIdent, obj->m_II.m_Matrix.GetData(), 3 * 4 * 4))
	{
		if (!(obj->m_ObjFlags & FOB_TRANS_MASK))
		{
			assert(0);
		}
	}
#endif

	if (!(obj->m_ObjFlags & FOB_UPDATED_RTMASK))
	{
		AddEf_HandleOldRTMask(obj);
	}

	uint32 nBatchFlags = EF_BatchFlags(SH, obj, re, passInfo, nAW);

	if (obj->m_ObjFlags & FOB_DECAL)
	{
		// Send all decals to decals list
		nList = EFSLIST_DECAL;
	}

	const uint32 nRenderlistsFlags = (FB_PREPROCESS | FB_MULTILAYERS | FB_TRANSPARENT);
	if (nBatchFlags & nRenderlistsFlags || nCloakLayerMask)
	{
		// branchless version of:
		//if      (pSH->m_Flags & FB_REFRACTIVE || nCloakLayerMask)           nList = EFSLIST_TRANSP, nBatchFlags &= ~FB_Z;
		//else if((nBatchFlags & FB_TRANSPARENT) && nList == EFSLIST_GENERAL) nList = EFSLIST_TRANSP;

		// Refractive objects go into same list as transparent objects - partial resolves support
		// arbitrary ordering between transparent and refractive correctly.

		uint32 mx1 = (nShaderFlags & EF_REFRACTIVE) | nCloakLayerMask;
		uint32 mx2 = mask_nz_zr(nBatchFlags & FB_TRANSPARENT, (nList ^ EFSLIST_GENERAL) | mx1);

		nBatchFlags &= iselmask(mx1 = nz2mask(mx1), ~FB_Z, nBatchFlags);
		nList = iselmask(mx1 | mx2, (mx1 & EFSLIST_TRANSP) | (mx2 & EFSLIST_TRANSP), nList);
	}

	// FogVolume contribution for transparencies isn't needed when volumetric fog is turned on.
	if ((((nBatchFlags & FB_TRANSPARENT) || (pSH->GetFlags2() & EF2_HAIR)) && !m_bVolumetricFogEnabled)
	    || passInfo.IsRecursivePass() /* account for recursive scene traversal done in forward fashion*/)
	{
		SRenderObjData* pOD = obj->GetObjData();
		if (pOD && pOD->m_FogVolumeContribIdx == (uint16) - 1)
		{
			I3DEngine* pEng = gEnv->p3DEngine;
			ColorF newContrib;
			pEng->TraceFogVolumes(obj->GetTranslation(), newContrib, passInfo);

			pOD->m_FogVolumeContribIdx = CRenderer::PushFogVolumeContribution(newContrib, passInfo);
		}
	}
	//if (nList != EFSLIST_GENERAL && nList != EFSLIST_TERRAINLAYER) nBatchFlags &= ~FB_Z;
	nBatchFlags &= ~(FB_Z & mask_nz_nz(nList ^ EFSLIST_GENERAL, nList ^ EFSLIST_TERRAINLAYER));

	nList = (nBatchFlags & FB_SKIN) ? EFSLIST_SKIN : nList;
	nList = (nBatchFlags & FB_EYE_OVERLAY) ? EFSLIST_EYE_OVERLAY : nList;

	const EShaderDrawType shaderDrawType = pSH->m_eSHDType;
	const uint32 nShaderFlags2 = pSH->m_Flags2;
	const uint64 ObjDecalFlag = obj->m_ObjFlags & FOB_DECAL;

	// make sure decals go into proper render list
	// also, set additional shadow flag (i.e. reuse the shadow mask generated for underlying geometry)
	// TODO: specify correct render list and additional flags directly in the engine once non-material decal rendering support is removed!
	if ((ObjDecalFlag || (nShaderFlags & EF_DECAL)))
	{
		// BK: Drop decals that are refractive (and cloaked!). They look bad if forced into refractive pass,
		// and break if they're in the decal pass
		if (nShaderFlags & (EF_REFRACTIVE | EF_FORCEREFRACTIONUPDATE) || nCloakRenderedMask)
			return;

		//SShaderTechnique *pTech = SH.GetTechnique();
		//if (pTech && pTech->m_nTechnique[TTYPE_Z] > 0 && ((nShaderFlags2 & EF2_FORCE_ZPASS) || CV_r_deferredshading)) // deferred shading always enabled
		if (nShaderFlags & EF_SUPPORTSDEFERREDSHADING_FULL)
		{
			nBatchFlags |= FB_Z;
		}

		nList = EFSLIST_DECAL;
		obj->m_ObjFlags |= FOB_INSHADOW;

		if (ObjDecalFlag == 0 && pShaderResources)
			obj->m_nSort = pShaderResources->m_SortPrio;
	}

	// Enable tessellation for water geometry
	obj->m_ObjFlags |= (pSH->m_Flags2 & EF2_HW_TESSELLATION && pSH->m_eShaderType == eST_Water) ? FOB_ALLOW_TESSELLATION : 0;

	const uint32 nForceFlags = (EF2_FORCE_DRAWLAST | EF2_FORCE_DRAWFIRST | EF2_FORCE_ZPASS | EF2_FORCE_TRANSPASS | EF2_FORCE_GENERALPASS | EF2_FORCE_DRAWAFTERWATER | EF2_FORCE_WATERPASS | EF2_AFTERHDRPOSTPROCESS | EF2_AFTERPOSTPROCESS);

	if (nShaderFlags2 & nForceFlags)
	{
		AddEf_HandleForceFlags(nList, nAW, nBatchFlags, nShaderFlags, nShaderFlags2, obj);
	}


	// Always force cloaked geometry to render after water
	//if (obj->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK) nAW = 1;   -> branchless
	nAW |= nz2one(obj->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK);

	if (nShaderFlags & (EF_REFRACTIVE | EF_FORCEREFRACTIONUPDATE) || nCloakRenderedMask)
	{
		SRenderObjData* pOD = obj->GetObjData();

		if (obj->m_pRenderNode && pOD)
		{
			const int32 align16 = (16 - 1);
			const int32 shift16 = 4;
			if (CRenderer::CV_r_RefractionPartialResolves)
			{
				AABB aabb;
				IRenderNode* pRenderNode = obj->m_pRenderNode;
				pRenderNode->FillBBox(aabb);

				int iOut[4];

				passInfo.GetCamera().CalcScreenBounds(&iOut[0], &aabb, CRenderer::GetWidth(), CRenderer::GetHeight());
				pOD->m_screenBounds[0] = min(iOut[0] >> shift16, 255);
				pOD->m_screenBounds[1] = min(iOut[1] >> shift16, 255);
				pOD->m_screenBounds[2] = min((iOut[2] + align16) >> shift16, 255);
				pOD->m_screenBounds[3] = min((iOut[3] + align16) >> shift16, 255);

#if REFRACTION_PARTIAL_RESOLVE_DEBUG_VIEWS
				if (CRenderer::CV_r_RefractionPartialResolvesDebug == eRPR_DEBUG_VIEW_3D_BOUNDS)
				{
					// Debug bounding box view for refraction partial resolves
					IRenderAuxGeom* pAuxRenderer = gEnv->pRenderer->GetIRenderAuxGeom();
					if (pAuxRenderer)
					{
						SAuxGeomRenderFlags oldRenderFlags = pAuxRenderer->GetRenderFlags();

						SAuxGeomRenderFlags newRenderFlags;
						newRenderFlags.SetDepthTestFlag(e_DepthTestOff);
						newRenderFlags.SetAlphaBlendMode(e_AlphaBlended);
						pAuxRenderer->SetRenderFlags(newRenderFlags);

						const bool bSolid = true;
						const ColorB solidColor(64, 64, 255, 64);
						pAuxRenderer->DrawAABB(aabb, bSolid, solidColor, eBBD_Faceted);

						const ColorB wireframeColor(255, 0, 0, 255);
						pAuxRenderer->DrawAABB(aabb, !bSolid, wireframeColor, eBBD_Faceted);

						// Set previous Aux render flags back again
						pAuxRenderer->SetRenderFlags(oldRenderFlags);
					}
				}
#endif
			}
			else if (nShaderFlags & EF_FORCEREFRACTIONUPDATE)
			{
				pOD->m_screenBounds[0] = 0;
				pOD->m_screenBounds[1] = 0;
				pOD->m_screenBounds[2] = min((CRenderer::GetWidth()) >> shift16, 255);
				pOD->m_screenBounds[3] = min((CRenderer::GetHeight()) >> shift16, 255);
			}
		}
	}

	// final step, for post 3d items, remove them from any other list than POST_3D_RENDER
	// (have to do this here as the batch needed to go through the normal nList assign path first)
	nBatchFlags = iselmask(nz2mask(nBatchFlags & FB_POST_3D_RENDER), FB_POST_3D_RENDER, nBatchFlags);

	// No need to sort opaque passes by water/after water. Ensure always on same list for more coherent sorting
	nAW |= nz2one((nList == EFSLIST_GENERAL) | (nList == EFSLIST_TERRAINLAYER) | (nList == EFSLIST_DECAL));

#ifndef _RELEASE
	nList = (shaderDrawType == eSHDT_DebugHelper) ? EFSLIST_DEBUG_HELPER : nList;
#endif

	passInfo.GetRenderView()->AddRenderItem(re, obj, SH, nList, nBatchFlags, passInfo.GetRendItemSorter(), false, passInfo.IsAuxWindow());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void CRenderer::EF_AddEf(CRenderElement* re, SShaderItem& pSH, CRenderObject* obj, const SRenderingPassInfo& passInfo, int nList, int nAW)
{
	EF_AddEf_NotVirtual(re, pSH, obj, passInfo, nList, nAW);
}

//////////////////////////////////////////////////////////////////////////
uint16 CRenderer::PushFogVolumeContribution(const ColorF& fogVolumeContrib, const SRenderingPassInfo& passInfo)
{
	int nThreadID = passInfo.ThreadID();

	const size_t maxElems((1 << (sizeof(uint16) * 8)) - 1);
	size_t numElems(m_RP.m_fogVolumeContibutions[nThreadID].size());
	assert(numElems < maxElems);
	if (numElems >= maxElems)
		return (uint16) - 1;

	size_t nIndex = ~0;
	m_RP.m_fogVolumeContibutions[nThreadID].push_back(fogVolumeContrib, nIndex);
	assert(nIndex <= (uint16) - 1); // Beware! Casting from uint32 to uint16 may loose top bits
	return static_cast<uint16>(nIndex);
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::GetFogVolumeContribution(uint16 idx, ColorF& rColor) const
{
	int nThreadID = m_RP.m_nProcessThreadID;
	if (idx >= m_RP.m_fogVolumeContibutions[nThreadID].size())
	{
		rColor = ColorF(0.0f, 0.0f, 0.0f, 1.0f);
	}
	else
	{
		rColor = m_RP.m_fogVolumeContibutions[nThreadID][idx];
	}
}

//////////////////////////////////////////////////////////////////////////
uint32 CRenderer::EF_BatchFlags(SShaderItem& SH, CRenderObject* pObj, CRenderElement* re, const SRenderingPassInfo& passInfo, int nAboveWater)
{

	uint32 nFlags = SH.m_nPreprocessFlags & FB_MASK;
	if (SH.m_nPreprocessFlags & FSPR_GENSPRITES)
		nFlags |= FB_PREPROCESS;
	SShaderTechnique* const __restrict pTech = SH.GetTechnique();
	CShaderResources* const __restrict pR = (CShaderResources*)SH.m_pShaderResources;
	CShader* const __restrict pS = (CShader*)SH.m_pShader;

	float fAlpha = pObj->m_fAlpha;
	uint32 uTransparent = 0; //(bool)(fAlpha < 1.0f); Not supported in new rendering pipeline 
	const uint64 ObjFlags = pObj->m_ObjFlags;

	if (!passInfo.IsRecursivePass() && pTech)
	{
		CryPrefetch(pTech->m_nTechnique);
		CryPrefetch(pR);

		//if (pObj->m_fAlpha < 1.0f) nFlags |= FB_TRANSPARENT;
		nFlags |= FB_TRANSPARENT * uTransparent;

		if (!((nFlags & FB_Z) && (!(pObj->m_RState & OS_NODEPTH_WRITE) || (pS->m_Flags2 & EF2_FORCE_ZPASS))))
			nFlags &= ~FB_Z;

		if ((ObjFlags & FOB_DISSOLVE) || (ObjFlags & FOB_DECAL) || CRenderer::CV_r_usezpass != 2 || pObj->m_fDistance > CRenderer::CV_r_ZPrepassMaxDist)
			nFlags &= ~FB_ZPREPASS;

		pObj->m_ObjFlags |= (nFlags & FB_ZPREPASS) ? FOB_ZPREPASS : 0;

		if (pTech->m_nTechnique[TTYPE_DEBUG] > 0 && 0 != (ObjFlags & FOB_SELECTED))
			nFlags |= FB_DEBUG;

		const uint32 nMaterialLayers = pObj->m_nMaterialLayers;
		const uint32 DecalFlags = pS->m_Flags & EF_DECAL;

		if (passInfo.IsShadowPass())
			nFlags &= ~FB_PREPROCESS;

		nFlags &= ~(FB_PREPROCESS & uTransparent);

		if ((nMaterialLayers & ~uTransparent) && CV_r_usemateriallayers)
		{
			const uint32 nResourcesNoDrawFlags = static_cast<CShaderResources*>(pR)->CShaderResources::GetMtlLayerNoDrawFlags();

			// if ((nMaterialLayers & MTL_LAYER_BLEND_DYNAMICFROZEN) && !(nResourcesNoDrawFlags & MTL_LAYER_FROZEN))
			uint32 uMask = mask_nz_zr(nMaterialLayers & MTL_LAYER_BLEND_DYNAMICFROZEN, nResourcesNoDrawFlags & MTL_LAYER_FROZEN);

			nFlags |= FB_MULTILAYERS & uMask;

			//if ((nMaterialLayers & MTL_LAYER_BLEND_CLOAK) && !(nResourcesNoDrawFlags&MTL_LAYER_CLOAK))
			uMask = mask_nz_zr(nMaterialLayers & MTL_LAYER_BLEND_CLOAK, nResourcesNoDrawFlags & MTL_LAYER_CLOAK);

			//prevent general pass when fully cloaked
			nFlags &= ~(uMask & (FB_TRANSPARENT | (((nMaterialLayers & MTL_LAYER_BLEND_CLOAK) == MTL_LAYER_BLEND_CLOAK) ? FB_GENERAL : 0)));
			nFlags |= uMask & (FB_TRANSPARENT | FB_MULTILAYERS);
		}

		//if ( ((ObjFlags & (FOB_DECAL)) | DecalFlags) == 0 ) // put the mask below
		{
			if (pTech->m_nTechnique[TTYPE_MOTIONBLURPASS] > 0 && (ObjFlags & FOB_HAS_PREVMATRIX) && CV_r_MotionVectors)
			{
				uint32 uMask = mask_zr_zr(ObjFlags & (FOB_DECAL), DecalFlags);
				nFlags |= FB_MOTIONBLUR & uMask;
			}
		}

		// apply motion blur to skinned vegetation when it moves (for example breaking trees)
		if (pTech->m_nTechnique[TTYPE_MOTIONBLURPASS] > 0 && ((ObjFlags & FOB_SKINNED) != 0) && ((ObjFlags & FOB_HAS_PREVMATRIX) != 0) && CV_r_MotionVectors)
		{
			nFlags |= FB_MOTIONBLUR;
		}

		SRenderObjData* pOD = pObj->GetObjData();
		if (pTech->m_nTechnique[TTYPE_CUSTOMRENDERPASS] > 0)
		{
			const uint32 nVisionParams = (pOD && pOD->m_nVisionParams);
			if (m_nThermalVisionMode && (pR->HeatAmount_unnormalized() || nVisionParams))
				nFlags |= FB_CUSTOM_RENDER;

			const uint32 customvisions = CRenderer::CV_r_customvisions;
			const uint32 nHUDSilhouettesParams = (pOD && pOD->m_nHUDSilhouetteParams);
			if (customvisions && nHUDSilhouettesParams)
			{
				nFlags |= FB_CUSTOM_RENDER;
			}
		}

		if (pOD->m_nCustomFlags & COB_POST_3D_RENDER)
		{
			nFlags |= FB_POST_3D_RENDER;
		}

		if (nFlags & FB_LAYER_EFFECT)
		{
			if ((!pOD->m_pLayerEffectParams) && !CV_r_DebugLayerEffect)
				nFlags &= ~FB_LAYER_EFFECT;
		}

		if (pR && pR->IsAlphaTested())
			pObj->m_ObjFlags |= FOB_ALPHATEST;
	}
	else if (passInfo.IsRecursivePass() && pTech && m_RP.m_TI[passInfo.ThreadID()].m_PersFlags & RBPF_MIRRORCAMERA)
	{
		nFlags &= (FB_TRANSPARENT | FB_GENERAL);
		nFlags |= FB_TRANSPARENT * uTransparent;                                      // if (pObj->m_fAlpha < 1.0f)                   nFlags |= FB_TRANSPARENT;
	}

	{
		//if ( (objFlags & FOB_ONLY_Z_PASS) || CV_r_ZPassOnly) && !(nFlags & (FB_TRANSPARENT))) - put it to the mask
		const uint32 mask = mask_nz_zr((uint32)CV_r_ZPassOnly, nFlags & (FB_TRANSPARENT));

		nFlags = iselmask(mask, FB_Z, nFlags);
	}

	nFlags |= nAboveWater ? 0 : FB_BELOW_WATER;

	// Cloak also requires resolve
	const uint32 nCloakMask = mask_nz_zr(pObj->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK, (pR ? static_cast<CShaderResources*>(pR)->CShaderResources::GetMtlLayerNoDrawFlags() & MTL_LAYER_CLOAK : 0));

	int nShaderFlags = (SH.m_pShader ? SH.m_pShader->GetFlags() : 0);
	if ((CRenderer::CV_r_RefractionPartialResolves && nShaderFlags & EF_REFRACTIVE) || (nShaderFlags & EF_FORCEREFRACTIONUPDATE) || nCloakMask)
		pObj->m_ObjFlags |= FOB_REQUIRES_RESOLVE;

	return nFlags;
}

///////////////////////////////////////////////////////////////////////////////
CRenderObject* CRenderer::EF_GetObject_Temp(int nThreadID)
{
	CryAutoCriticalSection lock(m_renderObjectAccessLock);

	CThreadSafeWorkerContainer<CRenderObject*>& Objs = m_RP.m_TempObjects[nThreadID];

	size_t nId = ~0;
	CRenderObject** ppObj = Objs.push_back_new(nId);
	CRenderObject* pObj = NULL;

	if (*ppObj == NULL)
	{
		*ppObj = new CRenderObject;
	}
	pObj = *ppObj;

	pObj->AssignId(nId);
	pObj->Init();
	pObj->m_pCompiledObject = nullptr;

	return pObj;
}

///////////////////////////////////////////////////////////////////////////////
CRenderObject* CRenderer::EF_GetObject()
{
	CRenderObject* pObj = CPermanentRenderObject::AllocateFromPool();
	return pObj;
}

///////////////////////////////////////////////////////////////////////////////
void CRenderer::EF_FreeObject(CRenderObject* pObj)
{
	assert(pObj && pObj->m_bPermanent);

	m_RP.m_persistentRenderObjectsToDelete[m_RP.m_nFillThreadID].push_back(reinterpret_cast<CPermanentRenderObject*>(pObj));
}

///////////////////////////////////////////////////////////////////////////////
CRenderObject* CRenderer::EF_DuplicateRO(CRenderObject* pSrc, const SRenderingPassInfo& passInfo)
{
	if (pSrc->m_bPermanent)
	{
		// Clone object and attach to the end of linked list of the source object

		CPermanentRenderObject* pObjNew = CPermanentRenderObject::AllocateFromPool();

		uint32 nId = pObjNew->m_Id;
		pObjNew->CloneObject(pSrc);
		pObjNew->m_Id = nId;
		pObjNew->m_pCompiledObject = nullptr; // Must not be cloned.

		CPermanentRenderObject* pObjSrc = reinterpret_cast<CPermanentRenderObject*>(pSrc);

		// Link duplicated object to the source object
		{
			// Prevent multi-threaded object cloning corrupting linked list.
			WriteLock lock(pObjSrc->m_accessLock);
			pObjNew->m_pNextPermanent = pObjSrc->m_pNextPermanent;
			pObjSrc->m_pNextPermanent = pObjNew;
			;
		}

		return pObjNew;
	}

	CRenderObject* pObjNew = CRenderer::EF_GetObject_Temp(passInfo.ThreadID());
	pObjNew->CloneObject(pSrc);
	return pObjNew;
}

//////////////////////////////////////////////////////////////////////////
// IShaderPublicParams implementation class.
//////////////////////////////////////////////////////////////////////////

struct CShaderPublicParams : public IShaderPublicParams
{
	CShaderPublicParams()
	{
		m_nRefCount = 0;
	}

	virtual void          AddRef()                  { m_nRefCount++; };
	virtual void          Release()                 { if (--m_nRefCount <= 0) delete this; };

	virtual void          SetParamCount(int nParam) { m_shaderParams.resize(nParam); }
	virtual int           GetParamCount() const     { return m_shaderParams.size(); };

	virtual SShaderParam& GetParam(int nIndex)
	{
		assert(nIndex >= 0 && nIndex < (int)m_shaderParams.size());
		return m_shaderParams[nIndex];
	}

	virtual const SShaderParam& GetParam(int nIndex) const
	{
		assert(nIndex >= 0 && nIndex < (int)m_shaderParams.size());
		return m_shaderParams[nIndex];
	}

	virtual SShaderParam* GetParamByName(const char* pszName)
	{
		for (int32 i = 0; i < m_shaderParams.size(); i++)
		{
			if (!stricmp(pszName, m_shaderParams[i].m_Name))
			{
				return &m_shaderParams[i];
			}
		}

		return 0;
	}

	virtual const SShaderParam* GetParamByName(const char* pszName) const
	{
		for (int32 i = 0; i < m_shaderParams.size(); i++)
		{
			if (!stricmp(pszName, m_shaderParams[i].m_Name))
			{
				return &m_shaderParams[i];
			}
		}

		return 0;
	}

	virtual SShaderParam* GetParamBySemantic(uint8 eParamSemantic)
	{
		for (int i = 0; i < m_shaderParams.size(); ++i)
		{
			if (m_shaderParams[i].m_eSemantic == eParamSemantic)
			{
				return &m_shaderParams[i];
			}
		}

		return NULL;
	}

	virtual const SShaderParam* GetParamBySemantic(uint8 eParamSemantic) const
	{
		for (int i = 0; i < m_shaderParams.size(); ++i)
		{
			if (m_shaderParams[i].m_eSemantic == eParamSemantic)
			{
				return &m_shaderParams[i];
			}
		}

		return NULL;
	}

	virtual void SetParam(int nIndex, const SShaderParam& param)
	{
		assert(nIndex >= 0 && nIndex < (int)m_shaderParams.size());

		m_shaderParams[nIndex] = param;
	}

	virtual void AddParam(const SShaderParam& param)
	{
		// shouldn't add existing parameter ?
		m_shaderParams.push_back(param);
	}

	virtual void RemoveParamByName(const char* pszName)
	{
		for (int32 i = 0; i < m_shaderParams.size(); i++)
		{
			if (!stricmp(pszName, m_shaderParams[i].m_Name))
			{
				m_shaderParams.erase(i);
			}
		}
	}

	virtual void RemoveParamBySemantic(uint8 eParamSemantic)
	{
		for (int i = 0; i < m_shaderParams.size(); ++i)
		{
			if (eParamSemantic == m_shaderParams[i].m_eSemantic)
			{
				m_shaderParams.erase(i);
			}
		}
	}

	virtual void SetParam(const char* pszName, UParamVal& pParam, EParamType nType = eType_FLOAT, uint8 eSemantic = 0)
	{
		int32 i;

		for (i = 0; i < m_shaderParams.size(); i++)
		{
			if (!stricmp(pszName, m_shaderParams[i].m_Name))
			{
				break;
			}
		}

		if (i == m_shaderParams.size())
		{
			SShaderParam pr;
			cry_strcpy(pr.m_Name, pszName);
			pr.m_Type = nType;
			pr.m_eSemantic = eSemantic;
			m_shaderParams.push_back(pr);
		}

		SShaderParam::SetParam(pszName, &m_shaderParams, pParam);
	}

	virtual void SetShaderParams(const DynArray<SShaderParam>& pParams)
	{
		m_shaderParams = pParams;
	}

	virtual void AssignToRenderParams(struct SRendParams& rParams)
	{
		if (!m_shaderParams.empty())
			rParams.pShaderParams = &m_shaderParams;
	}

	virtual DynArray<SShaderParam>* GetShaderParams()
	{
		if (m_shaderParams.empty())
		{
			return 0;
		}

		return &m_shaderParams;
	}

	virtual const DynArray<SShaderParam>* GetShaderParams() const
	{
		if (m_shaderParams.empty())
		{
			return 0;
		}

		return &m_shaderParams;
	}

	virtual uint8 GetSemanticByName(const char* szName)
	{
		static_assert(ECGP_COUNT <= 0xff, "8 bits are not enough to store all ECGParam values");

		if (strcmp(szName, "WrinkleMask0") == 0)
		{
			return ECGP_PI_WrinklesMask0;
		}
		if (strcmp(szName, "WrinkleMask1") == 0)
		{
			return ECGP_PI_WrinklesMask1;
		}
		if (strcmp(szName, "WrinkleMask2") == 0)
		{
			return ECGP_PI_WrinklesMask2;
		}

		return ECGP_Unknown;
	}

private:
	int                    m_nRefCount;
	DynArray<SShaderParam> m_shaderParams;
};

//////////////////////////////////////////////////////////////////////////
IShaderPublicParams* CRenderer::CreateShaderPublicParams()
{
	return new CShaderPublicParams;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CMotionBlur::SetupObject(CRenderObject* pObj, const SRenderingPassInfo& passInfo)
{
	assert(pObj);

	uint32 nThreadID = passInfo.ThreadID();

	if (passInfo.IsRecursivePass())
		return;

	SRenderObjData* const __restrict pOD = pObj->GetObjData();
	if (!pOD)
	{
		return;
	}

	pObj->m_ObjFlags &= ~FOB_HAS_PREVMATRIX;

	// don't apply regular object motion blur to skinned objects with bending (foliage)
	// they get their motion blur in the DrawSkinned Pass
	if (pOD->m_pSkinningData && pOD->m_pSkinningData->pAsyncJobs == NULL)
		return;

	if (pOD->m_uniqueObjectId != 0 && pObj->m_fDistance < CRenderer::CV_r_MotionBlurMaxViewDist)
	{
		const uint32 nFrameID = passInfo.GetMainFrameID();
		const uintptr_t ObjID = pOD ? pOD->m_uniqueObjectId : 0;
		const uint32 nObjFrameWriteID = (nFrameID) % 3;
		OMBParamsMapItor it = m_pOMBData[nObjFrameWriteID].find(ObjID);
		if (it != m_pOMBData[nObjFrameWriteID].end())
		{
			// if all good, get previous buffered frame
			const uint32 nObjPrevFrameID = (nFrameID - 1) % 3;
			OMBParamsMapItor itPrev = m_pOMBData[nObjPrevFrameID].find(ObjID);

			if (itPrev != m_pOMBData[nObjPrevFrameID].end())
			{
				SObjMotionBlurParams* pWriteObjMBData = &it->second;
				SObjMotionBlurParams* pPrevObjMBData = &itPrev->second;
				pWriteObjMBData->mObjToWorld = pObj->m_II.m_Matrix;

				const float fThreshold = CRenderer::CV_r_MotionBlurThreshold;
				if (pObj->m_ObjFlags & (FOB_NEAREST | FOB_MOTION_BLUR) || !Matrix34::IsEquivalent(pPrevObjMBData->mObjToWorld, pWriteObjMBData->mObjToWorld, fThreshold))
					pObj->m_ObjFlags |= FOB_HAS_PREVMATRIX;

				pWriteObjMBData->nFrameUpdateID = nFrameID;
				pWriteObjMBData->pRenderObj = pObj;

				return;
			}
		}

		m_FillData[nThreadID].push_back(OMBParamsMap::value_type(ObjID, SObjMotionBlurParams(pObj, pObj->m_II.m_Matrix, nFrameID)));
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void SRendItem::mfSortPreprocess(SRendItem* First, int Num)
{
	std::sort(First, First + Num, SCompareItemPreprocess());
}

//////////////////////////////////////////////////////////////////////////
void SRendItem::mfSortForZPass(SRendItem* First, int Num)
{
	std::sort(First, First + Num, SCompareRendItemZPass());
}

//////////////////////////////////////////////////////////////////////////
void SRendItem::mfSortByLight(SRendItem* First, int Num, bool bSort, const bool bIgnoreRePtr, bool bSortDecals)
{
	if (bSort)
	{
		if (bIgnoreRePtr)
			std::sort(First, First + Num, SCompareItem_NoPtrCompare());
		else
		{
			if (bSortDecals)
				std::sort(First, First + Num, SCompareItem_Decal());
			else
				std::sort(First, First + Num, SCompareRendItem());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void SRendItem::mfSortByDist(SRendItem* First, int Num, bool bDecals, bool InvertedOrder)
{
	//Note: Temporary use stable sort for flickering hair (meshes within the same skin attachment don't have a deterministic sort order)
	CRenderer* r = gRenDev;
	int i;
	if (!bDecals)
	{
		//Pre-pass to bring in the first 8 entries. 8 cache requests can be in flight
		const int iPrefetchLoopLastIndex = min_branchless(8, Num);
		for (i = 0; i < iPrefetchLoopLastIndex; i++)
		{
			//It's safe to prefetch NULL
			PrefetchLine(First[i].pObj, offsetof(CRenderObject, m_fSort));
		}

		const int iLastValidIndex = Num - 1;

		//Note: this seems like quite a bit of work to do some prefetching but this code was generating a
		//			level 2 cache miss per iteration of the loop - Rich S
		for (i = 0; i < Num; i++)
		{
			SRendItem* pRI = &First[i];
			int iPrefetchIndex = min_branchless(i + 8, iLastValidIndex);
			PrefetchLine(First[iPrefetchIndex].pObj, offsetof(CRenderObject, m_fSort));
			CRenderObject* pObj = pRI->pObj; // no need to flush, data is only read
			assert(pObj);

			// We're prefetching on m_fSort, we're still getting some L2 cache misses on access to m_fDistance,
			// but moving them closer in memory is complicated due to an aligned array that's nestled in there...
			float fAddDist = pObj->m_fSort;
			pRI->fDist = pObj->m_fDistance + fAddDist;
		}

		if (InvertedOrder)
			std::stable_sort(First, First + Num, SCompareDistInverted());
		else
			std::stable_sort(First, First + Num, SCompareDist());
	}
	else
	{
		std::stable_sort(First, First + Num, SCompareItem_Decal());
	}
}

int16 CTexture::StreamCalculateMipsSignedFP(float fMipFactor) const
{
	assert(IsStreamed());
	const uint32 nMaxExtent = max(m_nWidth, m_nHeight);
	float currentMipFactor = fMipFactor * nMaxExtent * nMaxExtent * gRenDev->GetMipDistFactor();
	float fMip = (0.5f * logf(max(currentMipFactor, 0.1f)) / gf_ln2 + (CRenderer::CV_r_TexturesStreamingMipBias + gRenDev->m_fTexturesStreamingGlobalMipFactor));
	int nMip = static_cast<int>(floorf(fMip * 256.0f));
	const int nNewMip = min(nMip, (m_nMips - m_CacheFileHeader.m_nMipsPersistent) << 8);
	return (int16)nNewMip;
}

float CTexture::StreamCalculateMipFactor(int16 nMipsSigned) const
{
	float fMip = nMipsSigned / 256.0f;
	float currentMipFactor = expf((fMip - (CRenderer::CV_r_TexturesStreamingMipBias + gRenDev->m_fTexturesStreamingGlobalMipFactor)) * 2.0f * gf_ln2);

	const uint32 nMaxExtent = max(m_nWidth, m_nHeight);
	float fMipFactor = currentMipFactor / (nMaxExtent * nMaxExtent * gRenDev->GetMipDistFactor());

	return fMipFactor;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
