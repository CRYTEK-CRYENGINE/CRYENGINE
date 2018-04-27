// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#ifdef INCLUDE_SCALEFORM_SDK
#include "GTexture_Impl.h"
#include "ScaleformRecording.h"

#include <CryRenderer/IScaleform.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IStereoRenderer.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>

// Verify assumptions about types being exchangeable made in this file.
// If one of these fails to compile, the behavior of the code at runtime is likely undefined.
static_assert(sizeof(uint32) == sizeof(UInt32), "Bad UInt32 size used");
static_assert(sizeof(uint64) == sizeof(UInt64), "Bad UInt64 size used");
static_assert(sizeof(IScaleformPlayback::RectF) == sizeof(GRectF), "Bad RectF size used");
static_assert(sizeof(IScaleformPlayback::Matrix23) == sizeof(GMatrix2D), "Bad Matrix23 size used");
static_assert(sizeof(IScaleformPlayback::BlendType) == sizeof(GRenderer::BlendType), "Bad BlendType size used");
static_assert(sizeof(Matrix44) == sizeof(GMatrix3D), "Bad Matrix44 size used");
static_assert(sizeof(IScaleformPlayback::VertexFormat) == sizeof(GRenderer::VertexFormat), "Bad VertexFormat used");
static_assert(sizeof(IScaleformPlayback::IndexFormat) == sizeof(GRenderer::IndexFormat), "Bad IndexFormat used");
static_assert(sizeof(IScaleformPlayback::BitmapDesc) == sizeof(GRenderer::BitmapDesc), "Bad BitmapDesc used");
static_assert(sizeof(IScaleformPlayback::SubmitMaskMode) == sizeof(GRenderer::SubmitMaskMode), "Bad SubmitMaskMode used");

static_assert(uint32(IScaleformPlayback::VertexFormat::Vertex_None) == uint32(GRenderer::VertexFormat::Vertex_None), "Bad VertexFormat index for Vertex_None");
static_assert(uint32(IScaleformPlayback::VertexFormat::Vertex_XY16i) == uint32(GRenderer::VertexFormat::Vertex_XY16i), "Bad VertexFormat index for Vertex_XY16i");
static_assert(uint32(IScaleformPlayback::VertexFormat::Vertex_XY32f) == uint32(GRenderer::VertexFormat::Vertex_XY32f), "Bad VertexFormat index for Vertex_XY32f");
static_assert(uint32(IScaleformPlayback::VertexFormat::Vertex_XY16iC32) == uint32(GRenderer::VertexFormat::Vertex_XY16iC32), "Bad VertexFormat index for Vertex_XY16iC32");
static_assert(uint32(IScaleformPlayback::VertexFormat::Vertex_XY16iCF32) == uint32(GRenderer::VertexFormat::Vertex_XY16iCF32), "Bad VertexFormat index for Vertex_XY16iCF32");

#if defined(_DEBUG)

#define _RECORD_CMD_PREFIX			   \
	IF(m_pCmdBuf && !IsRenderThread(), 0)           \
	{								   \
		size_t startPos = (size_t) -1;

#define _RECORD_CMD(cmd)		  \
		assert(cmd != GRCBA_Nop);	  \
		m_pCmdBuf->Write(cmd);		  \
		startPos = m_pCmdBuf->Size(); \
		m_pCmdBuf->Write((size_t) -1);

#define _RECORD_CMD_ARG(arg) \
		m_pCmdBuf->Write(arg);

#define _RECORD_CMD_DATA(data, size) \
		m_pCmdBuf->Write(data, size);

#define _RECORD_CMD_POSTFIX(ret)							  \
		assert(startPos != -1);									  \
		m_pCmdBuf->Patch(startPos, m_pCmdBuf->Size() - startPos); \
		return ret;												  \
	}

#else   // #if defined(_DEBUG)

#define _RECORD_CMD_PREFIX			   \
	IF(m_pCmdBuf && !IsRenderThread(), 0) \
	{

#define _RECORD_CMD(cmd)   \
		m_pCmdBuf->Write(cmd); \

#define _RECORD_CMD_ARG(arg) \
		m_pCmdBuf->Write(arg);

#define _RECORD_CMD_DATA(data, size) \
		m_pCmdBuf->Write(data, size);

#define _RECORD_CMD_POSTFIX(ret) \
		return ret;					 \
	}

#endif   // #if defined(_DEBUG)

#define CMD_VOID_RETURN ((void) 0)

#define RECORD_CMD(cmd, ret) \
	_RECORD_CMD_PREFIX		 \
	_RECORD_CMD(cmd)		 \
	_RECORD_CMD_POSTFIX(ret)

#define RECORD_CMD_1ARG(cmd, ret, arg0)	\
	_RECORD_CMD_PREFIX					\
	_RECORD_CMD(cmd)					\
	_RECORD_CMD_ARG(arg0)				\
	_RECORD_CMD_POSTFIX(ret)

#define RECORD_CMD_2ARG(cmd, ret, arg0, arg1) \
	_RECORD_CMD_PREFIX						  \
	_RECORD_CMD(cmd)						  \
	_RECORD_CMD_ARG(arg0)					  \
	_RECORD_CMD_ARG(arg1)					  \
	_RECORD_CMD_POSTFIX(ret)

