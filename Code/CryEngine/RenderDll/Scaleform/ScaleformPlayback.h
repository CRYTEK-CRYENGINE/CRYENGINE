// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _GRENDERER_XRENDER_H_
#define _GRENDERER_XRENDER_H_

#pragma once
#include <CryRenderer/IScaleform.h>

#if RENDERER_SUPPORT_SCALEFORM

#include <vector>

#include "../XRenderD3D9/GraphicsPipeline/Common/PrimitiveRenderPass.h"
#include "../XRenderD3D9/GraphicsPipeline/Common/UtilityPasses.h"

#define ENABLE_FLASH_FILTERS

struct IRenderer;
class ITexture;
class ITexture;
class CConstantBuffer;
class CScaleformPlayback;
struct SSF_GlobalDrawParams;
struct GRendererCommandBuffer;
class CD3D9Renderer;
class CCachedData;
class CRenderOutput;

//////////////////////////////////////////////////////////////////////
struct SSF_GlobalDrawParams
{
	enum EFillType
	{
		None,

		SolidColor,
		Texture,

		// Internal fills
		GlyphTexture,
		GlyphAlphaTexture,

		GlyphTextureMat,
		GlyphTextureMatMul,

		GlyphTextureYUV,
		GlyphTextureYUVA,

		// Scaleform fills
		GColor,
		GColorNoAddAlpha,
		G1Texture,
		G1TextureColor,
		G2Texture,
		G2TextureColor,
		G3Texture,

		FillCount
	};
	EFillType fillType;

	const IScaleformPlayback::DeviceData* vtxData;
	const IScaleformPlayback::DeviceData* idxData;

	enum ETexState
	{
		TS_Clamp = 0x01,

		TS_FilterLin = 0x02,
		TS_FilterTriLin = 0x04
	};

	struct STextureInfo
	{
		CTexture* pTex;
		uint32 texState;
	};
	STextureInfo texture[4];

	int texID_YUVA[4];

	uint32 blendModeStates;
	uint32 renderMaskedStates;

	bool isMultiplyDarkBlendMode;
	bool premultipliedAlpha;

	enum ESFMaskOp
	{
		BeginSubmitMask_Clear,
		BeginSubmitMask_Inc,
		BeginSubmitMask_Dec,
		EndSubmitMask,
		DisableMask
	};

	struct
	{
		int func;
		uint32 ref;
	}
	m_stencil;

	enum EAlphaBlendOp
	{
		Add,
		Substract,
		RevSubstract,
		Min,
		Max
	};
	EAlphaBlendOp blendOp;

	enum EBlurType
	{
		BlurNone = 0,
		
		// start_shadows,
		Box2InnerShadow,
		Box2InnerShadowHighlight,
		Box2InnerShadowMul,
		Box2InnerShadowMulHighlight,
		Box2InnerShadowKnockout,
		Box2InnerShadowHighlightKnockout,
		Box2InnerShadowMulKnockout,
		Box2InnerShadowMulHighlightKnockout,
		Box2Shadow,
		Box2ShadowHighlight,
		Box2ShadowMul,
		Box2ShadowMulHighlight,
		Box2ShadowKnockout,
		Box2ShadowHighlightKnockout,
		Box2ShadowMulKnockout,
		Box2ShadowMulHighlightKnockout,
		Box2Shadowonly,
		Box2ShadowonlyHighlight,
		Box2ShadowonlyMul,
		Box2ShadowonlyMulHighlight,
		// end_shadows,

		// start_blurs,
		Box1Blur,
		Box2Blur,
		Box1BlurMul,
		Box2BlurMul,
		// end_blurs,

		// start_cmatrix,
		//      CMatrix = 25,
		//      CMatrixMul,
		// end_cmatrix = 26,
		
		BlurCount,

		shadows_Highlight = 0x00000001,
		shadows_Mul = 0x00000002,
		shadows_Knockout = 0x00000004,
		blurs_Box2 = 0x00000001,
		blurs_Mul = 0x00000002,
		
		//      cmatrix_Mul                  = 0x00000001,
	};

	EBlurType blurType;

