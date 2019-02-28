// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if RENDERER_SUPPORT_SCALEFORM
#include <CrySystem/ISystem.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IStereoRenderer.h>
#include <CrySystem/IConsole.h>

#include "../XRenderD3D9/DriverD3D.h"
#include "../Common/Textures/Texture.h"
#include "../Common/Textures/TextureHelpers.h"
#include "../Common/RenderDisplayContext.h"
#include "ScaleformRender.h"

//////////////////////////////////////////////////////////////////////////

SSF_GlobalDrawParams::SSF_GlobalDrawParams()
{
	m_ScaleformMeshAttributesSize = Align(sizeof(SScaleformMeshAttributes), CRY_PLATFORM_ALIGNMENT);
	m_ScaleformRenderParametersSize = Align(sizeof(SScaleformRenderParameters), CRY_PLATFORM_ALIGNMENT);

	m_pScaleformMeshAttributes = reinterpret_cast<SScaleformMeshAttributes*>(CryModuleMemalign(m_ScaleformMeshAttributesSize, CRY_PLATFORM_ALIGNMENT));
	m_pScaleformRenderParameters = reinterpret_cast<SScaleformRenderParameters*>(CryModuleMemalign(m_ScaleformRenderParametersSize, CRY_PLATFORM_ALIGNMENT));

	Reset();
}

SSF_GlobalDrawParams::~SSF_GlobalDrawParams()
{
	CryModuleMemalignFree(m_pScaleformMeshAttributes);
	CryModuleMemalignFree(m_pScaleformRenderParameters);
}

//////////////////////////////////////////////////////////////////////////

AllocateConstIntCVar(CScaleformPlayback, ms_sys_flash_debugdraw);

float CScaleformPlayback::ms_sys_flash_stereo_maxparallax = 0.02f;
float CScaleformPlayback::ms_sys_flash_debugdraw_depth = 1.0f;

CScaleformPlayback::CScaleformPlayback()
	: m_pRenderer(reinterpret_cast<CD3D9Renderer*>(gEnv->pRenderer))
	, m_stencilAvail(reinterpret_cast<CD3D9Renderer*>(gEnv->pRenderer)->GetStencilBpp() != 0)
	, m_renderMasked(false)
	, m_stencilCounter(0)
	, m_avoidStencilClear(false)
	, m_maskedRendering(false)
	, m_extendCanvasToVP(false)
	, m_applyHalfPixelOffset(false)
	, m_maxVertices(0)
	, m_maxIndices(0)
	, m_matViewport(IDENTITY)
	, m_matViewport3D(IDENTITY)
	, m_matUncached(IDENTITY)
	, m_matCached2D(IDENTITY)
	, m_matCached3D(IDENTITY)
	, m_matCachedWVP(IDENTITY)
	, m_mat(Matrix23::Identity())
	, m_pMatWorld3D(nullptr)
	, m_matView3D(IDENTITY)
	, m_matProj3D(IDENTITY)
	, m_matCached2DDirty(false)
	, m_matCached3DDirty(false)
	, m_matCachedWVPDirty(false)
	, m_cxform()
	, m_glyphVB()
	, m_blendOpStack()
	, m_curBlendMode(Blend_None)
	, m_rsIdx(0)
	, m_renderStats()
	, m_canvasRect(0, 0, 0, 0)
	, m_clearFlags(0)
	, m_clearColor(Clr_Transparent)
	, m_compDepth(0)
	, m_stereo3DiBaseDepth(0)
	, m_maxParallax(0)
	, m_customMaxParallax(-1.0f)
	, m_maxParallaxScene(0)
	, m_screenDepthScene(1.0f)
	, m_stereoFixedProjDepth(false)
	, m_isStereo(false)
	, m_isLeft(false)
	, m_mainThreadID(0)
	, m_renderThreadID(0)
	, m_drawParams()
{
	m_blendOpStack.reserve(16);

	m_cxform[0] = ColorF(1.f, 1.f, 1.f, 1.f);
	m_cxform[1] = ColorF(0.f, 0.f, 0.f, 0.f);

	m_renderStats[0].Clear();
	m_renderStats[1].Clear();

	m_pRenderer->SF_GetMeshMaxSize(m_maxVertices, m_maxIndices);
	m_pRenderer->GetThreadIDs(m_mainThreadID, m_renderThreadID);
}

CScaleformPlayback::~CScaleformPlayback()
{
}

//////////////////////////////////////////////////////////////////////////////////////////
static size_t VertexSizes[] =
{
	 0, // CScaleformPlayback::Vertex_None:
	 4, // CScaleformPlayback::Vertex_XY16i:
	 8, // CScaleformPlayback::Vertex_XY32f:
	 8, // CScaleformPlayback::Vertex_XY16iC32:
	12, // CScaleformPlayback::Vertex_XY16iCF32:
	20, // CScaleformPlayback::Vertex_Glyph:
};

static size_t IndexSizes[] =
{
	 0, // CScaleformPlayback::Index_None:
	 2, // CScaleformPlayback::Index_16:
	 4, // CScaleformPlayback::Index_32:
};

IScaleformPlayback::DeviceData* CScaleformPlayback::CreateDeviceData(const void* pVertices, int numVertices, VertexFormat vf, bool bTemp)
{
	DeviceData* pData = new DeviceData;
	CRY_ASSERT((vf == Vertex_XY16i || vf == Vertex_XY16iC32 || vf == Vertex_XY16iCF32) && "CScaleformPlayback::SetVertexData() -- Unsupported source vertex format!");

	pData->Type = DevDT_Vertex;
	pData->NumElements = numVertices;
	pData->StrideSize = VertexSizes[vf];
	pData->VertexFormat = vf;
	pData->eVertexFormat = m_pRenderer->SF_GetResources().m_vertexDecls[vf];
	pData->OriginalDataPtr = pVertices;
	pData->DeviceDataHandle = gRenDev->m_DevBufMan.Create(BBT_VERTEX_BUFFER, bTemp ? BU_TRANSIENT : BU_STATIC, pData->StrideSize * pData->NumElements);

	assert(pData->DeviceDataHandle != ~0U);
	gRenDev->m_DevBufMan.UpdateBuffer(pData->DeviceDataHandle, pVertices, pData->StrideSize * pData->NumElements);

	return pData;
}

IScaleformPlayback::DeviceData* CScaleformPlayback::CreateDeviceData(const void* pIndices, int numIndices, IndexFormat idxf, bool bTemp)
{
	DeviceData* pData = new DeviceData;
	CRY_ASSERT((idxf == Index_16) && "CScaleformPlayback::SetIndexData() -- Unsupported source index format!");

	pData->Type = DevDT_Index;
	pData->NumElements = numIndices;
	pData->StrideSize = IndexSizes[idxf];
	pData->IndexFormat = idxf;
	pData->OriginalDataPtr = pIndices;
	pData->DeviceDataHandle = gRenDev->m_DevBufMan.Create(BBT_INDEX_BUFFER, bTemp ? BU_TRANSIENT : BU_STATIC, pData->StrideSize * pData->NumElements);

	assert(pData->DeviceDataHandle != ~0U);
	gRenDev->m_DevBufMan.UpdateBuffer(pData->DeviceDataHandle, pIndices, pData->StrideSize * pData->NumElements);

	return pData;
}

IScaleformPlayback::DeviceData* CScaleformPlayback::CreateDeviceData(const BitmapDesc* pBitmapList, int numBitmaps, bool bTemp)
{
	DeviceData* pData = new DeviceData;

	pData->Type = DevDT_BitmapList;
	pData->NumElements = numBitmaps * 6;
	pData->StrideSize = VertexSizes[Vertex_Glyph];
	pData->VertexFormat = Vertex_Glyph;
	pData->eVertexFormat = m_pRenderer->SF_GetResources().m_vertexDecls[Vertex_Glyph];
	pData->OriginalDataPtr = pBitmapList;
	pData->DeviceDataHandle = gRenDev->m_DevBufMan.Create(BBT_VERTEX_BUFFER, bTemp ? BU_TRANSIENT : BU_STATIC, pData->StrideSize * pData->NumElements);

	// NOTE: Get aligned scratch-mem, can be too large for stack-mem (pointer and size aligned to manager's alignment requirement)
	CryScopedAllocWithSizeVector(SGlyphVertex, pData->StrideSize * pData->NumElements, pGlyphs, CDeviceBufferManager::AlignBufferSizeForStreaming);

#if CRY_COMPILER_MSVC     /* Visual Studio */
#  define XXH_swap32 _byteswap_ulong
#elif CRY_COMPILER_GCC || CRY_COMPILER_CLANG
#  define XXH_swap32 __builtin_bswap32
#else
#  error Bad compiler
#endif

	// merge multiple bitmaps into one draw call via stitching the triangle strip
	for (int bitmapIdx(0), vertOffs(0); bitmapIdx < numBitmaps; ++bitmapIdx)
	{
		const BitmapDesc& bitmapDesc(pBitmapList[bitmapIdx]);
		const RectF& coords(bitmapDesc.Coords);
		const RectF& uvCoords(bitmapDesc.TextureCoords);

		// Scaleform uses BGRA, we use RGBA
		unsigned int color = (bitmapDesc.Color & 0xFF00FF00) + (XXH_swap32(bitmapDesc.Color & 0x00FF00FF) >> 8);

		SGlyphVertex& v0(pGlyphs[vertOffs]);
		v0.x = coords.Left;
		v0.y = coords.Top;
		v0.u = uvCoords.Left;
		v0.v = uvCoords.Top;
		v0.col = color;
		++vertOffs;

		pGlyphs[vertOffs] = pGlyphs[vertOffs - 1];
		++vertOffs;

		SGlyphVertex& v1(pGlyphs[vertOffs]);
		v1.x = coords.Right;
		v1.y = coords.Top;
		v1.u = uvCoords.Right;
		v1.v = uvCoords.Top;
		v1.col = color;
		++vertOffs;

		SGlyphVertex& v2(pGlyphs[vertOffs]);
		v2.x = coords.Left;
		v2.y = coords.Bottom;
		v2.u = uvCoords.Left;
		v2.v = uvCoords.Bottom;
		v2.col = color;
		++vertOffs;

		SGlyphVertex& v3(pGlyphs[vertOffs]);
		v3.x = coords.Right;
		v3.y = coords.Bottom;
		v3.u = uvCoords.Right;
		v3.v = uvCoords.Bottom;
		v3.col = color;
		++vertOffs;

		// degenerate triangle for termination
		pGlyphs[vertOffs] = pGlyphs[vertOffs - 1];
		++vertOffs;
	}

	assert(pData->DeviceDataHandle != ~0U);
	gRenDev->m_DevBufMan.UpdateBuffer(pData->DeviceDataHandle, pGlyphs, pData->StrideSize * pData->NumElements);

	return pData;
}