#define RECORD_CMD_3ARG(cmd, ret, arg0, arg1, arg2)	\
	_RECORD_CMD_PREFIX								\
	_RECORD_CMD(cmd)								\
	_RECORD_CMD_ARG(arg0)							\
	_RECORD_CMD_ARG(arg1)							\
	_RECORD_CMD_ARG(arg2)							\
	_RECORD_CMD_POSTFIX(ret)

#define RECORD_CMD_4ARG(cmd, ret, arg0, arg1, arg2, arg3) \
	_RECORD_CMD_PREFIX									  \
	_RECORD_CMD(cmd)									  \
	_RECORD_CMD_ARG(arg0)								  \
	_RECORD_CMD_ARG(arg1)								  \
	_RECORD_CMD_ARG(arg2)								  \
	_RECORD_CMD_ARG(arg3)								  \
	_RECORD_CMD_POSTFIX(ret)

#define RECORD_CMD_5ARG(cmd, ret, arg0, arg1, arg2, arg3, arg4)	\
	_RECORD_CMD_PREFIX											\
	_RECORD_CMD(cmd)											\
	_RECORD_CMD_ARG(arg0)										\
	_RECORD_CMD_ARG(arg1)										\
	_RECORD_CMD_ARG(arg2)										\
	_RECORD_CMD_ARG(arg3)										\
	_RECORD_CMD_ARG(arg4)										\
	_RECORD_CMD_POSTFIX(ret)

#define RECORD_CMD_6ARG(cmd, ret, arg0, arg1, arg2, arg3, arg4, arg5) \
	_RECORD_CMD_PREFIX												  \
	_RECORD_CMD(cmd)												  \
	_RECORD_CMD_ARG(arg0)											  \
	_RECORD_CMD_ARG(arg1)											  \
	_RECORD_CMD_ARG(arg2)											  \
	_RECORD_CMD_ARG(arg3)											  \
	_RECORD_CMD_ARG(arg4)											  \
	_RECORD_CMD_ARG(arg5)											  \
	_RECORD_CMD_POSTFIX(ret)

#define RECORD_CMD_7ARG(cmd, ret, arg0, arg1, arg2, arg3, arg4, arg5, arg6) \
	_RECORD_CMD_PREFIX												  \
	_RECORD_CMD(cmd)												  \
	_RECORD_CMD_ARG(arg0)											  \
	_RECORD_CMD_ARG(arg1)											  \
	_RECORD_CMD_ARG(arg2)											  \
	_RECORD_CMD_ARG(arg3)											  \
	_RECORD_CMD_ARG(arg4)											  \
	_RECORD_CMD_ARG(arg5)											  \
	_RECORD_CMD_ARG(arg6)											  \
	_RECORD_CMD_POSTFIX(ret)

//////////////////////////////////////////////////////////////////////////

volatile int CCachedDataStore::ms_numInst = 0;

CCachedDataStore* CCachedDataStore::Create(GRenderer* pRenderer, GRenderer::CacheProvider* pCache, GRenderer::CachedDataType type, IScaleformPlayback* pOwner, const IScaleformPlayback::DeviceData* pData)
{
	CCachedDataStore* pStore = 0;
	if (pCache)
	{
		GRenderer::CachedData* pCD = pCache->GetCachedData(pRenderer);
		if (pCD)
		{
			pStore = (CCachedDataStore*)pCD->GetRendererData();
			assert(pStore);
			assert(pStore->m_type == type && pStore->m_pData->OriginalDataPtr == pData->OriginalDataPtr); // data was previously cached with different args and not properly released yet via ReleaseCachedData()
		}
		else
		{
			pCD = pCache->CreateCachedData(type, pRenderer);
			assert(pCD);
			pStore = new CCachedDataStore(type, pOwner, pData);
			assert(pStore);
			pCD->SetRendererData(pStore);
		}
	}
	else
	{
		pStore = new CCachedDataStore(type, pOwner, pData);
		assert(pStore);
	}
	return pStore;
}

CCachedDataStore* CCachedDataStore::CreateOrReuse(GRenderer* pRenderer, GRenderer::CacheProvider* pCache, GRenderer::CachedDataType type, IScaleformPlayback* pOwner, const void* pVertices, int numVertices, GRenderer::VertexFormat _vf)
{
	// IScaleformPlayback::VertexFormat := VertexFormat
	const IScaleformPlayback::VertexFormat& vf = *((IScaleformPlayback::VertexFormat*)&_vf);

	GRenderer::CachedData* pData = nullptr;
	CCachedDataStore* pDataStore = nullptr;
	if (!pCache || !(pData = pCache->GetCachedData(pRenderer)) || !(pDataStore = reinterpret_cast<CCachedDataStore*>(pData->GetRendererData())))
		pDataStore = CCachedDataStore::Create(pRenderer, pCache, type, pOwner, pOwner->CreateDeviceData(pVertices, numVertices, vf));
	return pDataStore;
}