	//////////////////////////////////////////////////////////////////////////
	struct OutputParams : private NoCopy
	{
		typedef _smart_ptr<CTexture> TexSmartPtr;

		int key;

		// Backups for GraphicsPipeline = 0
		TexSmartPtr pRenderTarget;
		TexSmartPtr pStencilTarget;
		Matrix44 oldViewMat;
		int oldViewportWidth;
		int oldViewportHeight;

		// Pass for GraphicsPipeline > 0
		CPrimitiveRenderPass renderPass;
		mutable CClearRegionPass clearPass;

		// Handling clears
		mutable bool bRenderTargetClear;
		mutable bool bStencilTargetClear;
	};

	bool scissorEnabled;

	struct _D3DRectangle : public ::D3DRectangle {
		_D3DRectangle() {}
		_D3DRectangle(int l, int t, int r, int b) { left = l; top = t; right = r; bottom = b; }
	} scissor;

	struct _D3DViewPort : public ::D3DViewPort {
		_D3DViewPort() {}
		_D3DViewPort(float l, float t, float w, float h) { TopLeftX = l; TopLeftY = t; Width = w; Height = h; MinDepth = 0.0f; MaxDepth = 1.0f; }
	} viewport;

	OutputParams* pRenderOutput;

	//////////////////////////////////////////////////////////////////////////
	struct SScaleformMeshAttributes
	{
		// VS
		Matrix44 cCompositeMat;          // Float4x4
		Vec4 cTexGenMat[2][2];           // Float2x4[2]
		Vec2 cStereoVideoFrameSelect;    // Vec4
		Vec2 cPad;
	}
	*m_pScaleformMeshAttributes;
	size_t m_ScaleformMeshAttributesSize;
	mutable bool m_bScaleformMeshAttributesDirty;

	struct SScaleformRenderParameters
	{
		// PS
		ColorF cBitmapColorTransform[2]; // Float2x4
		Matrix44 cColorTransformMat;     // Float4x4
		ColorF cBlurFilterColor1;        // Float4
		ColorF cBlurFilterColor2;        // Float4
		Vec2 cBlurFilterBias;            // Vec2
		Vec2 cBlurFilterScale;           // Vec2
		Vec3 cBlurFilterSize;            // Float3
		float bPremultiplyAlpha;         // Float
	}
	*m_pScaleformRenderParameters;
	size_t m_ScaleformRenderParametersSize;
	mutable bool m_bScaleformRenderParametersDirty;

	mutable CConstantBuffer* m_vsBuffer;
	mutable CConstantBuffer* m_psBuffer;

	SSF_GlobalDrawParams();
	~SSF_GlobalDrawParams();

	//////////////////////////////////////////////////////////////////////////
	void Reset()
	{
		fillType = None;

		m_stencil.func = 0;
		m_stencil.ref = 0;

		vtxData = nullptr;
		idxData = nullptr;

		m_bScaleformMeshAttributesDirty = true;
		m_bScaleformRenderParametersDirty = true;

		m_pScaleformMeshAttributes->cCompositeMat.SetZero();

		texture[0].pTex = nullptr;
		texture[0].texState = 0;
		texture[1].pTex = nullptr;
		texture[1].texState = 0;

		m_pScaleformMeshAttributes->cTexGenMat[0][0] = Vec4(1, 0, 0, 0);
		m_pScaleformMeshAttributes->cTexGenMat[0][1] = Vec4(0, 1, 0, 0);

		m_pScaleformMeshAttributes->cTexGenMat[1][0] = Vec4(1, 0, 0, 0);
		m_pScaleformMeshAttributes->cTexGenMat[1][1] = Vec4(0, 1, 0, 0);

		texID_YUVA[0] = texID_YUVA[1] = texID_YUVA[2] = texID_YUVA[3] = -1;

		m_pScaleformMeshAttributes->cStereoVideoFrameSelect = Vec2(1.0f, 0.0f);

		m_pScaleformRenderParameters->cBitmapColorTransform[0] = ColorF(0, 0, 0, 0);
		m_pScaleformRenderParameters->cBitmapColorTransform[1] = ColorF(0, 0, 0, 0);

		m_pScaleformRenderParameters->cColorTransformMat.SetZero();

		blendModeStates = 0;
		renderMaskedStates = 0;

		isMultiplyDarkBlendMode = false;
		premultipliedAlpha = false;

		m_pScaleformRenderParameters->bPremultiplyAlpha = 0.f;

		scissorEnabled = false;
		scissor = SSF_GlobalDrawParams::_D3DRectangle(0, 0, 0, 0);
		viewport = SSF_GlobalDrawParams::_D3DViewPort(0.f, 0.f, 0.f, 0.f);

		blendOp = Add;
		blurType = BlurNone;

		m_pScaleformRenderParameters->cBlurFilterSize = Vec3(0.f, 0.f, 0.f);
		m_pScaleformRenderParameters->cBlurFilterScale = Vec2(0.f, 0.f);
		m_pScaleformRenderParameters->cBlurFilterBias = Vec2(0.f, 0.f);
		m_pScaleformRenderParameters->cBlurFilterColor1 = ColorF(0.f, 0.f, 0.f, 0.f);
		m_pScaleformRenderParameters->cBlurFilterColor2 = ColorF(0.f, 0.f, 0.f, 0.f);
	}
};

