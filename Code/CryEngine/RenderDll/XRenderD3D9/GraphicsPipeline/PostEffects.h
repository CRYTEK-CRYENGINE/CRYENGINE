// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"
#include "Common/UtilityPasses.h"
#include "Common/PrimitiveRenderPass.h"

class CRenderView;
class CEffectParam;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CPostEffectContext : private NoCopy
{
public:
	CPostEffectContext();
	virtual ~CPostEffectContext();

	void Setup(CPostEffectsMgr* pPostEffectsMgr);

	void EnableAltBackBuffer(bool enable);

	void         SetRenderView( CRenderView *pRenderView ) { m_pRenderView = pRenderView; }
	CRenderView* GetRenderView() const { return m_pRenderView; };

public:
	uint64              GetShaderRTMask() const;

	CTexture*           GetSrcBackBufferTexture() const;
	CTexture*           GetDstBackBufferTexture() const;
	CTexture*           GetDstDepthStencilTexture() const;

	CPostEffect*        GetPostEffect(EPostEffectID nID) const;
	const CEffectParam* GetEffectParamByName(const char* pszParam) const;

private:
	CPostEffectsMgr* m_pPostEffectsMgr = nullptr;
	uint64           m_shaderRTMask = 0;
	bool             m_bUseAltBackBuffer = false;
	CRenderView*     m_pRenderView = nullptr;
};

struct IPostEffectPass : private NoCopy
{
	virtual ~IPostEffectPass() {}

	virtual void Init() = 0;
	virtual void Execute(const CPostEffectContext& context) = 0;
};

class CPostEffectStage : public CGraphicsPipelineStage
{
public:
	CPostEffectStage();
	virtual ~CPostEffectStage();

	void Init() final;
	void Update() final;

	bool Execute();

private:
	void Execute3DHudFlashUpdate();

private:
	std::array<std::unique_ptr<IPostEffectPass>, EPostEffectID::Num> m_postEffectArray;
	CPostEffectContext m_context;

	CStretchRectPass   m_passCopyScreenToTex;
#ifndef _RELEASE
	CFullscreenPass    m_passAntialiasingDebug;
#endif

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CUnderwaterGodRaysPass : public IPostEffectPass
{
public:
	CUnderwaterGodRaysPass();
	~CUnderwaterGodRaysPass();

	virtual void Init() override;
	virtual void Execute(const CPostEffectContext& context) override;

private:
	static const int32 SliceCount = 10;

private:
	CFullscreenPass m_passUnderwaterGodRaysGen[SliceCount];
	CFullscreenPass m_passUnderwaterGodRaysFinal;

	CTexture*       m_pWavesTex = nullptr;
	CTexture*       m_pCausticsTex = nullptr;
	CTexture*       m_pUnderwaterBumpTex = nullptr;
};

//////////////////////////////////////////////////////////////////////////
class CWaterDropletsPass : public IPostEffectPass
{
public:
	CWaterDropletsPass();
	~CWaterDropletsPass();

	virtual void Init() override;
	virtual void Execute(const CPostEffectContext& context) override;

private:
	CFullscreenPass m_passWaterDroplets;

	CTexture*       m_pWaterDropletsBumpTex = nullptr;

};

//////////////////////////////////////////////////////////////////////////
class CWaterFlowPass : public IPostEffectPass
{
public:
	CWaterFlowPass();
	~CWaterFlowPass();

	virtual void Init() override;
	virtual void Execute(const CPostEffectContext& context) override;

private:
	CFullscreenPass m_passWaterFlow;

