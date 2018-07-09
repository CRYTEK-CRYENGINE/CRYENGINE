// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Includes
#include "StdAfx.h"
#include <CryRenderer/RenderElements/CREBreakableGlass.h>
#include "D3DREBreakableGlassBuffer.h"

#include "../Common/RendElements/Utils/PolygonMath2D.h"
#include "../Common/RendElements/Utils/SpatialHashGrid.h"

#include "DriverD3D.h"
#include <Cry3DEngine/IRenderNode.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryRenderer/IRenderAuxGeom.h>

// Error logging
#ifndef RELEASE
	#define LOG_GLASS_ERROR(str) CryLogAlways("[BreakGlassSystem Error]: %s", str)
#else
	#define LOG_GLASS_ERROR(str)
#endif

// OLD PIPELINE
/*
//--------------------------------------------------------------------------------------------------
// Name: mfDraw
// Desc: Draws breakable glass sim
//--------------------------------------------------------------------------------------------------
bool CREBreakableGlass::mfDraw(CShader* ef, SShaderPass* sfm)
{
#ifdef GLASS_DEBUG_MODE
	// Early out if disabled
	if (s_pCVars && s_pCVars->m_draw <= 0)
	{
		return true;
	}
#endif

	// Assume rendering core geometry
	uint32* pBufferId = &m_geomBufferId;
	int subFragIndex = -1;
	bool isFrag = false;

	// Loose fragments require a sub-buffer id
	CRenderer* pRenDev = gRenDev;
	CRenderObject* pRenderObject = pRenDev->m_RP.m_pCurObject;

	if (pRenderObject && pRenderObject->m_breakableGlassSubFragIndex != GLASSCFG_GLASS_PLANE_FLAG_LOD)
	{
		subFragIndex = pRenderObject->m_breakableGlassSubFragIndex;
		isFrag = true;

		if (subFragIndex < GLASSCFG_MAX_NUM_PHYS_FRAGMENTS)
		{
			pBufferId = &m_fragGeomBufferIds[subFragIndex].m_geomBufferId;
		}
		else
		{
			// Invalid id, so invalidate buffer
			pBufferId = NULL;
		}
	}

	// If not done yet, grab a new buffer from the manager
	CREBreakableGlassBuffer& bufferMan = CREBreakableGlassBuffer::RT_Instance();

	if (pBufferId && *pBufferId == CREBreakableGlassBuffer::NoBuffer)
	{
		*pBufferId = bufferMan.RT_AcquireBuffer(isFrag);
	}

	// Only continue if valid buffer obtained
	const CREBreakableGlassBuffer::EBufferType bufferType = isFrag ?
	                                                        CREBreakableGlassBuffer::EBufferType_Frag :
	                                                        CREBreakableGlassBuffer::EBufferType_Plane;

	if (pBufferId && bufferMan.RT_IsBufferValid(*pBufferId, bufferType))
	{
		// Apply any geometry updates on render calls after their associated update
		const uint32 drawFrame = pRenDev->GetFrameID(false);
		const uint32 updateFrame = m_geomUpdateFrame;
		if (drawFrame >= updateFrame)
		{
			RT_UpdateBuffers(subFragIndex);
		}

#ifdef GLASS_DEBUG_MODE
		// Allow forced regeneration of decals for real-time tweaking
		if (s_pCVars && s_pCVars->m_decalAlwaysRebuild > 0)
		{
			m_numDecalPSConsts = 0;
			UpdateImpactShaderConstants();
		}
#endif

		// Set runtime flags
		uint64 oldRTFlags = pRenDev->m_RP.m_FlagsShader_RT;
		pRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
		pRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE1];
		pRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE2];
		pRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE3];
		pRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE4];
		pRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE5];

		// Begin drawing
		if (CShader* pShader = ef)
		{
			uint32 passCount = 0;
			pShader->FXBegin(&passCount, 0);

			if (passCount > 0)
			{
				// Begin shader pass
				pShader->FXBeginPass(0);

				// Update shading state/constants
				gEnv->pRenderer->SetCullMode(R_CULL_NONE);
				SetImpactShaderConstants(pShader);

				// NULL/Valid material decides which part of glass is being rendered
				if (!pRenderObject->m_pCurrMaterial && !isFrag)
				{
					// Draw outline/crack geometry
					bufferMan.RT_DrawBuffer(*pBufferId, CREBreakableGlassBuffer::EBufferType_Crack);
				}
				else
				{
					// Draw stable/plane and fragment geometry
					bufferMan.RT_DrawBuffer(*pBufferId, bufferType);
				}

				// End shader pass
				pShader->FXEndPass();
			}

			// End drawing
			pShader->FXEnd();
		}

		// Revert runtime flags
		pRenDev->m_RP.m_FlagsShader_RT = oldRTFlags;

#ifdef GLASS_DEBUG_MODE
		// Only draw debug info with one of the passes
		if (pRenderObject
		    && !pRenderObject->m_pCurrMaterial
		    && pRenderObject->m_breakableGlassSubFragIndex == GLASSCFG_GLASS_PLANE_FLAG_LOD)
		{
			const bool drawWireframe = (s_pCVars && s_pCVars->m_drawWireframe > 0);
			const bool drawDebugData = (s_pCVars && s_pCVars->m_drawDebugData > 0);

			DebugDraw(drawWireframe, drawDebugData);
		}
#endif
	}
	else if (!isFrag && m_geomBufferId != CREBreakableGlassBuffer::NoBuffer)
	{
		// If we couldn't render the core geometry, if means our
		// buffer has been lost/recycled to another node
		m_geomBufferLost = true;
	}

	return true;
}//-------------------------------------------------------------------------------------------------
*/