//////////////////////////////////////////////////////////////////////
class CScaleformPlayback final : public IScaleformPlayback
{
	// GRenderer interface
public:
	virtual ITexture* CreateRenderTarget() override;
	virtual void SetDisplayRenderTarget(ITexture* pRT, bool setState = 1) override;
	virtual void PushRenderTarget(const RectF& frameRect, ITexture* pRT) override;
	virtual void PopRenderTarget() override;
	virtual int32 PushTempRenderTarget(const RectF& frameRect, uint32 targetW, uint32 targetH, bool wantClear = false, bool wantStencil = false) override;

	virtual void BeginDisplay(ColorF backgroundColor, const Viewport& viewport, bool bScissor, const Viewport& scissor, const RectF& x0x1y0y1) override;
	virtual void EndDisplay() override;

	virtual void SetMatrix(const Matrix23& m) override;
	virtual void SetUserMatrix(const Matrix23& m) override;
	virtual void SetCxform(const ColorF& cx0, const ColorF& cx1) override;

	virtual void PushBlendMode(BlendType mode) override;
	virtual void PopBlendMode() override;

	virtual void SetPerspective3D(const Matrix44& projMatIn) override;
	virtual void SetView3D(const Matrix44& viewMatIn) override;
	virtual void SetWorld3D(const Matrix44f* pWorldMatIn) override;

	virtual void SetVertexData(const DeviceData* pVertices) override;
	virtual void SetIndexData(const DeviceData* pIndices) override;

	virtual void DrawIndexedTriList(int baseVertexIndex, int minVertexIndex, int numVertices, int startIndex, int triangleCount) override;
	virtual void DrawLineStrip(int baseVertexIndex, int lineCount) override;

	virtual void LineStyleDisable() override;
	virtual void LineStyleColor(ColorF color) override;

	virtual void FillStyleDisable() override;
	virtual void FillStyleColor(ColorF color) override;
	virtual void FillStyleBitmap(const FillTexture& Fill) override;
	virtual void FillStyleGouraud(GouraudFillType fillType, const FillTexture& Texture0, const FillTexture& Texture1, const FillTexture& Texture2) override;

	virtual void DrawBitmaps(const DeviceData* pBitmaps, int startIndex, int count, ITexture* pTi, const Matrix23& m) override;

	virtual void BeginSubmitMask(SubmitMaskMode maskMode) override;
	virtual void EndSubmitMask() override;
	virtual void DisableMask() override;

	virtual void DrawBlurRect(ITexture* pSrcIn, const RectF& inSrcRect, const RectF& inDestRect, const BlurFilterParams& params, bool isLast = false) override;
	virtual void DrawColorMatrixRect(ITexture* pSrcIn, const RectF& inSrcRect, const RectF& inDestRect, const float* pMatrix, bool isLast = false) override;

	virtual void ReleaseResources() override;

public:
	static void InitCVars();