	CTexture*       m_pWaterFlowBumpTex = nullptr;

};

//////////////////////////////////////////////////////////////////////////
class CSharpeningPass : public IPostEffectPass
{
public:
	virtual void Init() override;
	virtual void Execute(const CPostEffectContext& context) override;

private:
	CStretchRectPass m_passStrechRect;
	CFullscreenPass  m_passSharpeningAndChromaticAberration;

};

//////////////////////////////////////////////////////////////////////////
class CBlurringPass : public IPostEffectPass
{
public:
	virtual void Init() override;
	virtual void Execute(const CPostEffectContext& context) override;

private:
	CStretchRectPass  m_passStrechRect;
	CGaussianBlurPass m_passGaussianBlur;
	CFullscreenPass   m_passBlurring;

};

//////////////////////////////////////////////////////////////////////////
class CUberGamePostEffectPass : public IPostEffectPass
{
public:
	virtual void Init() override;
	virtual void Execute(const CPostEffectContext& context) override;

private:
	CFullscreenPass m_passRadialBlurAndChromaShift;

};

//////////////////////////////////////////////////////////////////////////
class CFlashBangPass : public IPostEffectPass
{
public:
	virtual void Init() override;
	virtual void Execute(const CPostEffectContext& context) override;

private:
	CStretchRectPass m_passStrechRect;
	CFullscreenPass  m_passFlashBang;

};

//////////////////////////////////////////////////////////////////////////
class CPostStereoPass : public IPostEffectPass
{
public:
	virtual void Init() override;
	virtual void Execute(const CPostEffectContext& context) override;

private:
	CFullscreenPass m_passNearMask;
	CFullscreenPass m_passPostStereo;

	SamplerStateHandle m_samplerLinearMirror = EDefaultSamplerStates::Unspecified;

};

//////////////////////////////////////////////////////////////////////////
class CKillCameraPass : public IPostEffectPass
{
public:
	CKillCameraPass();
	~CKillCameraPass();

	virtual void Init() override;
	virtual void Execute(const CPostEffectContext& context) override;

private:
	CFullscreenPass m_passKillCameraFilter;

	CTexture*       m_pNoiseTex = nullptr;

};

//////////////////////////////////////////////////////////////////////////
class CScreenBloodPass : public IPostEffectPass
{
public:
	CScreenBloodPass();
	~CScreenBloodPass();

	virtual void Init() override;
	virtual void Execute(const CPostEffectContext& context) override;

private:
	CFullscreenPass m_passScreenBlood;

	CTexture*       m_pWaterDropletsBumpTex = nullptr;

};

//////////////////////////////////////////////////////////////////////////
class CScreenFaderPass : public IPostEffectPass
{
public:
	CScreenFaderPass() = default;

	virtual void Init() override;
	virtual void Execute(const CPostEffectContext& context) override;

private:
	CFullscreenPass m_passScreenFader;

};

//////////////////////////////////////////////////////////////////////////
class CHudSilhouettesPass : public IPostEffectPass
{
public:
	virtual void Init() override;
	virtual void Execute(const CPostEffectContext& context) override;

private:
	void ExecuteDeferredSilhouettesOptimised(const CPostEffectContext& context, float fBlendParam, float fType, float fFillStrength);

private:
	CStretchRectPass m_passStrechRect;
	CFullscreenPass  m_passDeferredSilhouettesOptimised;

};

//////////////////////////////////////////////////////////////////////////
class CHud3DPass : public IPostEffectPass
{
public:
	CHud3DPass();
	~CHud3DPass();

	virtual void Init() override;
	virtual void Execute(const CPostEffectContext& context) override;

	void ExecuteFlashUpdate(const CPostEffectContext &context, class CHud3D & hud3d);

private:
	void ExecuteDownsampleHud4x4(const CPostEffectContext &context, class CHud3D & hud3d, CTexture * pDstRT);
	void ExecuteBloomTexUpdate(const CPostEffectContext &context, class CHud3D & hud3d);
	void ExecuteFinalPass(const CPostEffectContext &context, CTexture* pOutputRT, CTexture* pOutputDS, class CHud3D & hud3d);

	bool SetVertex(CRenderPrimitive& prim, struct SHudData& pData) const;
	void SetShaderParams(const CPostEffectContext &context, EShaderStage shaderStages, CRenderPrimitive::ConstantManager& constantManager, const struct SHudData& data, const class CHud3D& hud3d) const;

private:
	CPrimitiveRenderPass          m_passDownsampleHud4x4;
	CPrimitiveRenderPass          m_passUpdateBloom;
	CGaussianBlurPass             m_passBlurGaussian;
	CPrimitiveRenderPass          m_passRenderHud;

	std::vector<CRenderPrimitive*> m_downsamplePrimitiveArray;
	std::vector<CRenderPrimitive*> m_bloomPrimitiveArray;
	std::vector<CRenderPrimitive*> m_hudPrimitiveArray;
};