CCachedDataStore* CCachedDataStore::CreateOrReuse(GRenderer* pRenderer, GRenderer::CacheProvider* pCache, GRenderer::CachedDataType type, IScaleformPlayback* pOwner, const void* pIndices, int numIndices, GRenderer::IndexFormat _idxf)
{
	// IScaleformPlayback::IndexFormat := IndexFormat
	const IScaleformPlayback::IndexFormat& idxf = *((IScaleformPlayback::IndexFormat*)&_idxf);

	GRenderer::CachedData* pData = nullptr;
	CCachedDataStore* pDataStore = nullptr;
	if (!pCache || !(pData = pCache->GetCachedData(pRenderer)) || !(pDataStore = reinterpret_cast<CCachedDataStore*>(pData->GetRendererData())))
		pDataStore = CCachedDataStore::Create(pRenderer, pCache, type, pOwner, pOwner->CreateDeviceData(pIndices, numIndices, idxf));
	return pDataStore;
}

CCachedDataStore* CCachedDataStore::CreateOrReuse(GRenderer* pRenderer, GRenderer::CacheProvider* pCache, GRenderer::CachedDataType type, IScaleformPlayback* pOwner, const GRenderer::BitmapDesc* _pBitmapList, int numBitmaps)
{
	// IScaleformPlayback::BitmapDesc := BitmapDesc
	IScaleformPlayback::BitmapDesc* pBitmapList = (IScaleformPlayback::BitmapDesc*)_pBitmapList;

	GRenderer::CachedData* pData = nullptr;
	CCachedDataStore* pDataStore = nullptr;
	if (!pCache || !(pData = pCache->GetCachedData(pRenderer)) || !(pDataStore = reinterpret_cast<CCachedDataStore*>(pData->GetRendererData())))
		pDataStore = CCachedDataStore::Create(pRenderer, pCache, type, pOwner, pOwner->CreateDeviceData(pBitmapList, numBitmaps));
	return pDataStore;
}

void CScaleformRecording::ReleaseCachedData(CachedData* pData, CachedDataType /*type*/)
{
	//assert(IsMainThread()); // see notes in CCachedDataStore::BackupAndRelease()
	CCachedDataStore* pStore = (CCachedDataStore*)(pData ? pData->GetRendererData() : 0);
	if (pStore)
		pStore->BackupAndRelease();
}

//////////////////////////////////////////////////////////////////////////

CScaleformRecording::CScaleformRecording()
	: m_maxVertices(0)
	, m_maxIndices(0)
	, m_rsIdx(0)
	, m_renderStats()
	, m_mainThreadID(0)
	, m_renderThreadID(0)
	, m_pCmdBuf(0)
{
	m_pPlayback = gEnv->pRenderer->SF_CreatePlayback();

	m_renderStats[0].Clear();
	m_renderStats[1].Clear();

	gEnv->pRenderer->SF_GetMeshMaxSize(m_maxVertices, m_maxIndices);
	gEnv->pRenderer->GetThreadIDs(m_mainThreadID, m_renderThreadID);
}

CScaleformRecording::~CScaleformRecording()
{
}

bool CScaleformRecording::GetRenderCaps(RenderCaps* pCaps)
{
	UInt32 nonPow2 = Cap_TexNonPower2;
	pCaps->CapBits = Cap_Index16 | Cap_FillGouraud | Cap_FillGouraudTex | Cap_CxformAdd | Cap_NestedMasks | nonPow2;             // should we allow 32 bit indices if supported?
#ifdef ENABLE_FLASH_FILTERS
	pCaps->CapBits |= Cap_RenderTargetPrePass;
#endif
	pCaps->BlendModes     = (1 << Blend_None) | (1 << Blend_Normal) | (1 << Blend_Multiply) | (1 << Blend_Lighten) | (1 << Blend_Darken) | (1 << Blend_Add) | (1 << Blend_Subtract);
	pCaps->VertexFormats  = (1 << Vertex_None) | (1 << Vertex_XY16i) | (1 << Vertex_XY16iC32) | (1 << Vertex_XY16iCF32);
	pCaps->MaxTextureSize = gEnv->pRenderer->GetMaxTextureSize();
	return true;
}

GTexture* CScaleformRecording::CreateTexture()
{
	return new GTextureXRender(this);
}

GTexture* CScaleformRecording::CreateTextureYUV()
{
	return new GTextureXRenderYUV(this);
}

std::vector<ITexture*> CScaleformRecording::GetTempRenderTargets() const
{
	return GetPlayback()->GetTempRenderTargets();
}

GRenderTarget* CScaleformRecording::CreateRenderTarget()
{
	assert(0 && "CScaleformRecording::CreateRenderTarget() -- Not implemented!"); return 0;
}

void CScaleformRecording::SetDisplayRenderTarget(GRenderTarget* /*pRT*/, bool /*setState*/)
{
	assert(0 && "CScaleformRecording::SetDisplayRenderTarget() -- Not implemented!");
}

void CScaleformRecording::PushRenderTarget(const GRectF& /*frameRect*/, GRenderTarget* /*pRT*/)
{
	assert(0 && "CScaleformRecording::PushRenderTarget() -- Not implemented!");
}