void CScaleformPlayback::ReleaseDeviceData(DeviceData* pData)
{
	gRenDev->m_DevBufMan.Destroy(pData->DeviceDataHandle);

	delete pData;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Private helper functions

void CScaleformPlayback::UpdateStereoSettings()
{
	IStereoRenderer* pStereoRenderer = m_pRenderer->GetIStereoRenderer();
	assert(pStereoRenderer);

	const float maxParallax = m_customMaxParallax >= 0.0f ? m_customMaxParallax : ms_sys_flash_stereo_maxparallax;
	m_maxParallax = !m_isStereo ? 0 : m_isLeft ? -maxParallax : maxParallax;
	m_maxParallax *= pStereoRenderer->GetStereoStrength();

	const float maxParallaxScene = pStereoRenderer->GetMaxSeparationScene() * 2;
	m_maxParallaxScene = !m_isStereo ? 0 : m_isLeft ? -maxParallaxScene : maxParallaxScene;
	m_screenDepthScene = pStereoRenderer->GetZeroParallaxPlaneDist();
}

namespace CScaleformPlaybackClearInternal
{
	const float safeRangeScale = 8.0f;

	struct MapToSafeRange
	{
		MapToSafeRange(float v) : val(v) {}
		operator int16() const
		{
			float v = val / safeRangeScale;
			return (int16)(v >= 0 ? v + 0.5f : v - 0.5f);
		}
		float val;
	};
}

void CScaleformPlayback::Clear(const ColorF& backgroundColor)
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	SSF_GlobalDrawParams& __restrict params = m_drawParams;
	SSF_GlobalDrawParams::OutputParams& __restrict rCurOutput = *params.pRenderOutput;

	if (backgroundColor.a == 0.f)
		return;
	if (backgroundColor.a == 255.f)
	{
		RECT rect = 
		{
			LONG(params.viewport.TopLeftX), 
			LONG(params.viewport.TopLeftY), 
			LONG(params.viewport.TopLeftX + params.viewport.Width), 
			LONG(params.viewport.TopLeftY + params.viewport.Height)
		};

		rCurOutput.clearPass.Execute(rCurOutput.pRenderTarget, backgroundColor, 1, &rect);
		rCurOutput.bRenderTargetClear = false;
		return;
	}

	using namespace CScaleformPlaybackClearInternal;

	// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
	CryStackAllocWithSizeVectorCleared(int16, 4 * 2, quadMem, CDeviceBufferManager::AlignBufferSizeForStreaming);

	quadMem[0] = MapToSafeRange(m_canvasRect.Left);
	quadMem[1] = MapToSafeRange(m_canvasRect.Top);
	quadMem[2] = MapToSafeRange(m_canvasRect.Right);
	quadMem[3] = MapToSafeRange(m_canvasRect.Top);
	quadMem[4] = MapToSafeRange(m_canvasRect.Left);
	quadMem[5] = MapToSafeRange(m_canvasRect.Bottom);
	quadMem[6] = MapToSafeRange(m_canvasRect.Right);
	quadMem[7] = MapToSafeRange(m_canvasRect.Bottom);

	const DeviceData* pData = CreateDeviceData(quadMem, 4, Vertex_XY16i, true);

	Matrix44 mat(m_matViewport);
	mat.m00 *= safeRangeScale;
	mat.m11 *= safeRangeScale;

	params.m_bScaleformMeshAttributesDirty = true;
	params.m_pScaleformMeshAttributes->cCompositeMat = mat;
	params.fillType = SSF_GlobalDrawParams::SolidColor;

	int oldBendState(params.blendModeStates);
	params.blendModeStates = backgroundColor.a < 255 ? GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA : 0;
	int oldRenderMaskedStates(params.renderMaskedStates);
	params.renderMaskedStates = m_renderMasked ? params.renderMaskedStates : 0;

	// setup render parameters
	ApplyColor(backgroundColor);
	ApplyTextureInfo(0);
	ApplyTextureInfo(1);

	// draw quad
	m_pRenderer->SF_DrawGlyphClear(pData, 0, params);
	rCurOutput.bRenderTargetClear = false;

	params.blendModeStates = oldBendState;
	params.renderMaskedStates = oldRenderMaskedStates;

	ReleaseDeviceData(const_cast<DeviceData*>(pData));

	// update stats
	m_renderStats[m_rsIdx].Triangles += 2;
	++m_renderStats[m_rsIdx].Primitives;
}

static inline void matrixOrthoOffCenterLH(Matrix44& mat, f32 l, f32 r, f32 b, f32 t)
{
	mat.m00 = 2.0f / (r - l);
	mat.m01 = 0;
	mat.m02 = 0;
	mat.m03 = (l + r) / (l - r);
	mat.m10 = 0;
	mat.m11 = 2.0f / (t - b);
	mat.m12 = 0;
	mat.m13 = (t + b) / (b - t);
	mat.m20 = 0;
	mat.m21 = 0;
	mat.m22 = 1.0f;
	mat.m23 = 0.0f;
	mat.m30 = 0;
	mat.m31 = 0;
	mat.m32 = 0;
	mat.m33 = 1.0f;
}

static inline void matrixOrthoToViewport(Matrix44& mat, int rtX0, int rtY0, int rtWidth, int rtHeight, int vpX0, int vpY0, int vpWidth, int vpHeight)
{
	//// map viewport into section of normalized clip space rect [(-1,1)...(1,-1)] using matrixOrthoOffCenterLH
	//float sx = 2.0f / (float) rtWidth;
	//float ox = (float) (rtX0 + rtX0 + rtWidth) / (float) (-rtWidth);

	//float sy = 2.0f / (float) -rtHeight;
	//float oy = (float) (rtY0 + rtY0 + rtHeight) / (float) (rtHeight);

	//float x0r = sx * vpX0 + ox;
	//float y0r = sy * vpY0 + oy;

	//float x1r = sx * (vpX0 + vpWidth ) + ox;
	//float y1r = sy * (vpY0 + vpHeight) + oy;

	//// remap normalized clip space rect [(-1,1)...(1,-1)] to viewport section in normalized clip space
	//float xs = (x1r - x0r) / 2.0f;
	//float xo = xs + x0r;

	//float ys = (y0r - y1r) / 2.0f;
	//float yo = ys + y1r;

	float xs = (float)vpWidth / (float)rtWidth;
	float ys = (float)vpHeight / (float)rtHeight;

	float xo = (vpWidth + 2.0f * (vpX0 - rtX0) - rtWidth) / (float)rtWidth;
	float yo = -(vpHeight + 2.0f * (vpY0 - rtY0) - rtHeight) / (float)rtHeight;

	mat.m00 = xs;
	mat.m01 = 0;
	mat.m02 = 0;
	mat.m03 = xo;
	mat.m10 = 0;
	mat.m11 = ys;
	mat.m12 = 0;
	mat.m13 = yo;
	mat.m20 = 0;
	mat.m21 = 0;
	mat.m22 = 1.0f;
	mat.m23 = 0.0f;
	mat.m30 = 0;
	mat.m31 = 0;
	mat.m32 = 0;
	mat.m33 = 1.0f;
}

void CScaleformPlayback::ApplyStencilMask(SSF_GlobalDrawParams::ESFMaskOp maskOp, unsigned int stencilRef)
{
	SSF_GlobalDrawParams& __restrict params = m_drawParams;

	uint32 renderMaskedStates = 0;
	int stencilFunc(FSS_STENCFUNC_ALWAYS);
	int stencilPass(FSS_STENCOP_KEEP);

	switch (maskOp)
	{
	case SSF_GlobalDrawParams::BeginSubmitMask_Clear:
		renderMaskedStates = GS_STENCIL | GS_NOCOLMASK_RGBA;
		stencilFunc = FSS_STENCFUNC_ALWAYS;
		stencilPass = FSS_STENCOP_REPLACE;
		break;
	case SSF_GlobalDrawParams::BeginSubmitMask_Inc:
		renderMaskedStates = GS_STENCIL | GS_NOCOLMASK_RGBA;
		stencilFunc = FSS_STENCFUNC_EQUAL;
		stencilPass = FSS_STENCOP_INCR;
		break;
	case SSF_GlobalDrawParams::BeginSubmitMask_Dec:
		renderMaskedStates = GS_STENCIL | GS_NOCOLMASK_RGBA;
		stencilFunc = FSS_STENCFUNC_EQUAL;
		stencilPass = FSS_STENCOP_DECR;
		break;
	case SSF_GlobalDrawParams::EndSubmitMask:
		renderMaskedStates = GS_STENCIL;
		stencilFunc = FSS_STENCFUNC_EQUAL;
		stencilPass = FSS_STENCOP_KEEP;
		break;
	case SSF_GlobalDrawParams::DisableMask:
	default:
		break;
	}

	params.renderMaskedStates =
		renderMaskedStates;
	params.m_stencil.func =
		STENC_FUNC(stencilFunc) |
		STENCOP_FAIL(FSS_STENCOP_KEEP) |
		STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
		STENCOP_PASS(stencilPass);
	params.m_stencil.ref =
		stencilRef;
}