	// IScaleformPlayback interface
public:
	CScaleformPlayback();
	virtual ~CScaleformPlayback();

	virtual DeviceData* CreateDeviceData(const void* pVertices, int numVertices, VertexFormat vf, bool bTemp = false) override;
	virtual DeviceData* CreateDeviceData(const void* pIndices, int numIndices, IndexFormat idxf, bool bTemp = false) override;
	virtual DeviceData* CreateDeviceData(const BitmapDesc* pBitmapList, int numBitmaps, bool bTemp = false) override;
	virtual void ReleaseDeviceData(DeviceData* pData) override;

	// IFlashPlayer
	virtual void SetClearFlags(uint32 clearFlags, ColorF clearColor = Clr_Transparent) override;
	virtual void SetCompositingDepth(float depth) override;

	virtual void SetStereoMode(bool stereo, bool isLeft) override;
	virtual void StereoEnforceFixedProjectionDepth(bool enforce) override;
	virtual void StereoSetCustomMaxParallax(float maxParallax) override;

	virtual void AvoidStencilClear(bool avoid) override;
	virtual void EnableMaskedRendering(bool enable) override;
	virtual void ExtendCanvasToViewport(bool extend) override;

	virtual void SetThreadIDs(uint32 mainThreadID, uint32 renderThreadID) override;
	virtual bool IsMainThread() const override;
	virtual bool IsRenderThread() const override;

	virtual void GetMemoryUsage(ICrySizer* pSizer) const override
	{
		// !!! don't have to add anything, all allocations go through GFx memory interface !!!
	}

	virtual std::vector<ITexture*> GetTempRenderTargets() const override;

	// Helper functions
	static void RenderFlashPlayerToTexture(IFlashPlayer* pFlashPlayer, CTexture* pOutput);
	static void RenderFlashPlayerToOutput(IFlashPlayer* pFlashPlayer, const std::shared_ptr<CRenderOutput> &output);

	void SetRenderOutput(std::shared_ptr<CRenderOutput> pRenderOutput);
	
private:
	std::shared_ptr<CRenderOutput> GetRenderOutput() const;

	void PushOutputTarget(const Viewport &viewport);
	void PopOutputTarget();

	void Clear(const ColorF& backgroundColor);

	void CalcTransMat2D(const Matrix23* pSrc, Matrix44* __restrict pDst) const;
	void CalcTransMat3D(const Matrix23* pSrc, Matrix44* __restrict pDst);

	void UpdateStereoSettings();

	void ApplyStencilMask(SSF_GlobalDrawParams::ESFMaskOp maskOp, unsigned int stencilRef);
	void ApplyBlendMode(BlendType blendMode);
	void ApplyColor(const ColorF& src);
	void ApplyTextureInfo(unsigned int texSlot, const FillTexture* pFill = 0);
	void ApplyCxform();
	void ApplyMatrix(const Matrix23* pMatIn);

private:
	enum
	{
		sys_flash_debugdraw_DefVal = 0
	};

	DeclareStaticConstIntCVar(ms_sys_flash_debugdraw, sys_flash_debugdraw_DefVal);

	static float ms_sys_flash_stereo_maxparallax;
	static float ms_sys_flash_debugdraw_depth;

private:
	CD3D9Renderer* m_pRenderer;

	bool m_stencilAvail;
	bool m_renderMasked;
	int  m_stencilCounter;
	bool m_avoidStencilClear;
	bool m_maskedRendering;
	bool m_extendCanvasToVP;

	bool m_scissorEnabled;
	bool m_applyHalfPixelOffset;

	int m_maxVertices;
	int m_maxIndices;

	// transformation matrices
	Matrix44 m_matViewport;
	Matrix44 m_matViewport3D;
	Matrix44 m_matUncached;
	Matrix44 m_matCached2D;
	Matrix44 m_matCached3D;
	Matrix44 m_matCachedWVP;

	Matrix23 m_mat;

	const Matrix44f* m_pMatWorld3D;
	Matrix44 m_matView3D;
	Matrix44 m_matProj3D;

	bool m_matCached2DDirty;
	bool m_matCached3DDirty;
	bool m_matCachedWVPDirty;