void CScaleformRecording::PopRenderTarget()
{
#ifdef ENABLE_FLASH_FILTERS
	// These are ref-initialized with 1, they need to be released
	m_pTempRTsLL.top()->SetRT(nullptr);
	m_pTempRTsLL.top()->Release();
	m_pTempRTs.top()->Release();

	// pop: Iterators and references to the erased element are invalidated. It is unspecified whether the past-the-end iterator is invalidated. Other references and iterators are not affected. 
	m_pTempRTsLL.pop();
	m_pTempRTs.pop();

	_RECORD_CMD_PREFIX
	  _RECORD_CMD(GRCBA_PopRenderTarget)
	_RECORD_CMD_POSTFIX(CMD_VOID_RETURN)

	GetPlayback()->PopRenderTarget();
#endif
}

GTexture* CScaleformRecording::PushTempRenderTarget(const GRectF& _frameRect, UInt targetW, UInt targetH, bool wantStencil)
{
#ifdef ENABLE_FLASH_FILTERS
	GTextureXRenderTempRTLockless* pTempRTLL;
	GTextureXRenderTempRT* pTempRT;

	// IScaleformPlayback::RectF := GRectF
	const IScaleformPlayback::RectF& frameRect = *((IScaleformPlayback::RectF*)&_frameRect);

	// These are ref-counted, they need to be free objects
	pTempRTLL = new GTextureXRenderTempRTLockless(this);
	pTempRT = new GTextureXRenderTempRT(this, -1, eTF_R8G8B8A8);
	pTempRTLL->SetRT(pTempRT);

	// emplace: All iterators, including the past-the-end iterator, are invalidated. No references are invalidated. 
	m_pTempRTs.push(pTempRT);
	m_pTempRTsLL.push(pTempRTLL);

	_RECORD_CMD_PREFIX
	GTextureXRenderTempRTLockless* pTempRTLL = new GTextureXRenderTempRTLockless(this);
	GTextureXRenderTempRT* pTempRT = new GTextureXRenderTempRT(this, -1, eTF_R8G8B8A8);
	pTempRTLL->SetRT(pTempRT);
	_RECORD_CMD(GRCBA_PushTempRenderTarget)
	_RECORD_CMD_ARG(frameRect)
	_RECORD_CMD_ARG(targetW)
	_RECORD_CMD_ARG(targetH)
	_RECORD_CMD_ARG(wantStencil)
	_RECORD_CMD_ARG(pTempRT->GetIDPtr())
	_RECORD_CMD_POSTFIX(pTempRTLL)

	int32 TempIRT = GetPlayback()->PushTempRenderTarget(frameRect, targetW, targetH, true, wantStencil);
	pTempRT->InitTextureFromTexId(TempIRT);

	return pTempRT;
#endif

	return nullptr;
}

void CScaleformRecording::BeginDisplay(GColor _backgroundColor, const GViewport& _viewport, Float x0, Float x1, Float y0, Float y1)
{
	// IScaleformPlayback::Viewport := GViewport::LRTB
	const ColorF backgroundColor = ColorF(_backgroundColor.GetRed(), _backgroundColor.GetGreen(), _backgroundColor.GetBlue(), _backgroundColor.GetAlpha());
	const IScaleformPlayback::Viewport& viewport = *((IScaleformPlayback::Viewport*)&_viewport.Left);
	const IScaleformPlayback::Viewport& scissor = *((IScaleformPlayback::Viewport*)&_viewport.ScissorLeft);
	const bool bScissor = (_viewport.Flags & GViewport::View_UseScissorRect) != 0;
	const IScaleformPlayback::RectF x0x1y0y1 = { x0, y0, x1, y1 };

	RECORD_CMD_5ARG(GRCBA_BeginDisplay, CMD_VOID_RETURN, backgroundColor, viewport, bScissor, scissor, x0x1y0y1);

	GetPlayback()->BeginDisplay(backgroundColor, viewport, bScissor, scissor, x0x1y0y1);
}

void CScaleformRecording::EndDisplay()
{
	RECORD_CMD(GRCBA_EndDisplay, CMD_VOID_RETURN);

	GetPlayback()->EndDisplay();
}

void CScaleformRecording::SetMatrix(const GMatrix2D& _m)
{
	// Matrix23 := GMatrix2D
	const IScaleformPlayback::Matrix23& m = *((IScaleformPlayback::Matrix23*)&_m);

	RECORD_CMD_1ARG(GRCBA_SetMatrix, CMD_VOID_RETURN, m);

	GetPlayback()->SetMatrix(m);
}

void CScaleformRecording::SetUserMatrix(const GMatrix2D& /*m*/)
{
	assert(0 && "CScaleformRecording::SetUserMatrix() -- Not implemented!");
}

void CScaleformRecording::SetCxform(const Cxform& _cx)
{
	// 0=mul, 1=add
	ColorF cx[2];

	cx[0] = ColorF(_cx.M_[0][0], _cx.M_[1][0], _cx.M_[2][0], _cx.M_[3][0]);
	cx[1] = ColorF(_cx.M_[0][1], _cx.M_[1][1], _cx.M_[2][1], _cx.M_[3][1]);

	RECORD_CMD_2ARG(GRCBA_SetCxform, CMD_VOID_RETURN, cx[0], cx[1]);

	GetPlayback()->SetCxform(cx[0], cx[1]);
}