void CScaleformPlayback::ApplyBlendMode(BlendType blendMode)
{
	SSF_GlobalDrawParams& __restrict params = m_drawParams;
	SSF_GlobalDrawParams::SScaleformRenderParameters* __restrict rparams = params.m_pScaleformRenderParameters;

	IF(ms_sys_flash_debugdraw, 0)
	{
		m_curBlendMode = Blend_None;
		params.blendModeStates = 0;
		return;
	}

	m_curBlendMode = blendMode;
	params.blendModeStates = GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
	params.premultipliedAlpha = false;

	switch (blendMode)
	{
	case Blend_None:
	case Blend_Normal:
	case Blend_Layer:
	case Blend_Screen:
	case Blend_Difference:
	case Blend_Invert:
	case Blend_Overlay:
	case Blend_HardLight:
		params.blendModeStates = GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
		break;
	case Blend_Multiply:
		params.blendModeStates = GS_BLSRC_DSTCOL | GS_BLDST_ZERO;
		break;
	case Blend_Add:
	case Blend_Subtract:
	case Blend_Lighten:
	case Blend_Darken:
		params.blendModeStates = GS_BLSRC_SRCALPHA | GS_BLDST_ONE;
		break;
	case Blend_Alpha:
	case Blend_Erase:
		params.blendModeStates = GS_BLSRC_ZERO | GS_BLDST_ONE;
		break;
	default:
		assert(!"CScaleformPlayback::ApplyBlendMode() -- Unsupported blend type!");
		break;
	}

	switch (blendMode)
	{
	case Blend_Subtract:
		params.blendOp = SSF_GlobalDrawParams::RevSubstract;
		break;
	case Blend_Lighten:
		params.blendOp = SSF_GlobalDrawParams::Max;
		break;
	case Blend_Darken:
		params.blendOp = SSF_GlobalDrawParams::Min;
		break;
	default:
		params.blendOp = SSF_GlobalDrawParams::Add;
		break;
	}

	params.isMultiplyDarkBlendMode = m_curBlendMode == Blend_Multiply || m_curBlendMode == Blend_Darken;

	if (m_maskedRendering && m_isStereo)
	{
		if (params.blendOp == SSF_GlobalDrawParams::Add && params.blendModeStates == (GS_BLSRC_SRCALPHA | GS_BLDST_ONE))
		{
			// Special blend mode for stereo, where icons get occluded by weapon
			params.blendModeStates = GS_BLSRC_DSTALPHA | GS_BLDST_ONE;
			params.premultipliedAlpha = true;
		}
	}

	const float fPreMul = params.premultipliedAlpha ? 1.f : 0.f;

	params.m_bScaleformRenderParametersDirty = params.m_bScaleformRenderParametersDirty ||
		(rparams->bPremultiplyAlpha != fPreMul);

	rparams->bPremultiplyAlpha = fPreMul;
}

void CScaleformPlayback::ApplyColor(const ColorF& src)
{
	SSF_GlobalDrawParams& __restrict params = m_drawParams;
	SSF_GlobalDrawParams::SScaleformRenderParameters* __restrict rparams = params.m_pScaleformRenderParameters;

	ColorF premultiplied(src * (1.0f / 255.0f));

	if (m_curBlendMode == Blend_Multiply || m_curBlendMode == Blend_Darken)
	{
		premultiplied.r = 1.0f + premultiplied.a * (premultiplied.r - 1.0f);
		premultiplied.g = 1.0f + premultiplied.a * (premultiplied.g - 1.0f);
		premultiplied.b = 1.0f + premultiplied.a * (premultiplied.b - 1.0f);
	}

	params.m_bScaleformRenderParametersDirty = params.m_bScaleformRenderParametersDirty ||
		(rparams->cBitmapColorTransform[0] != premultiplied) ||
		(rparams->cBitmapColorTransform[1] != ColorF(0, 0, 0, 0));

	rparams->cBitmapColorTransform[0] = premultiplied;
	rparams->cBitmapColorTransform[1] = ColorF(0, 0, 0, 0);
}

void CScaleformPlayback::ApplyTextureInfo(unsigned int texSlot, const FillTexture* pFill)
{
	assert(texSlot < 2);
	IF(texSlot >= 2, 0)
		return;

	SSF_GlobalDrawParams::STextureInfo& __restrict texInfo(m_drawParams.texture[texSlot]);
	SSF_GlobalDrawParams::SScaleformMeshAttributes* __restrict mparams = m_drawParams.m_pScaleformMeshAttributes;

	if (pFill && pFill->pTexture)
	{
		CTexture* pTex = static_cast<CTexture*>(pFill->pTexture);
		const Matrix23& m(pFill->TextureMatrix);
		assert(pTex->GetSrcFormat() != eTF_YUV && pTex->GetSrcFormat() != eTF_YUVA);

		texInfo.pTex = pTex;
		texInfo.texState  = (pFill->WrapMode   == Wrap_Clamp   ) ? SSF_GlobalDrawParams::TS_Clamp     : 0;
		texInfo.texState |= (pFill->SampleMode == Sample_Linear) ? SSF_GlobalDrawParams::TS_FilterTriLin : 0;

		m_drawParams.m_bScaleformMeshAttributesDirty = m_drawParams.m_bScaleformMeshAttributesDirty ||
			(mparams->cTexGenMat[texSlot][0] != Vec4(m.m00, m.m01, 0, m.m02)) ||
			(mparams->cTexGenMat[texSlot][1] != Vec4(m.m10, m.m11, 0, m.m12));

		mparams->cTexGenMat[texSlot][0] = Vec4(m.m00, m.m01, 0, m.m02);
		mparams->cTexGenMat[texSlot][1] = Vec4(m.m10, m.m11, 0, m.m12);
	}
	else
	{
		// No texture 0, disable texture filling
		if (texSlot == 0)
			m_drawParams.fillType = std::min(m_drawParams.fillType, SSF_GlobalDrawParams::GColor);
		// No texture 1, downgrade to one texture
		else if (texSlot == 1 && m_drawParams.fillType >= SSF_GlobalDrawParams::G2Texture)
			m_drawParams.fillType = SSF_GlobalDrawParams::EFillType(m_drawParams.fillType - (SSF_GlobalDrawParams::G2Texture - SSF_GlobalDrawParams::G1Texture));

		texInfo.pTex = TextureHelpers::LookupTexDefault(EFTT_DIFFUSE);
		texInfo.texState = 0;

		m_drawParams.m_bScaleformMeshAttributesDirty = m_drawParams.m_bScaleformMeshAttributesDirty ||
			(mparams->cTexGenMat[texSlot][0] != Vec4(1, 0, 0, 0)) ||
			(mparams->cTexGenMat[texSlot][1] != Vec4(0, 1, 0, 0));

		mparams->cTexGenMat[texSlot][0] = Vec4(1, 0, 0, 0);
		mparams->cTexGenMat[texSlot][1] = Vec4(0, 1, 0, 0);
	}
}

void CScaleformPlayback::ApplyCxform()
{
	SSF_GlobalDrawParams::SScaleformRenderParameters* __restrict rparams = m_drawParams.m_pScaleformRenderParameters;

	m_drawParams.m_bScaleformRenderParametersDirty = m_drawParams.m_bScaleformRenderParametersDirty ||
		(rparams->cBitmapColorTransform[0] != (m_cxform[0])) ||
		(rparams->cBitmapColorTransform[1] != (m_cxform[1] * (1.0f / 255.0f)));

	rparams->cBitmapColorTransform[0] = m_cxform[0];
	rparams->cBitmapColorTransform[1] = m_cxform[1] * (1.0f / 255.0f);
}

void CScaleformPlayback::CalcTransMat2D(const Matrix23* pSrc, Matrix44* __restrict pDst) const
{
	assert(pSrc && pDst);

	Matrix44 mat;
	mat.m00 = pSrc->m00;
	mat.m01 = pSrc->m01;
	mat.m02 = 0;
	mat.m03 = pSrc->m02;
	mat.m10 = pSrc->m10;
	mat.m11 = pSrc->m11;
	mat.m12 = 0;
	mat.m13 = pSrc->m12;
	mat.m20 = 0;
	mat.m21 = 0;
	mat.m22 = 0;
	mat.m23 = m_compDepth;
	mat.m30 = 0;
	mat.m31 = 0;
	mat.m32 = 0;
	mat.m33 = 1;

	*pDst = m_matViewport * mat;
}

void CScaleformPlayback::CalcTransMat3D(const Matrix23* pSrc, Matrix44* __restrict pDst)
{
	assert(pSrc && pDst);
	assert(m_pMatWorld3D);

	if (m_matCachedWVPDirty)
	{
		Matrix44 matProjPatched = m_matProj3D;
		assert((matProjPatched.m23 == -1.0f || matProjPatched.m23 == 0.0f) && "CScaleformPlayback::SetPerspective3D() -- projMatIn is not right handed");
		if (!m_stereoFixedProjDepth)
		{
			matProjPatched.m20 = -m_maxParallax;   // negative here as m_matProj3D is RH
			matProjPatched.m30 = -m_maxParallax * m_stereo3DiBaseDepth;
		}
		else
		{
			const float maxParallax = m_maxParallaxScene;

			const float projDepth = m_stereo3DiBaseDepth;
			const float screenDepth = m_screenDepthScene;
			const float perceivedDepth = max((*m_pMatWorld3D).m32, 1e-4f);

			matProjPatched.m30 += projDepth * maxParallax * (1.0f - screenDepth / perceivedDepth);
			matProjPatched.m23 = 0;
			matProjPatched.m33 = projDepth;
		}

		Matrix44 mat = (*m_pMatWorld3D);
		mat = mat * (m_matView3D);
		mat = mat * (matProjPatched);

		Matrix44 matCachedWVP;
		matCachedWVP.m00 = mat.m00;
		matCachedWVP.m01 = mat.m10;
		matCachedWVP.m02 = mat.m20;
		matCachedWVP.m03 = mat.m30;
		matCachedWVP.m10 = mat.m01;
		matCachedWVP.m11 = mat.m11;
		matCachedWVP.m12 = mat.m21;
		matCachedWVP.m13 = mat.m31;
		if (!m_stereoFixedProjDepth)
		{
			matCachedWVP.m20 = 0;
			matCachedWVP.m21 = 0;
			matCachedWVP.m22 = 0;
			matCachedWVP.m23 = 0;                                                                   // force set homogeneous z to 0
		}
		else
		{
			const float d = m_compDepth;
			matCachedWVP.m20 = mat.m03 * d;
			matCachedWVP.m21 = mat.m13 * d;
			matCachedWVP.m22 = mat.m23 * d;
			matCachedWVP.m23 = mat.m33 * d;
		}
		matCachedWVP.m30 = mat.m03;
		matCachedWVP.m31 = mat.m13;
		matCachedWVP.m32 = mat.m23;
		matCachedWVP.m33 = mat.m33;

		m_matCachedWVP = m_matViewport3D * matCachedWVP;
		m_matCachedWVPDirty = false;
	}

	Matrix44 mat;
	mat.m00 = pSrc->m00;
	mat.m01 = pSrc->m01;
	mat.m02 = 0;
	mat.m03 = pSrc->m02;
	mat.m10 = pSrc->m10;
	mat.m11 = pSrc->m11;
	mat.m12 = 0;
	mat.m13 = pSrc->m12;
	mat.m20 = 0;
	mat.m21 = 0;
	mat.m22 = 1;
	mat.m23 = 0;
	mat.m30 = 0;
	mat.m31 = 0;
	mat.m32 = 0;
	mat.m33 = 1;

	*pDst = m_matCachedWVP * mat;
}