	// current color transform for all draw modes
	ColorF m_cxform[2];

	// system vertex buffer for streaming glyphs
	struct SGlyphVertex
	{
		float x, y, u, v;
		unsigned int col;
	};
	std::vector<SGlyphVertex> m_glyphVB;

	// blend mode support
	std::vector<BlendType> m_blendOpStack;
	BlendType m_curBlendMode;

	// render stats
	threadID m_rsIdx;
	Stats m_renderStats[2];

	// draw parameters
	SSF_GlobalDrawParams m_drawParams;

	RectF m_canvasRect;

	uint32 m_clearFlags;
	ColorF m_clearColor;
	float m_compDepth;

	// stereo support
	float m_stereo3DiBaseDepth;
	float m_maxParallax;
	float m_customMaxParallax;

	float m_maxParallaxScene;
	float m_screenDepthScene;

	bool m_stereoFixedProjDepth;
	bool m_isStereo;
	bool m_isLeft;

	// lockless rendering
	threadID m_mainThreadID;
	threadID m_renderThreadID;

	// Render target management
	std::list<SSF_GlobalDrawParams::OutputParams> m_renderTargetStack;

	std::shared_ptr<CRenderOutput> m_pRenderOutput;
};

#endif // #if RENDERER_SUPPORT_SCALEFORM

//////////////////////////////////////////////////////////////////////
class CScaleformSink final : public IScaleformPlayback
{
	// GRenderer interface
public:
	virtual ITexture* CreateRenderTarget() override { return 0; }
	virtual void SetDisplayRenderTarget(ITexture*, bool) override {}
	virtual void PushRenderTarget(const RectF&, ITexture*) override {}
	virtual void PopRenderTarget() override {}
	virtual int32 PushTempRenderTarget(const RectF&, uint32, uint32, bool, bool) override { return 0; }

	virtual void BeginDisplay(ColorF, const Viewport&, bool, const Viewport&, const RectF&) override {}
	virtual void EndDisplay() override {}

	virtual void SetMatrix(const Matrix23&) override {}
	virtual void SetUserMatrix(const Matrix23&) override {}
	virtual void SetCxform(const ColorF&, const ColorF&) override {}

	virtual void PushBlendMode(BlendType) override {}
	virtual void PopBlendMode() override {}

	virtual void SetPerspective3D(const Matrix44&) override {}
	virtual void SetView3D(const Matrix44&) override {}
	virtual void SetWorld3D(const Matrix44f*) override {}

	virtual void SetVertexData(const DeviceData*) override {}
	virtual void SetIndexData(const DeviceData*) override {}

	virtual void DrawIndexedTriList(int, int, int, int, int) override {}
	virtual void DrawLineStrip(int, int) override {}

	virtual void LineStyleDisable() override {}
	virtual void LineStyleColor(ColorF color) override {}

	virtual void FillStyleDisable() override {}
	virtual void FillStyleColor(ColorF) override {}
	virtual void FillStyleBitmap(const FillTexture&) override {}
	virtual void FillStyleGouraud(GouraudFillType, const FillTexture&, const FillTexture&, const FillTexture&) override {}

	virtual void DrawBitmaps(const DeviceData*, int, int, ITexture*, const Matrix23&) override {}

	virtual void BeginSubmitMask(SubmitMaskMode) override {}
	virtual void EndSubmitMask() override {}
	virtual void DisableMask() override {}

	virtual void DrawBlurRect(ITexture*, const RectF&, const RectF&, const BlurFilterParams&, bool) override {}
	virtual void DrawColorMatrixRect(ITexture*, const RectF&, const RectF&, const float*, bool) override {}

	virtual void ReleaseResources() override {}

	// IScaleformPlayback interface
public:
	virtual ~CScaleformSink() {}

	virtual DeviceData* CreateDeviceData(const void* pVertices, int numVertices, VertexFormat vf, bool bTemp = false) override { return nullptr; }
	virtual DeviceData* CreateDeviceData(const void* pIndices, int numIndices, IndexFormat idxf, bool bTemp = false) override { return nullptr; }
	virtual DeviceData* CreateDeviceData(const BitmapDesc* pBitmapList, int numBitmaps, bool bTemp = false) override { return nullptr; }
	virtual void ReleaseDeviceData(DeviceData* pData) override {}