void CScaleformRecording::PushBlendMode(BlendType _mode)
{
	// BlendType := BlendType
	const IScaleformPlayback::BlendType& mode = *((IScaleformPlayback::BlendType*)&_mode);

	RECORD_CMD_1ARG(GRCBA_PushBlendMode, CMD_VOID_RETURN, mode);

	GetPlayback()->PushBlendMode(mode);
}

void CScaleformRecording::PopBlendMode()
{
	RECORD_CMD(GRCBA_PopBlendMode, CMD_VOID_RETURN);

	GetPlayback()->PopBlendMode();
}

void CScaleformRecording::SetPerspective3D(const GMatrix3D& _projMatIn)
{
	// Matrix44 := GMatrix3D
	const Matrix44f& projMatIn = *((Matrix44f*)&_projMatIn);

	RECORD_CMD_1ARG(GRCBA_SetPerspective3D, CMD_VOID_RETURN, projMatIn);

	GetPlayback()->SetPerspective3D(projMatIn);
}

void CScaleformRecording::SetView3D(const GMatrix3D& _viewMatIn)
{
	// Matrix44 := GMatrix3D
	const Matrix44f& viewMatIn = *((Matrix44f*)&_viewMatIn);

	RECORD_CMD_1ARG(GRCBA_SetView3D, CMD_VOID_RETURN, viewMatIn);

	GetPlayback()->SetView3D(viewMatIn);
}

void CScaleformRecording::SetWorld3D(const GMatrix3D* _pWorldMatIn)
{
	// Matrix44 := GMatrix3D
	const Matrix44f* pWorldMatIn = reinterpret_cast<const Matrix44f*>(_pWorldMatIn);

	_RECORD_CMD_PREFIX
	if (_pWorldMatIn)
	{
		_RECORD_CMD(GRCBA_SetWorld3D)
		_RECORD_CMD_ARG(pWorldMatIn)
	}
	else
	{
		_RECORD_CMD(GRCBA_SetWorld3D_NullPtr)
	}
	_RECORD_CMD_POSTFIX(CMD_VOID_RETURN)

	GetPlayback()->SetWorld3D(pWorldMatIn);
}

static inline size_t VertexSize(GRenderer::VertexFormat vf)
{
	switch (vf)
	{
	case GRenderer::Vertex_XY16i:
		return 4;
	case GRenderer::Vertex_XY32f:
		return 8;
	case GRenderer::Vertex_XY16iC32:
		return 8;
	case GRenderer::Vertex_XY16iCF32:
		return 12;
	case GRenderer::Vertex_None:
		return 0;

	default:
		assert(0);
		return 0;
	}
}

void CScaleformRecording::SetVertexData(const void* pVertices, int numVertices, VertexFormat _vf, CacheProvider* pCache)
{
	if (!pVertices) return;

	CCachedDataStore* pDataStore = CCachedDataStore::CreateOrReuse(this, pCache, Cached_Vertex, GetPlayback(), pVertices, numVertices, _vf);

	_RECORD_CMD_PREFIX
		_RECORD_CMD(GRCBA_SetVertexData)
		_RECORD_CMD_ARG(pDataStore) m_pCmdBuf->AddRefCountedObject(pDataStore); if (!pCache) pDataStore->Release();
	_RECORD_CMD_POSTFIX(CMD_VOID_RETURN)

	GetPlayback()->SetVertexData(pDataStore->GetPtr());

	if (!pCache)
	{
		m_pDataStore[0] = pDataStore;
		pDataStore->Release();
	}
		
}

static inline size_t IndexSize(GRenderer::IndexFormat idxf)
{
	switch (idxf)
	{
	case GRenderer::Index_16:
		return 2;
	case GRenderer::Index_32:
		return 4;
	case GRenderer::Index_None:
		return 0;

	default:
		assert(0);
		return 0;
	}
}

void CScaleformRecording::SetIndexData(const void* pIndices, int numIndices, IndexFormat _idxf, CacheProvider* pCache)
{
	if (!pIndices) return;

	CCachedDataStore* pDataStore = CCachedDataStore::CreateOrReuse(this, pCache, Cached_Index, GetPlayback(), pIndices, numIndices, _idxf);

	_RECORD_CMD_PREFIX
		_RECORD_CMD(GRCBA_SetIndexData)
		_RECORD_CMD_ARG(pDataStore) m_pCmdBuf->AddRefCountedObject(pDataStore); if (!pCache) pDataStore->Release();
	_RECORD_CMD_POSTFIX(CMD_VOID_RETURN)

	GetPlayback()->SetIndexData(pDataStore->GetPtr());

	if (!pCache)
	{
		m_pDataStore[1] = pDataStore;
		pDataStore->Release();
	}
}