void CScaleformPlayback::ApplyMatrix(const Matrix23* pMatIn)
{
	assert(pMatIn);

	bool is3D = m_pMatWorld3D != 0;
	bool useCaching = pMatIn == &m_mat;

	Matrix44* __restrict pMatOut = 0;

	if (!is3D)
	{
		pMatOut = useCaching ? &m_matCached2D : &m_matUncached;
		if (!useCaching || m_matCached2DDirty)
		{
			CalcTransMat2D(pMatIn, pMatOut);
			if (useCaching)
				m_matCached2DDirty = false;
		}
	}
	else
	{
		pMatOut = useCaching ? &m_matCached3D : &m_matUncached;
		if (!useCaching || m_matCached3DDirty)
		{
			CalcTransMat3D(pMatIn, pMatOut);
			if (useCaching)
				m_matCached3DDirty = false;
		}
	}

	m_drawParams.m_bScaleformMeshAttributesDirty = true;
	m_drawParams.m_pScaleformMeshAttributes->cCompositeMat = *pMatOut;
}

//////////////////////////////////////////////////////////////////////////////////////////
// IScaleformPlayback interface

ITexture* CScaleformPlayback::CreateRenderTarget()
{
	assert(0 && "CScaleformPlayback::CreateRenderTarget() -- Not implemented!");
	return 0;
}

void CScaleformPlayback::SetDisplayRenderTarget(ITexture* /*pRT*/, bool /*setState*/)
{
	assert(0 && "CScaleformPlayback::SetDisplayRenderTarget() -- Not implemented!");
}

void CScaleformPlayback::PushRenderTarget(const RectF& /*frameRect*/, ITexture* /*pRT*/)
{
	assert(0 && "CScaleformPlayback::PushRenderTarget() -- Not implemented!");
}

void CScaleformPlayback::PopOutputTarget()
{
	SSF_GlobalDrawParams& __restrict params = m_drawParams;
	SSF_GlobalDrawParams::OutputParams* __restrict pCurOutput = params.pRenderOutput;

	// Graphics pipeline >= 1
	if (CRenderer::CV_r_GraphicsPipeline > 0)
	{
		if (pCurOutput->renderPass.GetPrimitiveCount() >= 1)
		{
			pCurOutput->renderPass.Execute();
			pCurOutput->renderPass.BeginAddingPrimitives();
		}

		m_pRenderer->SF_GetResources().m_PrimitiveHeap.FreeUsedPrimitives(pCurOutput->key);

		GetDeviceObjectFactory().GetCoreCommandList().Reset();
	}

	// Graphics pipeline >= 0
	{
		m_pRenderer->SF_GetResources().m_CBHeap.FreeUsedConstantBuffers();

		params.m_vsBuffer = nullptr;
		params.m_psBuffer = nullptr;

		params.m_bScaleformMeshAttributesDirty = true;
		params.m_bScaleformRenderParametersDirty = true;
	}

	m_renderTargetStack.pop_back();

	// Graphics pipeline >= 1
	params.pRenderOutput = nullptr;

	m_pRenderer->SF_GetResources().FreeColorSurfaces();
	m_pRenderer->SF_GetResources().FreeStencilSurfaces();
}

void CScaleformPlayback::PushOutputTarget(const Viewport &viewport)
{
	SSF_GlobalDrawParams& __restrict params = m_drawParams;

	m_renderTargetStack.emplace_back();
	SSF_GlobalDrawParams::OutputParams& sNewOutput = m_renderTargetStack.back();

	sNewOutput.key = m_renderTargetStack.size() - 1;
	sNewOutput.oldViewMat = m_matViewport;
	sNewOutput.oldViewportHeight = 0;
	sNewOutput.oldViewportWidth = 0;

	// NOTE: this is a hacky workaround for all custom resolution outputs
	if (auto* pDC = m_pRenderOutput->GetDisplayContext())
	{
		sNewOutput.pRenderTarget  = pDC->GetCurrentColorOutput();
		sNewOutput.pStencilTarget = pDC->GetCurrentDepthOutput();
	}
	else
	{
		sNewOutput.pRenderTarget  = m_pRenderOutput->GetColorTarget();
		sNewOutput.pStencilTarget = m_pRenderOutput->GetDepthTarget();
	}

	// Graphics pipeline >= 1
	{
		params.viewport = SSF_GlobalDrawParams::_D3DViewPort((float)viewport.Left, (float)viewport.Top, (float)viewport.Width, (float)viewport.Height);

		sNewOutput.bRenderTargetClear  = sNewOutput.pRenderTarget  && (m_clearFlags & FRT_CLEAR_COLOR  ) && !(m_pRenderOutput->m_hasBeenCleared & FRT_CLEAR_COLOR  );
		sNewOutput.bStencilTargetClear = sNewOutput.pStencilTarget && (m_clearFlags & FRT_CLEAR_STENCIL) && !(m_pRenderOutput->m_hasBeenCleared & FRT_CLEAR_STENCIL);

		sNewOutput.renderPass.SetRenderTarget(0, sNewOutput.pRenderTarget);
		sNewOutput.renderPass.SetDepthTarget(sNewOutput.pStencilTarget);
		sNewOutput.renderPass.SetScissor(params.scissorEnabled, params.scissor);
		sNewOutput.renderPass.SetViewport(params.viewport);
		sNewOutput.renderPass.BeginAddingPrimitives();

		params.m_bScaleformMeshAttributesDirty   = params.m_bScaleformMeshAttributesDirty   || !(params.m_vsBuffer);
		params.m_bScaleformRenderParametersDirty = params.m_bScaleformRenderParametersDirty || !(params.m_psBuffer);
	}

	// Graphics pipeline >= 1
	params.pRenderOutput = &m_renderTargetStack.back();
}

void CScaleformPlayback::PopRenderTarget()
{
#ifdef ENABLE_FLASH_FILTERS
	SSF_GlobalDrawParams& __restrict params = m_drawParams;
	SSF_GlobalDrawParams::OutputParams* __restrict pCurOutput = params.pRenderOutput;

	if (m_renderTargetStack.size() <= 1)
		CryFatalError("PopRenderTarget called incorrectly");

	// Graphics pipeline >= 1
	if (CRenderer::CV_r_GraphicsPipeline > 0)
	{
		if (pCurOutput->renderPass.GetPrimitiveCount() >= 1)
		{
			pCurOutput->renderPass.Execute();
			pCurOutput->renderPass.BeginAddingPrimitives();
		}

		m_pRenderer->SF_GetResources().m_PrimitiveHeap.FreeUsedPrimitives(pCurOutput->key);

		GetDeviceObjectFactory().GetCoreCommandList().Reset();
	}

	// Graphics pipeline >= 0
	{
		m_pRenderer->SF_GetResources().m_CBHeap.FreeUsedConstantBuffers();

		params.m_vsBuffer = nullptr;
		params.m_psBuffer = nullptr;

		params.m_bScaleformMeshAttributesDirty   = true;
		params.m_bScaleformRenderParametersDirty = true;
	}

	//clear texture slot, may not be required
	ApplyTextureInfo(0);
//	ApplyTextureInfo(1);

	// restore viewport matrix when popping render target
	m_matViewport = pCurOutput->oldViewMat;

	// Graphics pipeline >= 1
	{
		params.viewport = SSF_GlobalDrawParams::_D3DViewPort(0.f, 0.f, float(pCurOutput->oldViewportWidth), float(pCurOutput->oldViewportHeight));
	}

	m_renderTargetStack.pop_back();

	// Graphics pipeline >= 1
	params.pRenderOutput = &m_renderTargetStack.back();
#endif
}