	virtual void SetClearFlags(uint32 clearFlags, ColorF clearColor = Clr_Transparent) override {}
	virtual void SetCompositingDepth(float depth) override {}

	virtual void SetStereoMode(bool stereo, bool isLeft) override {}
	virtual void StereoEnforceFixedProjectionDepth(bool enforce) override {}
	virtual void StereoSetCustomMaxParallax(float maxParallax) override {}

	virtual void AvoidStencilClear(bool avoid) override {}
	virtual void EnableMaskedRendering(bool enable) override {}
	virtual void ExtendCanvasToViewport(bool extend) override {}

	virtual void SetThreadIDs(uint32 mainThreadID, uint32 renderThreadID) override {}
	virtual bool IsMainThread() const override { return false; }
	virtual bool IsRenderThread() const override { return false; }

	virtual void GetMemoryUsage(ICrySizer* pSizer) const override {}

	virtual std::vector<ITexture*> GetTempRenderTargets() const override { return std::vector<ITexture*>(); }
};

//////////////////////////////////////////////////////////////////////
static inline void Unlock(CCachedData* pStore)
{
	if (pStore)
	{
		pStore->Unlock();
	}
}

#if defined(_DEBUG)

#define _PLAYBACK_CMD_SUFFIX				  \
	{										  \
		const size_t startPos = pBuffer->GetReadPos(); \
		const size_t argDataSize = pBuffer->Read<size_t>();

#define _PLAYBACK_CMD_POSTFIX								\
		const size_t argDataSizeRead = pBuffer->GetReadPos() - startPos;	\
		assert(argDataSizeRead == argDataSize);					\
	}

#else // #if defined(_DEBUG)

#define _PLAYBACK_CMD_SUFFIX \
	{

#define _PLAYBACK_CMD_POSTFIX \
	}

#endif // #if defined(_DEBUG)