void CScaleformRecording::DrawIndexedTriList(int baseVertexIndex, int minVertexIndex, int numVertices, int startIndex, int triangleCount)
{
	RECORD_CMD_5ARG(GRCBA_DrawIndexedTriList, CMD_VOID_RETURN, baseVertexIndex, minVertexIndex, numVertices, startIndex, triangleCount);

	GetPlayback()->DrawIndexedTriList(baseVertexIndex, minVertexIndex, numVertices, startIndex, triangleCount);
}

void CScaleformRecording::DrawLineStrip(int baseVertexIndex, int lineCount)
{
	RECORD_CMD_2ARG(GRCBA_DrawLineStrip, CMD_VOID_RETURN, baseVertexIndex, lineCount);

	GetPlayback()->DrawLineStrip(baseVertexIndex, lineCount);
}

void CScaleformRecording::LineStyleDisable()
{
	RECORD_CMD(GRCBA_LineStyleDisable, CMD_VOID_RETURN);

	GetPlayback()->LineStyleDisable();
}

void CScaleformRecording::LineStyleColor(GColor _color)
{
	const ColorF color = ColorF(_color.GetRed(), _color.GetGreen(), _color.GetBlue(), _color.GetAlpha());

	RECORD_CMD_1ARG(GRCBA_LineStyleColor, CMD_VOID_RETURN, color);

	GetPlayback()->LineStyleColor(color);
}

void CScaleformRecording::FillStyleDisable()
{
	RECORD_CMD(GRCBA_FillStyleDisable, CMD_VOID_RETURN);

	GetPlayback()->FillStyleDisable();
}

void CScaleformRecording::FillStyleColor(GColor _color)
{
	const ColorF color = ColorF(_color.GetRed(), _color.GetGreen(), _color.GetBlue(), _color.GetAlpha());

	RECORD_CMD_1ARG(GRCBA_FillStyleColor, CMD_VOID_RETURN, color);

	GetPlayback()->FillStyleColor(color);
}

void CScaleformRecording::FillStyleBitmap(const FillTexture* pFill)
{
	IScaleformPlayback::FillTexture Fill;

	if (!pFill)
		ZeroMemory(&Fill, sizeof(Fill));
	else
	{
		Fill.pTexture = gEnv->pRenderer->EF_GetTextureByID(static_cast<const GTextureXRenderBase*>(pFill->pTexture)->GetID());
		Fill.TextureMatrix = *((IScaleformPlayback::Matrix23*)&pFill->TextureMatrix);
		Fill.WrapMode = *((IScaleformPlayback::BitmapWrapMode*)&pFill->WrapMode);
		Fill.SampleMode = *((IScaleformPlayback::BitmapSampleMode*)&pFill->SampleMode);

		IF(m_pCmdBuf && !IsRenderThread(), 0)
		{
			m_pCmdBuf->AddRefCountedObject(pFill->pTexture);
		}
	}

	RECORD_CMD_1ARG(GRCBA_FillStyleBitmap, CMD_VOID_RETURN, Fill);

	GetPlayback()->FillStyleBitmap(Fill);
}

void CScaleformRecording::FillStyleGouraud(GouraudFillType _fillType, const FillTexture* pTexture0, const FillTexture* pTexture1, const FillTexture* pTexture2)
{
	// IScaleformPlayback::Matrix23 := GMatrix2D
	IScaleformPlayback::GouraudFillType fillType = *((IScaleformPlayback::GouraudFillType*)&_fillType);
	IScaleformPlayback::FillTexture Fill0;
	IScaleformPlayback::FillTexture Fill1;
	IScaleformPlayback::FillTexture Fill2;

	if (!pTexture0)
		ZeroMemory(&Fill0, sizeof(Fill0));
	else
	{
		Fill0.pTexture = gEnv->pRenderer->EF_GetTextureByID(static_cast<const GTextureXRenderBase*>(pTexture0->pTexture)->GetID());
		Fill0.TextureMatrix = *((IScaleformPlayback::Matrix23*)&pTexture0->TextureMatrix);
		Fill0.WrapMode = *((IScaleformPlayback::BitmapWrapMode*)&pTexture0->WrapMode);
		Fill0.SampleMode = *((IScaleformPlayback::BitmapSampleMode*)&pTexture0->SampleMode);

		IF(m_pCmdBuf && !IsRenderThread(), 0)
		{
			m_pCmdBuf->AddRefCountedObject(pTexture0->pTexture);
		}
	}

	if (!pTexture1)
		ZeroMemory(&Fill1, sizeof(Fill1));
	else
	{
		Fill1.pTexture = gEnv->pRenderer->EF_GetTextureByID(static_cast<const GTextureXRenderBase*>(pTexture1->pTexture)->GetID());
		Fill1.TextureMatrix = *((IScaleformPlayback::Matrix23*)&pTexture1->TextureMatrix);
		Fill1.WrapMode = *((IScaleformPlayback::BitmapWrapMode*)&pTexture1->WrapMode);
		Fill1.SampleMode = *((IScaleformPlayback::BitmapSampleMode*)&pTexture1->SampleMode);

		IF(m_pCmdBuf && !IsRenderThread(), 0)
		{
			m_pCmdBuf->AddRefCountedObject(pTexture1->pTexture);
		}
	}

	if (!pTexture2)
		ZeroMemory(&Fill2, sizeof(Fill2));
	else
	{
		Fill2.pTexture = gEnv->pRenderer->EF_GetTextureByID(static_cast<const GTextureXRenderBase*>(pTexture2->pTexture)->GetID());
		Fill2.TextureMatrix = *((IScaleformPlayback::Matrix23*)&pTexture2->TextureMatrix);
		Fill2.WrapMode = *((IScaleformPlayback::BitmapWrapMode*)&pTexture2->WrapMode);
		Fill2.SampleMode = *((IScaleformPlayback::BitmapSampleMode*)&pTexture2->SampleMode);

		IF(m_pCmdBuf && !IsRenderThread(), 0)
		{
			m_pCmdBuf->AddRefCountedObject(pTexture2->pTexture);
		}
	}

	RECORD_CMD_4ARG(GRCBA_FillStyleGouraud, CMD_VOID_RETURN, fillType, Fill0, Fill1, Fill2);

	GetPlayback()->FillStyleGouraud(fillType, Fill0, Fill1, Fill2);
}