int32 CScaleformPlayback::PushTempRenderTarget(const RectF& frameRect, uint32 targetW, uint32 targetH, bool wantClear, bool wantStencil)
{
#ifdef ENABLE_FLASH_FILTERS
	SSF_GlobalDrawParams& __restrict params = m_drawParams;

	uint32 RTWidth  = max((uint32)2, targetW);
	uint32 RTHeight = max((uint32)2, targetH);

	//align to power of 2 dimensions
	if (!IsPowerOfTwo(RTWidth))
	{
		int log2 = IntegerLog2_RoundUp(RTWidth);
		RTWidth = 1 << log2;
	}

	if (!IsPowerOfTwo(RTHeight))
	{
		int log2 = IntegerLog2_RoundUp(RTHeight);
		RTHeight = 1 << log2;
	}

	CTexture* pNewTempRT = m_pRenderer->SF_GetResources().GetColorSurface(m_pRenderer, RTWidth, RTHeight, eTF_R8G8B8A8, RTWidth, RTHeight);// TODO: allow larger RTs upto ST (needs viewport adjustment), pNewTempST->pTexture->GetWidth(), pNewTempST->pTexture->GetHeight());
	CTexture* pNewTempST = wantStencil ?
		m_pRenderer->SF_GetResources().GetStencilSurface(m_pRenderer, pNewTempRT->GetWidth(), pNewTempRT->GetHeight(), CRendererResources::s_hwTexFormatSupport.GetClosestFormatSupported(eTF_D24S8)) :
		nullptr;

	m_renderTargetStack.emplace_back();
	SSF_GlobalDrawParams::OutputParams& sNewOutput = m_renderTargetStack.back();

	sNewOutput.key = m_renderTargetStack.size() - 1;
	sNewOutput.oldViewMat = m_matViewport;
	sNewOutput.oldViewportHeight = 0; // Previous width
	sNewOutput.oldViewportWidth = 0;  // Previous height
	sNewOutput.pRenderTarget = pNewTempRT;
	sNewOutput.pStencilTarget = pNewTempST;
	sNewOutput.bRenderTargetClear = pNewTempRT && wantClear;
	sNewOutput.bStencilTargetClear = pNewTempST && false;

	// Graphics pipeline >= 1
	{
		params.viewport = SSF_GlobalDrawParams::_D3DViewPort(0.f, 0.f, float(targetW), float(targetH));

		sNewOutput.renderPass.SetRenderTarget(0, sNewOutput.pRenderTarget);
		sNewOutput.renderPass.SetDepthTarget(sNewOutput.pStencilTarget);
		sNewOutput.renderPass.SetScissor(params.scissorEnabled, params.scissor);
		sNewOutput.renderPass.SetViewport(params.viewport);
		sNewOutput.renderPass.BeginAddingPrimitives();

		params.m_bScaleformMeshAttributesDirty = params.m_bScaleformMeshAttributesDirty || !(params.m_vsBuffer);
		params.m_bScaleformRenderParametersDirty = params.m_bScaleformRenderParametersDirty || !(params.m_psBuffer);
	}

	// Graphics pipeline >= 1
	params.pRenderOutput = &m_renderTargetStack.back();

	// set ViewportMatrix
	float dx = frameRect.Width();
	float dy = frameRect.Height();
	if (dx < 1) { dx = 1; }
	if (dy < 1) { dy = 1; }

	m_matViewport.SetIdentity();
	m_matViewport.m00 =  2.0f / dx;
	m_matViewport.m11 = -2.0f / dy;
	m_matViewport.m03 = -1.0f - m_matViewport.m00 * (frameRect.Left);
	m_matViewport.m13 =  1.0f - m_matViewport.m11 * (frameRect.Top);

	if (m_applyHalfPixelOffset)
	{
		// Adjust by -0.5 pixel to match DirectX pixel coverage rules.
		float xhalfPixelAdjust = (targetW > 0) ? (1.0f / (float)targetW) : 0.0f;
		float yhalfPixelAdjust = (targetH > 0) ? (1.0f / (float)targetH) : 0.0f;

		m_matViewport.m03 -= xhalfPixelAdjust;
		m_matViewport.m13 += yhalfPixelAdjust;
	}

	return (pNewTempRT) ? pNewTempRT->GetID() : -1;
#else
	return -1;
#endif
}

void CScaleformPlayback::BeginDisplay(ColorF backgroundColor, const Viewport& viewport, bool bScissor, const Viewport& scissor, const RectF& x0x1y0y1)
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SSF_GlobalDrawParams& __restrict params = m_drawParams;

	UpdateStereoSettings();

	// NOTE: this is a hacky workaround for all render-target stack pushed that specify no CTexture
	if (!m_pRenderOutput)
	{
		if (auto* pDC = m_pRenderer->GetActiveDisplayContext())
			m_pRenderOutput = pDC->GetRenderOutput();

		if (!m_pRenderOutput)
		{
			if (auto* pDC = m_pRenderer->GetBaseDisplayContext())
				m_pRenderOutput = pDC->GetRenderOutput();
		}
	}

	m_pRenderOutput->BeginRendering(nullptr, 0); // Override clear flag

	assert(x0x1y0y1.Width() != 0 && x0x1y0y1.Height() != 0);

	float invWidth (1.0f / std::max(x0x1y0y1.Width (), 1.0f));
	float invHeight(1.0f / std::max(x0x1y0y1.Height(), 1.0f));

	// Positions the scale-form viewport inside the (possibly larger) render-output viewport (which doesn't need to be output-resolution)
	auto& outputViewPort = m_pRenderOutput->GetDisplayContext() ? m_pRenderOutput->GetDisplayContext()->GetViewport() : m_pRenderOutput->GetViewport();
	int rtX0(outputViewPort.x), rtY0(outputViewPort.y), rtWidth(outputViewPort.width), rtHeight(outputViewPort.height);

	rtWidth  = std::max(rtWidth , 1);
	rtHeight = std::max(rtHeight, 1);

	Matrix44 matScale = Matrix44(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
	matScale.m00 = (float)viewport.Width  * invWidth;
	matScale.m11 = (float)viewport.Height * invHeight;
	matScale.m03 = -x0x1y0y1.Left * invWidth  + (float)viewport.Left;
	matScale.m13 = -x0x1y0y1.Top  * invHeight + (float)viewport.Top;

	Matrix44 matRescale;
	matrixOrthoOffCenterLH(matRescale, (f32)rtX0, (f32)(rtX0 + rtWidth), (f32)(rtY0 + rtHeight), (f32)rtY0);
	if (m_applyHalfPixelOffset)
	{
		matRescale.m03 -= 1.0f / (float)rtWidth;
		matRescale.m13 += 1.0f / (float)rtHeight;
	}
	m_matViewport = matRescale * matScale;

	matrixOrthoToViewport(m_matViewport3D, rtX0, rtY0, rtWidth, rtHeight, viewport.Left, viewport.Top, viewport.Width, viewport.Height);
	if (m_applyHalfPixelOffset)
	{
		m_matViewport3D.m03 -= 1.0f / (float)rtWidth;
		m_matViewport3D.m13 += 1.0f / (float)rtHeight;
	}

#if !defined(_RELEASE)
	m_matViewport.m33 = ms_sys_flash_debugdraw_depth;
	m_matViewport3D.m33 = ms_sys_flash_debugdraw_depth;
#endif

	uint32 nReverseDepthEnabled = 0;
	m_pRenderer->EF_Query(EFQ_ReverseDepthEnabled, nReverseDepthEnabled);

	if (nReverseDepthEnabled)
	{
		Matrix44 matReverseDepth(IDENTITY);
		matReverseDepth.m22 = -1.0f;
		matReverseDepth.m23 = 1.0f;

		m_matViewport = matReverseDepth * m_matViewport;
		m_matViewport3D = matReverseDepth * m_matViewport3D;
	}

	m_pMatWorld3D = 0;

	// Scissors
	{
		int scX0(rtX0), scY0(rtY0), scX1(rtX0 + rtWidth), scY1(rtY0 + rtHeight);
		if (scX0 < scissor.Left)
			scX0 = scissor.Left;
		if (scY0 < scissor.Top)
			scY0 = scissor.Top;
		if (scX1 > scissor.Left + scissor.Width)
			scX1 = scissor.Left + scissor.Width;
		if (scY1 > scissor.Top + scissor.Height)
			scY1 = scissor.Top + scissor.Height;

		params.scissorEnabled = bScissor;
		params.scissor = SSF_GlobalDrawParams::_D3DRectangle(scX0, scY0, scX1, scY1);
	}

	// Viewports
	{
		int vpL(rtX0), vpT(rtY0), vpW(rtWidth), vpH(rtHeight);
		if (vpL < viewport.Left)
			vpL = viewport.Left;
		if (vpT < viewport.Top)
			vpT = viewport.Top;
		if (vpW > viewport.Width)
			vpW = viewport.Width;
		if (vpH > viewport.Height)
			vpH = viewport.Height;

		params.viewport = SSF_GlobalDrawParams::_D3DViewPort(float(vpL), float(vpT), float(vpW), float(vpH));
	}

	params.blendModeStates = 0;
	params.renderMaskedStates = 0;
	m_stencilCounter = 0;

	m_blendOpStack.resize(0);
	ApplyBlendMode(Blend_None);
	PushOutputTarget(Viewport(rtX0, rtY0, rtWidth, rtHeight));

	// Virtual canvas (coord-system relative to viewport)
	if (!m_extendCanvasToVP)
	{
		m_canvasRect = x0x1y0y1;
	}
	else
	{
		const float x0vp = (-1.0f - m_matViewport.m03) / m_matViewport.m00;
		const float y0vp = ( 1.0f - m_matViewport.m13) / m_matViewport.m11;
		const float x1vp = ( 1.0f - m_matViewport.m03) / m_matViewport.m00;
		const float y1vp = (-1.0f - m_matViewport.m13) / m_matViewport.m11;

		m_canvasRect = RectF(x0vp, y0vp, x1vp, y1vp);
	}

	m_rsIdx = 0;
	m_pRenderer->EF_Query(EFQ_RenderThreadList, m_rsIdx);
	assert(m_rsIdx < CRY_ARRAY_COUNT(m_renderStats));

	// Make parallax resolution independent
	m_maxParallaxScene *= (float)rtWidth / (float)viewport.Width;

	Clear(!ms_sys_flash_debugdraw ? backgroundColor : ColorF(0, 255, 0, 255));
}

void CScaleformPlayback::EndDisplay()
{
	assert(m_avoidStencilClear || !m_stencilCounter);

	PopOutputTarget();

	CRY_ASSERT(m_pRenderOutput);

#if 0
	// NOTE: this is a hacky workaround for all render-target stack pushed that specify no CTexture
	if (auto* pDC = m_pRenderer->GetActiveDisplayContext())
	{
		if (m_pRenderOutput == pDC->GetRenderOutput() && pDC->IsNativeScalingEnabled())
		{
			m_pRenderer->GetGraphicsPipeline().m_UpscalePass->Execute(m_pRenderOutput->GetColorTarget(), pDC->GetCurrentBackBuffer());
		}
	}
#endif

	m_pRenderOutput->EndRendering(nullptr);
	m_pRenderOutput = nullptr;
}

void CScaleformPlayback::SetMatrix(const Matrix23& m)
{
	m_mat              = m;
	m_matCached2DDirty = true;
	m_matCached3DDirty = true;
}

void CScaleformPlayback::SetUserMatrix(const Matrix23& /*m*/)
{
	assert(0 && "CScaleformPlayback::SetUserMatrix() -- Not implemented!");
}