//--------------------------------------------------------------------------------------------------
// Name: RT_UpdateBuffers
// Desc: Geometry update on render thread
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::RT_UpdateBuffers(const int subFragIndex)
{
	ASSERT_IS_RENDER_THREAD(gRenDev->m_pRT);

	const bool isFrag = (subFragIndex >= 0 && subFragIndex < GLASSCFG_MAX_NUM_PHYS_FRAGMENTS);
	const bool geomRebuilt = isFrag ? m_fragGeomRebuilt[subFragIndex] : m_geomRebuilt;

	if (geomRebuilt && m_dirtyGeomBufferCount > 0)
	{
		CREBreakableGlassBuffer& bufferMan = CREBreakableGlassBuffer::RT_Instance();

		// Extract data for main geom or loose fragment
		const uint32 bufferId = isFrag ? m_fragGeomBufferIds[subFragIndex].m_geomBufferId : m_geomBufferId;
		SGlassGeom& planeGeom = isFrag ? m_fragFullGeom[subFragIndex] : m_planeGeom;
		SGlassGeom& crackGeom = isFrag ? m_fragFullGeom[subFragIndex] : m_crackGeom;

		CREBreakableGlassBuffer::EBufferType bufferType = isFrag ?
		                                                  CREBreakableGlassBuffer::EBufferType_Frag :
		                                                  CREBreakableGlassBuffer::EBufferType_Plane;

		// Transfer vertex data to buffers
		if (planeGeom.m_verts.Count() >= 3 && planeGeom.m_inds.Count() >= 3 &&
		    crackGeom.m_verts.Count() >= 3 && crackGeom.m_inds.Count() >= 3)
		{
			if (planeGeom.m_verts.Count() >= GLASSCFG_MAX_NUM_PLANE_VERTS ||
			    crackGeom.m_verts.Count() >= GLASSCFG_MAX_NUM_PLANE_VERTS)
			{
				LOG_GLASS_ERROR("Vertex buffers too large, need to increase fixed sizes.");
			}

			bufferMan.RT_UpdateVertexBuffer(bufferId, bufferType, &planeGeom.m_verts[0], (uint32)planeGeom.m_verts.Count());
			bufferMan.RT_UpdateIndexBuffer(bufferId, bufferType, &planeGeom.m_inds[0], (uint32)planeGeom.m_inds.Count());
			bufferMan.RT_UpdateTangentBuffer(bufferId, bufferType, &planeGeom.m_tans[0], (uint32)planeGeom.m_tans.Count());

			// Fragments use a single combined buffer
			if (!isFrag)
			{
				bufferType = CREBreakableGlassBuffer::EBufferType_Crack;

				bufferMan.RT_UpdateVertexBuffer(bufferId, bufferType, &crackGeom.m_verts[0], (uint32)crackGeom.m_verts.Count());
				bufferMan.RT_UpdateIndexBuffer(bufferId, bufferType, &crackGeom.m_inds[0], (uint32)crackGeom.m_inds.Count());
				bufferMan.RT_UpdateTangentBuffer(bufferId, bufferType, &crackGeom.m_tans[0], (uint32)crackGeom.m_tans.Count());
			}
		}
		else
		{
			bufferMan.RT_ClearBuffer(bufferId, bufferType);
		}

		// Update our shader constants if there's been new impacts
		if (m_numDecalImpacts > m_numDecalPSConsts)
		{
			UpdateImpactShaderConstants();
			m_numDecalPSConsts = m_numDecalImpacts;
		}

		// Free un-needed data
		MemoryBarrier();
		planeGeom.Free();
		crackGeom.Free();

		// Done
		if (isFrag)
		{
			m_fragGeomRebuilt[subFragIndex] = false;
		}
		else
		{
			m_geomRebuilt = false;
		}

		--m_dirtyGeomBufferCount;
		MemoryBarrier();
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: UpdateImpactShaderConstants
// Desc: Sets shader constants for the current impact points
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::UpdateImpactShaderConstants()
{
	// Tweakable parameters
	float minImpactSpeed = GLASSCFG_MIN_BULLET_SPEED;
	float minImpactRadius = 0.25f;
	float maxImpactRadius = 1.25f;
	float randDecalChance = 0.75f;
	float randDecalScale = 3.6f;
	float impactScale = 2.15f;

	if (s_pCVars)
	{
		minImpactSpeed = s_pCVars->m_minImpactSpeed;
		minImpactRadius = s_pCVars->m_decalMinRadius;
		maxImpactRadius = s_pCVars->m_decalMaxRadius;
		randDecalChance = s_pCVars->m_decalRandChance;
		randDecalScale = s_pCVars->m_decalRandScale;
		impactScale = max(s_pCVars->m_decalScale, 0.01f);
	}

	// Calculate (approx) unit decal sizes
	float invImpactScale = __fres(impactScale);
	Vec2 invUnitDecalScale = m_invUVRange * invImpactScale;

	// Hard-coded decal atlas offsets
	// - Must be kept in sync with "%ENGINE%\EngineAssets\Textures\glass_decalatlas_*.tif"
	const Vec4 impactAtlasOffset(0.5f, 1.0f, 0.0f, 0.0f);
	const Vec4 bulletAtlasOffset(0.5f, 1.0f, 0.5f, 0.0f);

	// Force the same seed for consistent results
	SeedRand();

	// Calculate decal data for new impacts
	while (m_numDecalPSConsts < m_numDecalImpacts && m_numDecalImpacts <= GLASSCFG_MAX_NUM_PHYS_FRAGMENTS)
	{
		// Get impact params for this decal
		// - Data never changed or moved outside of debug testing, so should be safe to read only
		const uint index = m_numDecalPSConsts;
		const SGlassImpactParams& impact = m_impactParams[index];

		// Determine what type of decal we have
		const bool isBullet = (impact.speed >= minImpactSpeed);
		const bool isLargeDecal = (isBullet && GetRandF(index) >= randDecalChance);

		// Calculate decal diameter
		float invDecalScale = GetImpactRadius(impact) * impactScale;
		invDecalScale = clamp_tpl<float>(invDecalScale, minImpactRadius, maxImpactRadius);

		invDecalScale = min(invDecalScale, maxImpactRadius);
		invDecalScale *= isLargeDecal ? randDecalScale : 1.0f;

		invDecalScale = __fres(invDecalScale);

		// Calculate decal center UV coord
		Vec2 uv = m_glassParams.uvOrigin;
		uv += m_glassParams.uvXAxis * impact.x;
		uv += m_glassParams.uvYAxis * impact.y;

		// Decal: xy = uv-space position, zw = inverse scale
		PREFAST_ASSUME(index < GLASSCFG_MAX_NUM_IMPACT_DECALS);
		m_decalPSConsts[index].decal.x = uv.x;
		m_decalPSConsts[index].decal.y = uv.y;
		m_decalPSConsts[index].decal.z = invUnitDecalScale.x * invDecalScale;
		m_decalPSConsts[index].decal.w = invUnitDecalScale.y * invDecalScale;

		// Atlas: xy = scaling, zw = position offset
		if (isBullet && !isLargeDecal)
		{
			m_decalPSConsts[index].atlas = bulletAtlasOffset;
		}
		else
		{
			m_decalPSConsts[index].atlas = impactAtlasOffset;
		}

		// Next decal
		++m_numDecalPSConsts;
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: SetImpactShaderConstants
// Desc: Sets shader constants for the current impact points
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::SetImpactShaderConstants(CShader* pShader)
{
	const uint numDecalElems = 2;
//	pShader->FXSetPSFloat(s_ImpactDecalParamName, (Vec4*)m_decalPSConsts, GLASSCFG_MAX_NUM_IMPACT_DECALS * numDecalElems);
}//-------------------------------------------------------------------------------------------------

#ifdef GLASS_DEBUG_MODE
//--------------------------------------------------------------------------------------------------
// Name: DebugDraw
// Desc: Draws debug information
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::DebugDraw(const bool wireframe, const bool data)
{
	// Prepare aux geom renderer
	IRenderAuxGeom* pRenderer = gEnv->pRenderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags oldFlags = pRenderer->GetRenderFlags();

	SAuxGeomRenderFlags newFlags = e_Def3DPublicRenderflags;
	newFlags.SetCullMode(e_CullModeNone);
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetDepthWriteFlag(e_DepthWriteOff);
	newFlags.SetDepthTestFlag(e_DepthTestOff);
	pRenderer->SetRenderFlags(newFlags);

	// Draw mesh wireframe outline
	if (wireframe)
	{
		DrawFragmentDebug(pRenderer, newFlags);

		// Draw fragment bit states just underneath plane
		//      Vec3 fragCenter(0.0f, -0.1f, -0.05f);
		//      fragCenter = m_params.matrix.TransformPoint(fragCenter);
		//
		//      char activeBin[72];
		//      char looseBin[72];
		//      char freeBin[72];
		//      uint32 fragBit = 1;
		//
		//      for (uint i = 0; i < GLASSCFG_FRAGMENT_ARRAY_SIZE; ++i, fragBit <<= 1)
		//      {
		//        activeBin[i] = (m_fragsActive & fragBit) ? '1' : '0';
		//        looseBin[i] = (m_fragsLoose & fragBit) ? '1' : '0';
		//        freeBin[i] = (m_fragsFree & fragBit) ? '1' : '0';
		//
		//        // Add some separators every 8 bits for readability
		//        if ((i & 7) == 7)
		//        {
		//          ++i;
		//          activeBin[i] = '|';
		//          looseBin[i] = '|';
		//          freeBin[i] = '|';
		//        }
		//      }
		//
		//      activeBin[GLASSCFG_FRAGMENT_ARRAY_SIZE] = '\0';
		//      looseBin[GLASSCFG_FRAGMENT_ARRAY_SIZE] = '\0';
		//      freeBin[GLASSCFG_FRAGMENT_ARRAY_SIZE] = '\0';
		//
		//      const float fragFontCol[4] = {1.0f, 1.0f, 0.5f, 1.0f};
		//      IRenderAuxText::DrawLabelExF(fragCenter, 1.2f, fragFontCol, false, true, "A:%s\nL:%s\nF:%s", activeBin, looseBin, freeBin);
	}

	// Draw shatter data
	if (data)
	{
		newFlags.SetFillMode(e_FillModeSolid);
		pRenderer->SetRenderFlags(newFlags);

		DrawFragmentConnectionDebug(pRenderer, newFlags);
		//DrawDebugData(pRenderer, newFlags);

		// Draw all impact points
		if (!m_impactLineList.IsEmpty())
		{
			pRenderer->SetRenderFlags(newFlags);

			CRY_ASSERT(m_impactLineList.Count() % 2 == 0);// Invalid number of verts
			pRenderer->DrawLines(&m_impactLineList[0], m_impactLineList.Count(), ColorB(127, 255, 127, 255), 1.5f);
		}

		// Draw uv data at fragment center
		//      const float fragFontCol[4] = {1.0f, 0.0f, 1.0f, 1.0f};
		//      Vec4* pBasis = m_glassParams.uvBasis;
		//
		//      Vec3 center = m_params.matrix.TransformPoint(Vec3(pBasis[0].x, pBasis[0].y, 0.0f));
		//      IRenderAuxText::DrawLabelExF(center, 1.2f, fragFontCol, false, true, "[0]\n%.3f,%.3f", pBasis[0].z, pBasis[0].w);
		//
		//      center = m_params.matrix.TransformPoint(Vec3(pBasis[1].x, pBasis[1].y, 0.0f));
		//      IRenderAuxText::DrawLabelExF(center, 1.2f, fragFontCol, false, true, "[1]\n%.3f,%.3f", pBasis[1].z, pBasis[1].w);
		//
		//      center = m_params.matrix.TransformPoint(Vec3(pBasis[2].x, pBasis[2].y, 0.0f));
		//      IRenderAuxText::DrawLabelExF(center, 1.2f, fragFontCol, false, true, "[2]\n%.3f,%.3f", pBasis[2].z, pBasis[2].w);

		// Hash grid data overlay
		//      if (m_pHashGrid)
		//      {
		//        m_pHashGrid->DebugDraw();
		//      }

		// Draw axes
		//      newFlags.SetDepthTestFlag(e_DepthTestOff);
		//      pRenderer->SetRenderFlags(newFlags);
		//
		//      const Matrix34& transMat = m_params.matrix;
		//      const Vec3 pos = transMat.GetTranslation() + transMat.TransformVector(Vec3(m_glassParams.size.x * 0.5f, m_glassParams.size.y * 0.5f, m_glassParams.thickness * 0.5f));
		//      pRenderer->DrawLine(pos, ColorB(255, 64, 64, 255), pos + transMat.GetColumn0(), ColorB(255, 64, 64, 255), 2.0f);
		//      pRenderer->DrawLine(pos, ColorB(64, 255, 64, 255), pos + transMat.GetColumn1(), ColorB(64, 255, 64, 255), 2.0f);
		//      pRenderer->DrawLine(pos, ColorB(64, 64, 255, 255), pos + transMat.GetColumn2(), ColorB(64, 64, 255, 255), 2.0f);
		//
		//      const float red[4] = {1.0f, 0.25f, 0.25f, 1.0f};
		//      const float grn[4] = {0.25f, 1.0f, 0.25f, 1.0f};
		//      const float blu[4] = {0.25f, 0.25f, 1.0f, 1.0f};
		//
		//      IRenderAuxText::DrawLabelEx(pos + transMat.GetColumn0() * 1.1f, 0.5f, red, false, true, "X");
		//      IRenderAuxText::DrawLabelEx(pos + transMat.GetColumn1() * 1.1f, 0.5f, grn, false, true, "Y");
		//      IRenderAuxText::DrawLabelEx(pos + transMat.GetColumn2() * 1.1f, 0.5f, blu, false, true, "Z");
	}

	// Reset states
	pRenderer->SetRenderFlags(oldFlags);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: DrawFragmentDebug
// Desc: Draws active fragment data
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::DrawFragmentDebug(IRenderAuxGeom* const pRenderer, SAuxGeomRenderFlags& flags)
{
	PodArray<Vec3> temp;
	const float fragFontCol[4] = { 1.0f, 0.0f, 1.0f, 1.0f };
	const float triFontCol[4] = { 0.0f, 1.0f, 1.0f, 1.0f };

	// Pre-evaluate state bits
	const uint32 invalidState = ~m_fragsActive | m_fragsLoose;
	uint32 fragBit = 1;

	// Create outlines
	const uint numFrags = m_frags.size();

	for (uint i = 0; i < numFrags; ++i, fragBit <<= 1)
	{
		if (invalidState & fragBit)
			continue;

		// Fragment outline
		const SGlassFragment& frag = m_frags[i];

		for (int j = 0; j < frag.m_outlinePts.Count(); ++j)
		{
			const int next = (j + 1) % frag.m_outlinePts.Count();

			temp.push_back(frag.m_outlinePts[j]);
			temp.push_back(frag.m_outlinePts[next]);
		}

		// Triangle outlines
		//    for (int j = 0; j < frag.m_triInds.Count(); j+=3)
		//    {
		//      temp.push_back(frag.m_outlinePts[frag.m_triInds[0+j]]);
		//      temp.push_back(frag.m_outlinePts[frag.m_triInds[1+j]]);
		//
		//      temp.push_back(frag.m_outlinePts[frag.m_triInds[1+j]]);
		//      temp.push_back(frag.m_outlinePts[frag.m_triInds[2+j]]);
		//
		//      temp.push_back(frag.m_outlinePts[frag.m_triInds[2+j]]);
		//      temp.push_back(frag.m_outlinePts[frag.m_triInds[0+j]]);
		//
		//      // Draw id at triangle center
		// //       Vec3 triCenter = frag.m_outlinePts[frag.m_triPts[0+j]] + frag.m_outlinePts[frag.m_triPts[1+j]] + frag.m_outlinePts[frag.m_triPts[2+j]];
		// //       triCenter = triCenter * (1.0f / 3.0f);
		// //       triCenter.z = -0.01f;
		// //       triCenter = m_params.matrix.TransformPoint(triCenter);
		// //
		// //       IRenderAuxText::DrawLabelExF(triCenter, 0.5f, triFontCol, false, true, "%i", j);
		//    }

		// Draw id at fragment center
		//    Vec3 fragCenter = Vec3(frag.m_center.x, frag.m_center.y, -0.01f);
		//    fragCenter = m_params.matrix.TransformPoint(fragCenter);
		//
		//    IRenderAuxText::DrawLabelExF(fragCenter, 0.8f, fragFontCol, false, true, "%i", i);
	}

	// Transform and draw
	if (!temp.IsEmpty())
	{
		for (int i = 0; i < temp.Count(); ++i)
		{
			temp[i].z = -0.005f;
		}

		TransformPointList(temp);
		pRenderer->DrawLines(&temp[0], temp.Count(), ColorB(255, 127, 64, 255), 2.0f);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: DrawFragmentDebug
// Desc: Draws specified fragment
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::DrawFragmentDebug(const uint fragIndex, const Matrix34& matrix, const uint8 buffId, const float alpha)
{
	PodArray<Vec3> temp;
	const byte uAlpha = (byte)(clamp_tpl<float>(alpha, 0.0f, 1.0f) * 255.0f);

	// Create outline
	if (fragIndex < (uint)m_frags.size())
	{
		const SGlassFragment& frag = m_frags[fragIndex];

		for (int j = 0; j < frag.m_outlinePts.Count(); ++j)
		{
			const int next = (j + 1) % frag.m_outlinePts.Count();

			temp.push_back(frag.m_outlinePts[j]);
			temp.push_back(frag.m_outlinePts[next]);
		}

		// Draw fragment phys size at center
		Vec3 fragCenter = Vec3(frag.m_center.x, frag.m_center.y, 0.0f);
		fragCenter = matrix.TransformPoint(fragCenter);

		const Vec2 bounds = CalculatePolygonBounds2D(frag.m_outlinePts.begin(), frag.m_outlinePts.Count());
		const float physFragSize = max(bounds.x, bounds.y) * 0.7f;

		const float fragFontCol[4] = { 1.0f, 0.0f, 1.0f, 1.0f };
		IRenderAuxText::DrawLabelExF(fragCenter, 0.5f, fragFontCol, false, true, "B:%i, F:%i", buffId, fragIndex);
	}

	if (!temp.IsEmpty())
	{
		// Transform
		const int numPts = temp.Count();
		Vec3* pPts = temp.begin();

		for (int i = 0; i < numPts; ++i)
		{
			pPts[i] = matrix.TransformPoint(temp[i]);
		}

		// Draw
		IRenderAuxGeom* pRenderer = gEnv->pRenderer->GetIRenderAuxGeom();
		SAuxGeomRenderFlags oldFlags = pRenderer->GetRenderFlags();

		SAuxGeomRenderFlags newFlags = e_Def3DPublicRenderflags;
		newFlags.SetCullMode(e_CullModeNone);
		newFlags.SetDepthWriteFlag(e_DepthWriteOff);
		newFlags.SetAlphaBlendMode(e_AlphaBlended);
		newFlags.SetDepthTestFlag(e_DepthTestOff);
		pRenderer->SetRenderFlags(newFlags);

		pRenderer->DrawLines(&temp[0], temp.Count(), ColorB(255, 255, 64, uAlpha), 2.0f);

		pRenderer->SetRenderFlags(oldFlags);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: DrawFragmentConnectionDebug
// Desc: Draws active fragment connection data with per-fragment colouring
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::DrawFragmentConnectionDebug(IRenderAuxGeom* const pRenderer, SAuxGeomRenderFlags& flags)
{
	PodArray<Vec3> anchorLines, connLinesA, connLinesB;

	// Generate all anchor connection lines
	PodArray<int> anchorFrags, stableFrags;
	const uint8 numAnchors = m_glassParams.numAnchors;

	// Find all anchored fragments
	for (int i = 0; i < numAnchors; ++i)
	{
		SGlassImpactParams anchorParams;
		anchorParams.x = m_glassParams.anchors[i].x;
		anchorParams.y = m_glassParams.anchors[i].y;

		uint8 anchorFrag = 0;
		if (FindImpactFragment(anchorParams, anchorFrag))
		{
			// Add line from anchor to center
			anchorLines.push_back(m_glassParams.anchors[i]);
			anchorLines.push_back(m_frags[anchorFrag].m_center);
		}

		// Create circle for each anchor point
		const Vec3 center = m_glassParams.anchors[i];
		const float radius = 0.06f;
		const uint32 numSides = 8;

		Vec3 a(Vec3Constants<float>::fVec3_Zero);
		Vec3 b(Vec3Constants<float>::fVec3_Zero);
		sincos_tpl(0.0f, &a.x, &a.y);

		const float angleInc = gf_PI2 / (float)numSides;
		float angle = angleInc;

		for (int j = 0; j < numSides; ++j, angle += angleInc)
		{
			sincos_tpl(angle, &b.x, &b.y);

			anchorLines.push_back(a * radius + center);
			anchorLines.push_back(b * radius + center);

			a = b;
		}

		// Draw anchor ID
		//    float fontCol[4] = {0.5f, 1.0f, 0.25f, 1.0f};
		//    IRenderAuxText::DrawLabelExF(m_params.matrix.TransformPoint(center), 0.8f, fontCol, false, true, "%i", i);
	}

	// Pre-evaluate state bits
	const uint32 invalidState = ~m_fragsActive | m_fragsLoose;
	uint32 fragBit = 1;

	// Generate all connection lines
	const uint numFrags = m_frags.size();

	for (uint i = 0; i < numFrags; ++i, fragBit <<= 1)
	{
		if (invalidState & fragBit)
			continue;

		// Get start frag center
		const SGlassFragment& frag = m_frags[i];

		Vec2 startPt = frag.m_center;

		// Create each existing connection
		for (int j = 0; j < frag.m_outConnections.Count(); ++j)
		{
			// Get end frag center
			int m = frag.m_outConnections[j];

			Vec2 endPt = m_frags[m].m_center;

			// Create line
			connLinesA.push_back(startPt);
			connLinesA.push_back(endPt);
		}
	}

	// Draw connections above glass surface
	if (!anchorLines.IsEmpty())
	{
		for (int i = 0; i < anchorLines.Count(); ++i)
		{
			anchorLines[i].z = -0.008f;
		}

		TransformPointList(anchorLines);
		pRenderer->DrawLines(&anchorLines[0], anchorLines.Count(), ColorB(255, 200, 127, 255), 1.0f);
	}

	if (!connLinesA.IsEmpty())
	{
		for (int i = 0; i < connLinesA.Count(); ++i)
		{
			connLinesA[i].z = -0.005f;
		}

		TransformPointList(connLinesA);
		pRenderer->DrawLines(&connLinesA[0], connLinesA.Count(), ColorB(64, 255, 64, 255), 1.0f);
	}

	if (!connLinesB.IsEmpty())
	{
		for (int i = 0; i < connLinesB.Count(); ++i)
		{
			connLinesB[i].z = -0.005f;
		}

		TransformPointList(connLinesB);
		pRenderer->DrawLines(&connLinesB[0], connLinesB.Count(), ColorB(64, 255, 255, 255), 1.0f);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: DrawDebugData
// Desc: Draws debug info, e.g. Energy graph
//--------------------------------------------------------------------------------------------------
void CREBreakableGlass::DrawDebugData(IRenderAuxGeom* const pRenderer, SAuxGeomRenderFlags& flags)
{
	// Don't draw anything if there's nothing to draw!
	if (m_impactParams.empty())
	{
		return;
	}

	// Set flags
	flags = e_Def2DPublicRenderflags;
	flags.SetCullMode(e_CullModeNone);
	flags.SetDepthTestFlag(e_DepthTestOff);
	flags.SetAlphaBlendMode(e_AlphaBlended);
	pRenderer->SetRenderFlags(flags);

	// Draw energy graphs
	Vec3 radialVerts[30];
	Vec3 circularVerts[30];
	uint16 indices[58];

	float totalEnergy, radialEnergy, circularEnergy;

	for (int i = 0; i < 30; ++i)
	{
		// Energy graph point
		totalEnergy = 0.2f * (float)i;
		radialEnergy = circularEnergy = 0.0f;
		CalculateImpactEnergies(totalEnergy, radialEnergy, circularEnergy);

		radialVerts[i] = Vec3((float)i, -radialEnergy, 0.0f);
		circularVerts[i] = Vec3((float)i, -circularEnergy, 0.0f);

		// Indices
		if (i < 29)
		{
			indices[i * 2] = i;
			indices[i * 2 + 1] = i + 1;
		}

		// Rescale graph
		radialVerts[i].x = radialVerts[i].x * 0.005f + 0.05f;
		radialVerts[i].y = radialVerts[i].y * 0.02f + 0.5f;

		circularVerts[i].x = circularVerts[i].x * 0.005f + 0.05f;
		circularVerts[i].y = circularVerts[i].y * 0.02f + 0.5f;
	}

	pRenderer->DrawLines(radialVerts, 30, indices, 58, ColorB(255, 0, 0, 255), 2.0f);
	pRenderer->DrawLines(circularVerts, 30, indices, 58, ColorB(0, 255, 0, 255), 2.0f);

	// Graph axes
	Vec3 graphVerts[4] =
	{
		Vec3(0.05f, 0.5f, 0.2f), Vec3(0.05f + 0.005f * 30.0f, 0.5f,                 0.2f),
		Vec3(0.05f, 0.5f, 0.2f), Vec3(0.05f,                  0.5f - 0.01f * 15.0f, 0.2f)
	};
	pRenderer->DrawLines(graphVerts, 4, ColorB(255, 255, 255, 255), 1.5f);

	// Draw current shatter energy
	const SGlassImpactParams& currImpact = m_impactParams.back();

	float breakThreshold = CalculateBreakThreshold();
	float excessEnergy = max(currImpact.speed * (1.0f / 12.5f) - breakThreshold, 0.0f);

	excessEnergy *= 5.0f;
	excessEnergy = excessEnergy * 0.005f + 0.05f;

	Vec3 lineVerts[2] =
	{
		Vec3(excessEnergy, 0.45f, 0.2f), Vec3(excessEnergy, 0.5f, 0.2f)
	};
	pRenderer->DrawLines(lineVerts, 2, ColorB(127, 255, 255, 255), 2.0f);
}//-------------------------------------------------------------------------------------------------
#endif // GLASS_DEBUG_MODE