void CScaleformRecording::DrawBitmaps(BitmapDesc* pBitmapList, int listSize, int startIndex, int count, const GTexture* _pTi, const GMatrix2D& _m, CacheProvider* pCache)
{
	if (!(pBitmapList && _pTi)) return;

	CCachedDataStore* pDataStore = CCachedDataStore::CreateOrReuse(this, pCache, Cached_BitmapList, GetPlayback(), pBitmapList, listSize);

	// IScaleformPlayback::Matrix23 := GMatrix2D
	const IScaleformPlayback::Matrix23& m = *((IScaleformPlayback::Matrix23*)&_m);
	ITexture* pTi = gEnv->pRenderer->EF_GetTextureByID(static_cast<const GTextureXRenderBase*>(_pTi)->GetID());

	_RECORD_CMD_PREFIX
		_RECORD_CMD(GRCBA_DrawBitmaps)
		_RECORD_CMD_ARG(pDataStore) m_pCmdBuf->AddRefCountedObject(pDataStore); if (!pCache) pDataStore->Release();
		_RECORD_CMD_ARG(listSize)
		_RECORD_CMD_ARG(startIndex)
		_RECORD_CMD_ARG(count)
		_RECORD_CMD_ARG(pTi) m_pCmdBuf->AddRefCountedObject(const_cast<GTexture*>(_pTi));
		_RECORD_CMD_ARG(m)
	_RECORD_CMD_POSTFIX(CMD_VOID_RETURN)

	GetPlayback()->DrawBitmaps(pDataStore->GetPtr(), startIndex, count, pTi, m);

	if (!pCache)
	{
		m_pDataStore[2] = pDataStore;
		pDataStore->Release();
	}
}

void CScaleformRecording::BeginSubmitMask(SubmitMaskMode _maskMode)
{
	// SubmitMaskMode := SubmitMaskMode
	const IScaleformPlayback::SubmitMaskMode& maskMode = *((IScaleformPlayback::SubmitMaskMode*)&_maskMode);

	RECORD_CMD_1ARG(GRCBA_BeginSubmitMask, CMD_VOID_RETURN, maskMode);

	GetPlayback()->BeginSubmitMask(maskMode);
}

void CScaleformRecording::EndSubmitMask()
{
	RECORD_CMD(GRCBA_EndSubmitMask, CMD_VOID_RETURN);

	GetPlayback()->EndSubmitMask();
}

void CScaleformRecording::DisableMask()
{
	RECORD_CMD(GRCBA_DisableMask, CMD_VOID_RETURN);

	GetPlayback()->DisableMask();
}

UInt CScaleformRecording::CheckFilterSupport(const BlurFilterParams& params)
{
#ifdef ENABLE_FLASH_FILTERS
	UInt flags = FilterSupport_Ok;
	if (params.Passes > 1 || (params.BlurX * params.BlurY > 32))
		flags |= FilterSupport_Multipass;

	return flags;
#else
	return FilterSupport_None;
#endif
}

void CScaleformRecording::DrawBlurRect(GTexture* _pSrcIn, const GRectF& _inSrcRect, const GRectF& _inDestRect, const BlurFilterParams& _params, bool islast)
{
	// IScaleformPlayback::RectF := GRectF
	const IScaleformPlayback::RectF& inSrcRect = *((IScaleformPlayback::RectF*)&_inSrcRect);
	const IScaleformPlayback::RectF& inDestRect = *((IScaleformPlayback::RectF*)&_inDestRect);
	ITexture* pSrcIn = gEnv->pRenderer->EF_GetTextureByID(static_cast<const GTextureXRenderBase*>(_pSrcIn)->GetID());
	const IScaleformPlayback::BlurFilterParams& params = *((IScaleformPlayback::BlurFilterParams*)&_params);

#ifdef ENABLE_FLASH_FILTERS
	_RECORD_CMD_PREFIX
	  _RECORD_CMD(GRCBA_DrawBlurRect)
	_RECORD_CMD_ARG(pSrcIn) m_pCmdBuf->AddRefCountedObject(_pSrcIn);
	_RECORD_CMD_ARG(inSrcRect)
	_RECORD_CMD_ARG(inDestRect)
	_RECORD_CMD_ARG(params)
	_RECORD_CMD_ARG(islast)
	_RECORD_CMD_POSTFIX(CMD_VOID_RETURN)
#endif

	GetPlayback()->DrawBlurRect(pSrcIn, inSrcRect, inDestRect, params, islast);
}