void CScaleformPlayback::SetCxform(const ColorF& cx0, const ColorF& cx1)
{
	m_cxform[0] = cx0;
	m_cxform[1] = cx1;
}

void CScaleformPlayback::PushBlendMode(BlendType mode)
{
	m_blendOpStack.push_back(m_curBlendMode);

	if ((mode > Blend_Layer) && (m_curBlendMode != mode))
	{
		ApplyBlendMode(mode);
	}
}

void CScaleformPlayback::PopBlendMode()
{
	assert(m_blendOpStack.size() && "CScaleformPlayback::PopBlendMode() -- Blend mode stack is empty!");

	if (m_blendOpStack.size())
	{
		BlendType newBlendMode(Blend_None);

		for (int i(m_blendOpStack.size() - 1); i >= 0; --i)
		{
			if (m_blendOpStack[i] > Blend_Layer)
			{
				newBlendMode = m_blendOpStack[i];
				break;
			}
		}

		m_blendOpStack.pop_back();

		if (newBlendMode != m_curBlendMode)
		{
			ApplyBlendMode(newBlendMode);
		}
	}
}

void CScaleformPlayback::SetPerspective3D(const Matrix44& projMatIn)
{
	m_matProj3D         = projMatIn;
	m_matCachedWVPDirty = true;
	m_matCached3DDirty  = true;
}

void CScaleformPlayback::SetView3D(const Matrix44& viewMatIn)
{
	m_matView3D         = viewMatIn;
	m_matCachedWVPDirty = true;
	m_matCached3DDirty  = true;

	m_stereo3DiBaseDepth = -m_matView3D.m32;
	assert(m_stereo3DiBaseDepth >= 0);
}

void CScaleformPlayback::SetWorld3D(const Matrix44f* pWorldMatIn)
{
	m_pMatWorld3D       = pWorldMatIn;
	m_matCachedWVPDirty = pWorldMatIn != 0;
	m_matCached3DDirty  = pWorldMatIn != 0;
}

void CScaleformPlayback::SetVertexData(const DeviceData* pVertices)
{
	m_drawParams.vtxData = pVertices;
}

void CScaleformPlayback::SetIndexData(const DeviceData* pIndices)
{
	m_drawParams.idxData = pIndices;
}

void CScaleformPlayback::DrawIndexedTriList(int baseVertexIndex, int minVertexIndex, int numVertices, int startIndex, int triangleCount)
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	SSF_GlobalDrawParams& __restrict params = m_drawParams;
	IF ((m_renderMasked && !m_stencilAvail) || params.vtxData->VertexFormat == Vertex_None || params.idxData->IndexFormat == Index_None, 0)
		return;

	// setup render parameters
	ApplyMatrix(&m_mat);

	// draw triangle list
	m_pRenderer->SF_DrawIndexedTriList(baseVertexIndex, minVertexIndex, numVertices, startIndex, triangleCount, params);

	// update stats
	m_renderStats[m_rsIdx].Triangles += triangleCount;
	++m_renderStats[m_rsIdx].Primitives;
}

void CScaleformPlayback::DrawLineStrip(int baseVertexIndex, int lineCount)
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	SSF_GlobalDrawParams& __restrict params = m_drawParams;
	IF ((m_renderMasked && !m_stencilAvail) || params.vtxData->VertexFormat != Vertex_XY16i, 0)
		return;

	// setup render parameters
	ApplyMatrix(&m_mat);

	// draw triangle list
	m_pRenderer->SF_DrawLineStrip(baseVertexIndex, lineCount, params);

	// update stats
	m_renderStats[m_rsIdx].Lines += lineCount;
	++m_renderStats[m_rsIdx].Primitives;
}

void CScaleformPlayback::LineStyleDisable()
{
}

void CScaleformPlayback::LineStyleColor(ColorF color)
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	m_drawParams.fillType = SSF_GlobalDrawParams::SolidColor;

	ApplyTextureInfo(0);
	ApplyTextureInfo(1);
	ApplyColor(color * m_cxform[0] + m_cxform[1]);
}

void CScaleformPlayback::FillStyleDisable()
{
}

void CScaleformPlayback::FillStyleColor(ColorF color)
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	m_drawParams.fillType = SSF_GlobalDrawParams::SolidColor;

	ApplyTextureInfo(0);
	ApplyTextureInfo(1);
	ApplyColor(color * m_cxform[0] + m_cxform[1]);
}

void CScaleformPlayback::FillStyleBitmap(const FillTexture& Fill)
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	m_drawParams.fillType = SSF_GlobalDrawParams::Texture;

	ApplyTextureInfo(0, &Fill);
	ApplyTextureInfo(1, &Fill);
	ApplyCxform();
}

void CScaleformPlayback::FillStyleGouraud(GouraudFillType fillType, const FillTexture& Texture0, const FillTexture& Texture1, const FillTexture& Texture2)
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	static SSF_GlobalDrawParams::EFillType fillTypeLUT[] =
	{
		SSF_GlobalDrawParams::GColor,             // GFill_Color:
		SSF_GlobalDrawParams::G1Texture,		  // GFill_1Texture:
		SSF_GlobalDrawParams::G1TextureColor,	  // GFill_1TextureColor:
		SSF_GlobalDrawParams::G2Texture,		  // GFill_2Texture:
		SSF_GlobalDrawParams::G2TextureColor,	  // GFill_2TextureColor:
		SSF_GlobalDrawParams::G3Texture			  // GFill_3Texture:
	};

	m_drawParams.fillType = fillTypeLUT[fillType];

	ApplyTextureInfo(0, &Texture0);
	ApplyTextureInfo(1, &Texture1);
	ApplyCxform();
}

void CScaleformPlayback::DrawBitmaps(const DeviceData* pBitmaps, int startIndex, int count, ITexture* pTi, const Matrix23& m)
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SSF_GlobalDrawParams& __restrict params = m_drawParams;

	CTexture* pTex(static_cast<CTexture*>(pTi)); pTi->AddRef();

	params.texture[0].texState = SSF_GlobalDrawParams::TS_FilterLin | SSF_GlobalDrawParams::TS_Clamp;
	params.texture[0].pTex = pTex;

	// setup render parameters
	ApplyMatrix(&m);

	params.m_bScaleformMeshAttributesDirty = params.m_bScaleformMeshAttributesDirty ||
		(params.m_pScaleformMeshAttributes->cTexGenMat[0][0] != Vec4(1, 0, 0, 0)) ||
		(params.m_pScaleformMeshAttributes->cTexGenMat[0][1] != Vec4(0, 1, 0, 0));

	params.m_pScaleformMeshAttributes->cTexGenMat[0][0] = Vec4(1, 0, 0, 0);
	params.m_pScaleformMeshAttributes->cTexGenMat[0][1] = Vec4(0, 1, 0, 0);

	bool isYUV = pTex->GetSrcFormat() == eTF_YUV || pTex->GetSrcFormat() == eTF_YUVA;
	if (!isYUV)
	{
		params.fillType = pTex->GetSrcFormat() != eTF_A8 ? SSF_GlobalDrawParams::GlyphTexture : SSF_GlobalDrawParams::GlyphAlphaTexture;

		params.texture[1].pTex = 
		params.texture[2].pTex = 
		params.texture[3].pTex = TextureHelpers::LookupTexDefault(EFTT_DIFFUSE);

		params.m_bScaleformRenderParametersDirty = params.m_bScaleformRenderParametersDirty ||
			(params.m_pScaleformMeshAttributes->cStereoVideoFrameSelect != Vec2(1.0f, 0.0f));

		params.m_pScaleformMeshAttributes->cStereoVideoFrameSelect = Vec2(1.0f, 0.0f);
	}
	else
	{
		params.fillType = pTex->GetSrcFormat() == eTF_YUV ? SSF_GlobalDrawParams::GlyphTextureYUV : SSF_GlobalDrawParams::GlyphTextureYUVA;

		// Fetch circularly link YUVA planes (Y->U->V->A-> Y...)
		params.texture[1].pTex = static_cast<CTexture*>(m_pRenderer->EF_GetTextureByID(params.texture[0].pTex->GetCustomID()));
		params.texture[2].pTex = static_cast<CTexture*>(m_pRenderer->EF_GetTextureByID(params.texture[1].pTex->GetCustomID()));
		params.texture[3].pTex = static_cast<CTexture*>(m_pRenderer->EF_GetTextureByID(params.texture[2].pTex->GetCustomID()));

		Vec2 texGenYUVAStereo(0.0f, 0.0f);
		if (false /*pTexYUV->IsStereoContent()*/)
		{
			if (m_isStereo)
				texGenYUVAStereo = Vec2(m_isLeft ? 2.0f / 3.0f : 1.0f / 3.0f, m_isLeft ? 0.0f : 2.0f / 3.0f);
			else
				texGenYUVAStereo = Vec2(2.0f / 3.0f, 0.0f);
		}
		else
			texGenYUVAStereo = Vec2(1.0f, 0.0f);

		params.m_bScaleformRenderParametersDirty = params.m_bScaleformRenderParametersDirty ||
			(params.m_pScaleformMeshAttributes->cStereoVideoFrameSelect != texGenYUVAStereo);

		params.m_pScaleformMeshAttributes->cStereoVideoFrameSelect = texGenYUVAStereo;
	}

//	ApplyTextureInfo(0);
//	ApplyTextureInfo(1);
	ApplyCxform();

	// draw glyphs
	m_pRenderer->SF_DrawGlyphClear(pBitmaps, startIndex * 6, params);

	pTi->Release();

	// update stats
	m_renderStats[m_rsIdx].Triangles += count * 6;
	++m_renderStats[m_rsIdx].Primitives;
}