template<class T>
void SF_Playback(T* pRenderer, GRendererCommandBufferReadOnly* pBuffer)
{
	assert(pRenderer);

	CCachedData* pLastVertexStore = 0;
	CCachedData* pLastIndexStore = 0;

	const size_t cmdBufSize = pBuffer->Size();
	if (cmdBufSize)
	{
		pBuffer->ResetReadPos();
		while (pBuffer->GetReadPos() < cmdBufSize)
		{
			const EGRendererCommandBufferCmds curCmd = pBuffer->Read<EGRendererCommandBufferCmds>();
			_PLAYBACK_CMD_SUFFIX
				switch (curCmd)
				{
				case GRCBA_PopRenderTarget:
				{
					pRenderer->T::PopRenderTarget();
					break;
				}

				case GRCBA_PushTempRenderTarget:
				{
					const IScaleformPlayback::RectF& frameRect = pBuffer->ReadRef<IScaleformPlayback::RectF>();
					uint32 targetW = pBuffer->Read<uint32>();
					uint32 targetH = pBuffer->Read<uint32>();
					int32* pRT = pBuffer->Read<int32*>();
					bool wS = pBuffer->Read<bool>();
					int32 pTempRT = pRenderer->T::PushTempRenderTarget(frameRect, targetW, targetH, true, wS);
					*pRT = pTempRT;
					break;
				}

				case GRCBA_BeginDisplay:
				{
					ColorF backgroundColor = pBuffer->Read<ColorF>();
					const IScaleformPlayback::Viewport& viewport = pBuffer->ReadRef<IScaleformPlayback::Viewport>();
					bool bS = pBuffer->Read<bool>();
					const IScaleformPlayback::Viewport& scissor = pBuffer->ReadRef<IScaleformPlayback::Viewport>();
					const IScaleformPlayback::RectF& x0x1y0y1 = pBuffer->ReadRef<IScaleformPlayback::RectF>();
					pRenderer->T::BeginDisplay(backgroundColor, viewport, bS, scissor, x0x1y0y1);
					break;
				}
				case GRCBA_EndDisplay:
				{
					pRenderer->T::EndDisplay();
					break;
				}
				case GRCBA_SetMatrix:
				{
					const IScaleformPlayback::Matrix23& m = pBuffer->ReadRef<IScaleformPlayback::Matrix23>();
					pRenderer->T::SetMatrix(m);
					break;
				}
				case GRCBA_SetUserMatrix:
				{
					assert(0);
					break;
				}
				case GRCBA_SetCxform:
				{
					const ColorF& cx0 = pBuffer->ReadRef<ColorF>();
					const ColorF& cx1 = pBuffer->ReadRef<ColorF>();
					pRenderer->T::SetCxform(cx0, cx1);
					break;
				}
				case GRCBA_PushBlendMode:
				{
					IScaleformPlayback::BlendType mode = pBuffer->Read<IScaleformPlayback::BlendType>();
					pRenderer->T::PushBlendMode(mode);
					break;
				}
				case GRCBA_PopBlendMode:
				{
					pRenderer->T::PopBlendMode();
					break;
				}
				case GRCBA_SetPerspective3D:
				{
					Matrix44 projMatIn = pBuffer->ReadRef<Matrix44f>();
					pRenderer->T::SetPerspective3D(projMatIn);
					break;
				}
				case GRCBA_SetView3D:
				{
					Matrix44 viewMatIn = pBuffer->ReadRef<Matrix44f>();
					pRenderer->T::SetView3D(viewMatIn);
					break;
				}
				case GRCBA_SetWorld3D:
				{
					const Matrix44f& worldMatIn = pBuffer->ReadRef<Matrix44f>();
					pRenderer->T::SetWorld3D(&worldMatIn);
					break;
				}
				case GRCBA_SetWorld3D_NullPtr:
				{
					pRenderer->T::SetWorld3D(0);
					break;
				}
				case GRCBA_SetVertexData:
				{
					Unlock(pLastVertexStore);
					CCachedData* pStore = pBuffer->Read<CCachedData*>();
					const IScaleformPlayback::DeviceData* pDeviceData = 0;
					pStore->Lock();
					pDeviceData = pStore->GetPtr();
					pLastVertexStore = pStore;
					pRenderer->T::SetVertexData(pDeviceData);
					break;
				}
				case GRCBA_SetIndexData:
				{
					Unlock(pLastIndexStore);
					CCachedData* pStore = pBuffer->Read<CCachedData*>();
					const IScaleformPlayback::DeviceData* pDeviceData = 0;
					pStore->Lock();
					pDeviceData = pStore->GetPtr();
					pLastIndexStore = pStore;
					pRenderer->T::SetIndexData(pDeviceData);
					break;
				}
				case GRCBA_DrawIndexedTriList:
				{
					int baseVertexIndex = pBuffer->Read<int>();
					int minVertexIndex = pBuffer->Read<int>();
					int numVertices = pBuffer->Read<int>();
					int startIndex = pBuffer->Read<int>();
					int triangleCount = pBuffer->Read<int>();
					pRenderer->T::DrawIndexedTriList(baseVertexIndex, minVertexIndex, numVertices, startIndex, triangleCount);
					Unlock(pLastVertexStore);
					pLastVertexStore = 0;
					Unlock(pLastIndexStore);
					pLastIndexStore = 0;
					break;
				}
				case GRCBA_DrawLineStrip:
				{
					int baseVertexIndex = pBuffer->Read<int>();
					int lineCount = pBuffer->Read<int>();
					pRenderer->T::DrawLineStrip(baseVertexIndex, lineCount);
					Unlock(pLastVertexStore);
					pLastVertexStore = 0;
					break;
				}
				case GRCBA_LineStyleDisable:
				{
					pRenderer->T::LineStyleDisable();
					break;
				}
				case GRCBA_LineStyleColor:
				{
					const ColorF& color = pBuffer->Read<ColorF>();
					pRenderer->T::LineStyleColor(color);
					break;
				}
				case GRCBA_FillStyleDisable:
				{
					pRenderer->T::FillStyleDisable();
					break;
				}
				case GRCBA_FillStyleColor:
				{
					const ColorF& color = pBuffer->Read<ColorF>();
					pRenderer->T::FillStyleColor(color);
					break;
				}
				case GRCBA_FillStyleBitmap:
				{
					const IScaleformPlayback::FillTexture& fill = pBuffer->ReadRef<IScaleformPlayback::FillTexture>();
					pRenderer->T::FillStyleBitmap(fill);
					break;
				}
				case GRCBA_FillStyleGouraud:
				{
					IScaleformPlayback::GouraudFillType fillType = pBuffer->Read<IScaleformPlayback::GouraudFillType>();
					const IScaleformPlayback::FillTexture& texture0 = pBuffer->ReadRef<IScaleformPlayback::FillTexture>();
					const IScaleformPlayback::FillTexture& texture1 = pBuffer->ReadRef<IScaleformPlayback::FillTexture>();
					const IScaleformPlayback::FillTexture& texture2 = pBuffer->ReadRef<IScaleformPlayback::FillTexture>();
					pRenderer->T::FillStyleGouraud(fillType, texture0, texture1, texture2);
					break;
				}
				case GRCBA_DrawBitmaps:
				{
					CCachedData* pStore = pBuffer->Read<CCachedData*>();
					const IScaleformPlayback::DeviceData* pDeviceData = 0;
					pStore->Lock();
					pDeviceData = pStore->GetPtr();
					int startIndex = pBuffer->Read<int>();
					int count = pBuffer->Read<int>();
					ITexture* pTi = pBuffer->Read<ITexture*>();
					const IScaleformPlayback::Matrix23& m = pBuffer->ReadRef<IScaleformPlayback::Matrix23>();
					pRenderer->T::DrawBitmaps(pDeviceData, startIndex, count, pTi, m);
					Unlock(pStore);
					break;
				}
				case GRCBA_BeginSubmitMask:
				{
					IScaleformPlayback::SubmitMaskMode maskMode = pBuffer->Read<IScaleformPlayback::SubmitMaskMode>();
					pRenderer->T::BeginSubmitMask(maskMode);
					break;
				}
				case GRCBA_EndSubmitMask:
				{
					pRenderer->T::EndSubmitMask();
					break;
				}
				case GRCBA_DisableMask:
				{
					pRenderer->T::DisableMask();
					break;
				}

				case GRCBA_DrawBlurRect:
				{
					ITexture* pSrcIn = pBuffer->Read<ITexture*>();
					const IScaleformPlayback::RectF& inSrcRect = pBuffer->ReadRef<IScaleformPlayback::RectF>();
					const IScaleformPlayback::RectF& inDestRect = pBuffer->ReadRef<IScaleformPlayback::RectF>();
					const IScaleformPlayback::BlurFilterParams& params = pBuffer->ReadRef<IScaleformPlayback::BlurFilterParams>();
					const bool isLast = pBuffer->Read<bool>();
					pRenderer->T::DrawBlurRect(pSrcIn, inSrcRect, inDestRect, params, isLast);
					break;
				}

				case GRCBA_DrawColorMatrixRect:
				{
					ITexture* pSrcIn = pBuffer->Read<ITexture*>();
					const IScaleformPlayback::RectF& inSrcRect = pBuffer->ReadRef<IScaleformPlayback::RectF>();
					const IScaleformPlayback::RectF& inDestRect = pBuffer->ReadRef<IScaleformPlayback::RectF>();
					size_t size = pBuffer->Read<size_t>();
					const float* pMatrix = (float*)(size ? pBuffer->GetReadPosPtr() : 0);
					pBuffer->Skip(size, true);
					const bool isLast = pBuffer->Read<bool>();
					pRenderer->T::DrawColorMatrixRect(pSrcIn, inSrcRect, inDestRect, pMatrix, isLast);
					break;
				}

				default:
				{
					assert(0);
					break;
				}
			}
			_PLAYBACK_CMD_POSTFIX
		}
		assert(pBuffer->GetReadPos() == cmdBufSize);
	}

	Unlock(pLastVertexStore);
	Unlock(pLastIndexStore);
};

#endif // #ifndef _GRENDERER_XRENDER_H_