void CScaleformRecording::DrawColorMatrixRect(GTexture* _pSrcIn, const GRectF& _inSrcRect, const GRectF& _inDestRect, const Float* pMatrix, bool islast)
{
	// IScaleformPlayback::RectF := GRectF
	const IScaleformPlayback::RectF& inSrcRect = *((IScaleformPlayback::RectF*)&_inSrcRect);
	const IScaleformPlayback::RectF& inDestRect = *((IScaleformPlayback::RectF*)&_inDestRect);
	ITexture* pSrcIn = gEnv->pRenderer->EF_GetTextureByID(static_cast<const GTextureXRenderBase*>(_pSrcIn)->GetID());

#ifdef ENABLE_FLASH_FILTERS
	_RECORD_CMD_PREFIX
	  _RECORD_CMD(GRCBA_DrawColorMatrixRect)
	_RECORD_CMD_ARG(pSrcIn) m_pCmdBuf->AddRefCountedObject(_pSrcIn);
	_RECORD_CMD_ARG(inSrcRect)
	_RECORD_CMD_ARG(inDestRect)
	_RECORD_CMD_DATA(pMatrix, 5 * 4 * sizeof(Float))
	_RECORD_CMD_ARG(islast)
	_RECORD_CMD_POSTFIX(CMD_VOID_RETURN)
#endif
		
	GetPlayback()->DrawColorMatrixRect(pSrcIn, inSrcRect, inDestRect, pMatrix, islast);
}

void CScaleformRecording::GetRenderStats(Stats* pStats, bool resetStats)
{
	threadID rsIdx = 0;
	gEnv->pRenderer->EF_Query(IsMainThread() ? EFQ_MainThreadList : EFQ_RenderThreadList, rsIdx);
	assert(rsIdx < CRY_ARRAY_COUNT(m_renderStats));

	if (pStats)
		memcpy(pStats, &m_renderStats[rsIdx], sizeof(Stats));
	if (resetStats)
		m_renderStats[rsIdx].Clear();
}

void CScaleformRecording::GetStats(GStatBag* /*pBag*/, bool /*reset*/)
{
	assert(0 && "CScaleformRecording::CScaleformRecording::GetStats() -- Not implemented!");
}

void CScaleformRecording::ReleaseResources()
{
	GetPlayback()->ReleaseResources();
}

namespace CScaleformRecordingClearInternal
{
	const float safeRangeScale = 8.0f;

	struct MapToSafeRange
	{
		MapToSafeRange(float v) : val(v) {}
		operator int16() const
		{
			float v = val / safeRangeScale;
			return (int16) (v >= 0 ? v + 0.5f : v - 0.5f);
		}
		float val;
	};
}

void CScaleformRecording::SetClearFlags(uint32 clearFlags, ColorF clearColor)
{
	m_pPlayback->SetClearFlags(clearFlags, clearColor);
}

void CScaleformRecording::SetCompositingDepth(float depth)
{
	m_pPlayback->SetCompositingDepth(depth);
}

void CScaleformRecording::SetStereoMode(bool stereo, bool isLeft)
{
	m_pPlayback->SetStereoMode(stereo, isLeft);
}

void CScaleformRecording::StereoEnforceFixedProjectionDepth(bool enforce)
{
	m_pPlayback->StereoEnforceFixedProjectionDepth(enforce);
}

void CScaleformRecording::StereoSetCustomMaxParallax(float maxParallax)
{
	m_pPlayback->StereoSetCustomMaxParallax(maxParallax);
}

void CScaleformRecording::AvoidStencilClear(bool avoid)
{
	m_pPlayback->AvoidStencilClear(avoid);
}

void CScaleformRecording::EnableMaskedRendering(bool enable)
{
	m_pPlayback->EnableMaskedRendering(enable);
}

void CScaleformRecording::ExtendCanvasToViewport(bool extend)
{
	m_pPlayback->ExtendCanvasToViewport(extend);
}

void CScaleformRecording::SetThreadIDs(uint32 mainThreadID, uint32 renderThreadID)
{
	m_mainThreadID   = mainThreadID;
	m_renderThreadID = renderThreadID;
	assert(IsMainThread());
}

bool CScaleformRecording::IsMainThread() const
{
	uint32 threadID = GetCurrentThreadId();
	return threadID == m_mainThreadID;
}

bool CScaleformRecording::IsRenderThread() const
{
	uint32 threadID = GetCurrentThreadId();
	return threadID == m_renderThreadID;
}

void CScaleformRecording::SetRecordingCommandBuffer(GRendererCommandBuffer* pCmdBuf)
{
	assert(IsMainThread());
	m_pCmdBuf = pCmdBuf;
}

#endif // #ifdef INCLUDE_SCALEFORM_SDK