void CScaleformPlayback::BeginSubmitMask(SubmitMaskMode maskMode)
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	m_renderMasked = true;
	IF(!m_stencilAvail || ms_sys_flash_debugdraw == 2, 0)
	return;

	m_drawParams.pRenderOutput->bStencilTargetClear = m_drawParams.pRenderOutput->pStencilTarget && (!m_avoidStencilClear) && (maskMode == Mask_Clear);

	switch (maskMode)
	{
	case Mask_Clear:
		if (!m_avoidStencilClear)
		{
			m_stencilCounter = 1;
			ApplyStencilMask(SSF_GlobalDrawParams::BeginSubmitMask_Clear, m_stencilCounter);
		}
		else
		{
			++m_stencilCounter;
			ApplyStencilMask(SSF_GlobalDrawParams::BeginSubmitMask_Clear, m_stencilCounter);
		}
		break;
	case Mask_Increment:
		ApplyStencilMask(SSF_GlobalDrawParams::BeginSubmitMask_Inc, m_stencilCounter++);
		break;
	case Mask_Decrement:
		ApplyStencilMask(SSF_GlobalDrawParams::BeginSubmitMask_Dec, m_stencilCounter--);
		break;
	}

	++m_renderStats[m_rsIdx].Masks;
}

void CScaleformPlayback::EndSubmitMask()
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	m_renderMasked = true;
	IF(!m_stencilAvail || ms_sys_flash_debugdraw == 2, 0)
	return;

	ApplyStencilMask(SSF_GlobalDrawParams::EndSubmitMask, m_stencilCounter);
}

void CScaleformPlayback::DisableMask()
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	m_renderMasked = false;
	IF(!m_stencilAvail || ms_sys_flash_debugdraw == 2, 0)
	return;

	m_drawParams.pRenderOutput->bStencilTargetClear = m_drawParams.pRenderOutput->pStencilTarget && false;

	if (!m_avoidStencilClear)
		m_stencilCounter = 0;

	ApplyStencilMask(SSF_GlobalDrawParams::DisableMask, 0);
}

void CScaleformPlayback::DrawBlurRect(ITexture* _pSrcIn, const RectF& inSrcRect, const RectF& inDestRect, const BlurFilterParams& filter, bool islast)
{
	#ifdef ENABLE_FLASH_FILTERS
	SSF_GlobalDrawParams& __restrict params = m_drawParams;

	//FIXME 3d matrix screws up blur, should this be necessary?
	const Matrix44f* pMat3D_Cached = m_pMatWorld3D;
	SetWorld3D(NULL);

	// Flash can call in-to-out cases
	ITexture* pSrcIn = _pSrcIn; _pSrcIn->AddRef();
	if (pSrcIn == params.pRenderOutput->pRenderTarget)
	{
		// TODO: Don't allocate a RenderTarget here, even though there might be a chance that it clones the surface with color-compression
		CTexture* pDupl = m_pRenderer->SF_GetResources().GetColorSurface(m_pRenderer, pSrcIn->GetWidth(), pSrcIn->GetHeight(), pSrcIn->GetTextureDstFormat(), pSrcIn->GetWidth(), pSrcIn->GetHeight());
		CTexture* pRecl = static_cast<CTexture*>(_pSrcIn);

		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(pRecl->GetDevTexture(), pDupl->GetDevTexture());

		pSrcIn = pDupl;
		pSrcIn->AddRef();
	}

	ITexture* psrc     = pSrcIn;
	ITexture* pnextsrc = NULL;
	RectF srcrect, destrect = { -1, -1, 1, 1 };

	uint32 numPasses = filter.Passes;

	BlurFilterParams pass[3];
	SSF_GlobalDrawParams::EBlurType passis[3];

	pass[0] = filter;
	pass[1] = filter;
	pass[2] = filter;

	bool mul = (m_curBlendMode == Blend_Multiply || m_curBlendMode == Blend_Darken);
	passis[0] = passis[1] = SSF_GlobalDrawParams::Box2Blur;
	passis[2] = mul ? SSF_GlobalDrawParams::Box2BlurMul : SSF_GlobalDrawParams::Box2Blur;

	params.blendOp   = SSF_GlobalDrawParams::Add;

	if (filter.Mode & Filter_Shadow)
	{
		if (filter.Mode & Filter_HideObject)
		{
			passis[2] = SSF_GlobalDrawParams::Box2Shadowonly;

			if (filter.Mode & Filter_Highlight)
				passis[2] = (SSF_GlobalDrawParams::EBlurType) (passis[2] + SSF_GlobalDrawParams::shadows_Highlight);
		}
		else
		{
			if (filter.Mode & Filter_Inner)
				passis[2] = SSF_GlobalDrawParams::Box2InnerShadow;
			else
				passis[2] = SSF_GlobalDrawParams::Box2Shadow;

			if (filter.Mode & Filter_Knockout)
				passis[2] = (SSF_GlobalDrawParams::EBlurType) (passis[2] + SSF_GlobalDrawParams::shadows_Knockout);

			if (filter.Mode & Filter_Highlight)
				passis[2] = (SSF_GlobalDrawParams::EBlurType) (passis[2] + SSF_GlobalDrawParams::shadows_Highlight);
		}

		if (mul)
			passis[2] = (SSF_GlobalDrawParams::EBlurType) (passis[2] + SSF_GlobalDrawParams::shadows_Mul);
	}

	if (filter.BlurX * filter.BlurY > 32)
	{
		numPasses *= 2;

		pass[0].BlurY = 1;
		pass[1].BlurX = 1;
		pass[2].BlurX = 1;

		passis[0] = passis[1] = SSF_GlobalDrawParams::Box1Blur;
		if (passis[2] == SSF_GlobalDrawParams::Box2Blur)
			passis[2] = SSF_GlobalDrawParams::Box1Blur;
		else if (passis[2] == SSF_GlobalDrawParams::Box2BlurMul)
			passis[2] = SSF_GlobalDrawParams::Box1BlurMul;
	}

	uint32 bufWidth  = (uint32)ceilf(inSrcRect.Width());
	uint32 bufHeight = (uint32)ceilf(inSrcRect.Height());
	uint32 lastPass  = numPasses - 1;

	for (uint32 i = 0; i < numPasses; i++)
	{
		uint32 passi = (i == lastPass) ? 2 : (i & 1);
		const BlurFilterParams& pparams = pass[passi];
		params.blurType = passis[passi];

		ITexture* pSrcITex = psrc;
		int texWidth       = pSrcITex->GetWidth();
		int texHeight      = pSrcITex->GetHeight();

		psrc->AddRef();
		pSrcIn->AddRef();

		FillTexture fillTexture;
		fillTexture.pTexture = psrc;
		fillTexture.WrapMode = Wrap_Clamp;
		fillTexture.SampleMode = Sample_Linear;

		FillTexture sourceTexture;
		sourceTexture.pTexture = pSrcIn;
		sourceTexture.WrapMode = Wrap_Clamp;
		sourceTexture.SampleMode = Sample_Linear;

		ApplyTextureInfo(0, &fillTexture);
		ApplyTextureInfo(1, &sourceTexture);

		if (i != lastPass)
		{
			pnextsrc = static_cast<CTexture*>(m_pRenderer->EF_GetTextureByID(PushTempRenderTarget(RectF(-1, -1, 1, 1), bufWidth, bufHeight, true, false)));
			ApplyMatrix(&Matrix23::Identity());
			destrect = { -1, -1, 1, 1 };
		}
		else
		{
			pnextsrc = NULL;
			ApplyMatrix(&m_mat);
			destrect = inDestRect;
		}

		// not sure why this must set always to Add, but if set to current blendmode in last pass it will darken the results...
		//    if (i < last)
		{
			params.blendModeStates = GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA;
			params.blendOp         = SSF_GlobalDrawParams::Add;
		}
		//    else
		//    {
		//      ApplyBlendMode(m_curBlendMode);
		//    }

		const float mult = 1.0f / 255.0f;

		//set blend state
		{
			if (i == lastPass)
			{
				const float cxformData[4 * 2] =
				{
					filter.cxform[0][0] * filter.cxform[3][0],
					filter.cxform[1][0] * filter.cxform[3][0],
					filter.cxform[2][0] * filter.cxform[3][0],
					filter.cxform[3][0],

					filter.cxform[0][1] * mult * filter.cxform[3][0],
					filter.cxform[1][1] * mult * filter.cxform[3][0],
					filter.cxform[2][1] * mult * filter.cxform[3][0],
					filter.cxform[3][1] * mult
				};

				params.m_pScaleformRenderParameters->cBitmapColorTransform[0] = ColorF(cxformData[0], cxformData[1], cxformData[2], cxformData[3]);
				params.m_pScaleformRenderParameters->cBitmapColorTransform[1] = ColorF(cxformData[4], cxformData[5], cxformData[6], cxformData[7]);
			}
		//	else
			{
				params.m_pScaleformRenderParameters->cBitmapColorTransform[0] = ColorF(1, 1, 1, 1);
				params.m_pScaleformRenderParameters->cBitmapColorTransform[1] = ColorF(0, 0, 0, 0);
			}
		}

		float SizeX = uint(pparams.BlurX - 1) * 0.5f;
		float SizeY = uint(pparams.BlurY - 1) * 0.5f;

		if (passis[passi] == SSF_GlobalDrawParams::Box1Blur || passis[passi] == SSF_GlobalDrawParams::Box1BlurMul)
		{
			const float fsize[] = { pparams.BlurX > 1 ? SizeX : SizeY, 0, 1.0f / ((SizeX * 2 + 1) * (SizeY * 2 + 1)) };

			params.m_pScaleformRenderParameters->cBlurFilterSize = Vec3(fsize[0], fsize[1], fsize[2]);
			params.m_pScaleformRenderParameters->cBlurFilterScale = Vec2(pparams.BlurX > 1 ? 1.0f / texWidth : 0.f, pparams.BlurY > 1 ? 1.0f / texHeight : 0.f);
		}
		else
		{
			const float fsize[] = { SizeX, SizeY, 1.0f / ((SizeX * 2 + 1) * (SizeY * 2 + 1)) };

			params.m_pScaleformRenderParameters->cBlurFilterSize = Vec3(fsize[0], fsize[1], fsize[2]);
			params.m_pScaleformRenderParameters->cBlurFilterScale = Vec2(1.0f / texWidth, 1.0f / texHeight);
		}

		{
			params.m_pScaleformRenderParameters->cBlurFilterBias = Vec2(-filter.Offset.x, -filter.Offset.y);
		}

		{
			params.m_pScaleformRenderParameters->cBlurFilterColor1 = ColorF(filter.Color.r * mult, filter.Color.g * mult, filter.Color.b * mult, filter.Color.a * mult);
			params.m_pScaleformRenderParameters->cBlurFilterColor2 = ColorF(filter.Color2.r * mult, filter.Color2.g * mult, filter.Color2.b * mult, filter.Color2.a * mult);
		}

		params.m_bScaleformRenderParametersDirty = true;

		// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
		PREFAST_SUPPRESS_WARNING(6263)
		CryStackAllocWithSize(BitmapDesc, Rect, CDeviceBufferManager::AlignBufferSizeForStreaming);

		srcrect =
		{
			inSrcRect.Left  * 1.0f / texWidth, inSrcRect.Top    * 1.0f / texHeight,
			inSrcRect.Right * 1.0f / texWidth, inSrcRect.Bottom * 1.0f / texHeight
		};

		Rect->Coords = destrect;
		Rect->TextureCoords = srcrect;
		Rect->Color = 0;

		const DeviceData* pData = CreateDeviceData(Rect, 1, true);

		m_pRenderer->SF_DrawBlurRect(pData, params);

		ReleaseDeviceData(const_cast<DeviceData*>(pData));

		psrc->Release();
		pSrcIn->Release();

		if (i != lastPass)
		{
			PopRenderTarget();
			psrc = pnextsrc;
		}
	}

	SetWorld3D(pMat3D_Cached);
	m_renderStats[m_rsIdx].Filters += numPasses;

	//restore blend mode
	ApplyBlendMode(m_curBlendMode);
	ApplyTextureInfo(0);
	ApplyTextureInfo(1);

	pSrcIn->Release();
	if (pSrcIn != _pSrcIn)
		_pSrcIn->Release();
	#endif
}

void CScaleformPlayback::DrawColorMatrixRect(ITexture* pSrcIn, const RectF& inSrcRect, const RectF& inDestRect, const float* pMatrix, bool islast)
{
	#ifdef ENABLE_FLASH_FILTERS
	SSF_GlobalDrawParams& __restrict params = m_drawParams;

	pSrcIn->AddRef();

	FillTexture fillTexture;
	fillTexture.pTexture = pSrcIn;
	fillTexture.WrapMode = Wrap_Clamp;
	fillTexture.SampleMode = Sample_Linear;

	params.m_pScaleformRenderParameters->cColorTransformMat.SetColumn4(0, ColorF(pMatrix[ 0], pMatrix[ 1], pMatrix[ 2], pMatrix[ 3]).toVec4());
	params.m_pScaleformRenderParameters->cColorTransformMat.SetColumn4(1, ColorF(pMatrix[ 4], pMatrix[ 5], pMatrix[ 6], pMatrix[ 7]).toVec4());
	params.m_pScaleformRenderParameters->cColorTransformMat.SetColumn4(2, ColorF(pMatrix[ 8], pMatrix[ 9], pMatrix[10], pMatrix[11]).toVec4());
	params.m_pScaleformRenderParameters->cColorTransformMat.SetColumn4(3, ColorF(pMatrix[12], pMatrix[13], pMatrix[14], pMatrix[15]).toVec4());

	params.m_pScaleformRenderParameters->cBitmapColorTransform[1]       = ColorF(pMatrix[16], pMatrix[17], pMatrix[18], pMatrix[19]);
	params.m_bScaleformRenderParametersDirty = true;

	params.blendModeStates = GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA;
	params.blendOp         = SSF_GlobalDrawParams::Add;

	if (m_curBlendMode == Blend_Multiply || m_curBlendMode == Blend_Darken)
		params.fillType = SSF_GlobalDrawParams::GlyphTextureMatMul;
	else
		params.fillType = SSF_GlobalDrawParams::GlyphTextureMat;

	ITexture* pSrcITex    = pSrcIn;
	int texWidth  = pSrcITex->GetWidth();
	int texHeight = pSrcITex->GetHeight();

	ApplyMatrix(&m_mat);
	ApplyTextureInfo(0, &fillTexture);
//	ApplyTextureInfo(1);

	// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
	CryStackAllocWithSize(BitmapDesc, Rect, CDeviceBufferManager::AlignBufferSizeForStreaming);

	RectF srcrect =
	{
		inSrcRect.Left  * 1.0f / texWidth, inSrcRect.Top    * 1.0f / texHeight,
		inSrcRect.Right * 1.0f / texWidth, inSrcRect.Bottom * 1.0f / texHeight
	};

	Rect->Coords = inDestRect;
	Rect->TextureCoords = srcrect;
	Rect->Color = 0xffffffff;

	const DeviceData* pBitmap = CreateDeviceData(Rect, 1, true);

	m_pRenderer->SF_DrawGlyphClear(pBitmap, 0, params);

	ReleaseDeviceData(const_cast<DeviceData*>(pBitmap));

	//restore blend mode
	ApplyBlendMode(m_curBlendMode);
	ApplyTextureInfo(0);
//	ApplyTextureInfo(1);

	pSrcIn->Release();
#endif
}

void CScaleformPlayback::ReleaseResources()
{
	m_renderTargetStack.clear();
}

//////////////////////////////////////////////////////////////////////////////////////////
// IScaleformPlayback interface

void CScaleformPlayback::SetClearFlags(uint32 clearFlags, ColorF clearColor)
{
	m_clearFlags = clearFlags;
	m_clearColor = clearColor;
}

void CScaleformPlayback::SetCompositingDepth(float depth)
{
	m_compDepth = (depth >= 0 && depth <= 1) ? depth : 0;
}

void CScaleformPlayback::SetStereoMode(bool stereo, bool isLeft)
{
	m_isStereo = stereo;
	m_isLeft = isLeft;
}

void CScaleformPlayback::StereoEnforceFixedProjectionDepth(bool enforce)
{
	m_stereoFixedProjDepth = enforce;
}

void CScaleformPlayback::StereoSetCustomMaxParallax(float maxParallax)
{
	m_customMaxParallax = maxParallax;
}

void CScaleformPlayback::AvoidStencilClear(bool avoid)
{
	m_avoidStencilClear = avoid;
}

void CScaleformPlayback::EnableMaskedRendering(bool enable)
{
	m_maskedRendering = enable;
}

void CScaleformPlayback::ExtendCanvasToViewport(bool extend)
{
	m_extendCanvasToVP = extend;
}

void CScaleformPlayback::SetThreadIDs(uint32 mainThreadID, uint32 renderThreadID)
{
	m_mainThreadID = mainThreadID;
	m_renderThreadID = renderThreadID;
	assert(IsMainThread());
}

bool CScaleformPlayback::IsMainThread() const
{
	uint32 threadID = GetCurrentThreadId();
	return threadID == m_mainThreadID;
}

bool CScaleformPlayback::IsRenderThread() const
{
	uint32 threadID = GetCurrentThreadId();
	return threadID == m_renderThreadID;
}

std::vector<ITexture*> CScaleformPlayback::GetTempRenderTargets() const
{
	std::vector<ITexture*> textures;
	for (std::list<SSF_GlobalDrawParams::OutputParams>::const_iterator it = m_renderTargetStack.begin(); it != m_renderTargetStack.end(); ++it)
	{
		textures.push_back(it->pRenderTarget);
	}
	return textures;
}

void CScaleformPlayback::InitCVars()
{
	#if defined(_DEBUG)
	{
		static bool s_init = false;
		assert(!s_init);
		s_init = true;
	}
	#endif

	DefineConstIntCVar3("sys_flash_debugdraw", ms_sys_flash_debugdraw, sys_flash_debugdraw_DefVal, VF_CHEAT | VF_CHEAT_NOCHECK, "Enables debug drawing of Flash assets (1). Canvas area is drawn in bright green.\nAlso draw masks (2).");
	REGISTER_CVAR2("sys_flash_stereo_maxparallax", &ms_sys_flash_stereo_maxparallax, ms_sys_flash_stereo_maxparallax, VF_NULL, "Maximum parallax when viewing Flash content in stereo 3D");
	#if !defined(_RELEASE)
	REGISTER_CVAR2("sys_flash_debugdraw_depth", &ms_sys_flash_debugdraw_depth, ms_sys_flash_debugdraw_depth, VF_CHEAT | VF_CHEAT_NOCHECK, "Projects Flash assets to a certain depth to be able to zoom in/out (default is 1.0)");
	#endif
}

void CScaleformPlayback::SetRenderOutput(std::shared_ptr<CRenderOutput> pRenderOutput)
{
	m_pRenderOutput = pRenderOutput;
}

std::shared_ptr<CRenderOutput> CScaleformPlayback::GetRenderOutput() const
{
	return m_pRenderOutput;
}

void CScaleformPlayback::RenderFlashPlayerToTexture(IFlashPlayer* pFlashPlayer, CTexture* pOutput)
{
	CRenderOutputPtr pTempRenderOutput = std::make_shared<CRenderOutput>(pOutput, FRT_CLEAR_COLOR /* no automatic clears */, Clr_Transparent, 0.0f);

	int x, y, width, height; float aspect;
	pFlashPlayer->GetViewport(x, y, width, height, aspect);

	pTempRenderOutput->SetViewport(SRenderViewport(x, y, width, height));

	RenderFlashPlayerToOutput(pFlashPlayer, pTempRenderOutput);
}

void CScaleformPlayback::RenderFlashPlayerToOutput(IFlashPlayer* pFlashPlayer, const std::shared_ptr<CRenderOutput> &output)
{
	CScaleformPlayback* pScaleformPlayback = static_cast<CScaleformPlayback*>(pFlashPlayer->GetPlayback());
	CRY_ASSERT(pScaleformPlayback->IsRenderThread());

	// Remember currently used Render Target
	std::shared_ptr<CRenderOutput> pLastOutput = pScaleformPlayback->GetRenderOutput();
	pScaleformPlayback->SetRenderOutput(output);

	pFlashPlayer->Render();

	// Restore previous Flash render output.
	pScaleformPlayback->SetRenderOutput(pLastOutput);
}

#endif // #if RENDERER_SUPPORT_SCALEFORM
