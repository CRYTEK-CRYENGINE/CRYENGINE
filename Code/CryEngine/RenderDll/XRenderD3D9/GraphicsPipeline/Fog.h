// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"

class CFogStage : public CGraphicsPipelineStage
{
public:
	struct SForwardParams
	{
		Vec4  vfParams;
		Vec4  vfRampParams;
		Vec4  vfSunDir;
		Vec3  vfColGradBase;
		float padding0;
		Vec3  vfColGradDelta;
		float padding1;
		Vec4  vfColGradParams;
		Vec4  vfColGradRadial;
		// Fog shadows
		Vec4  vfShadowDarkening;
		Vec4  vfShadowDarkeningSunAmb;
	};

public:
	void Init() final;
	void Update() final;
	void Execute();

	void FillForwardParams(SForwardParams& forwardParams, bool enable = true) const;

private:
	void ExecuteVolumetricFogShadow();
	f32  GetFogCullDistance() const;

private:
	_smart_ptr<CTexture> m_pTexInterleaveSamplePattern;
	_smart_ptr<CTexture> m_pCloudShadowTex;

#if defined(VOLUMETRIC_FOG_SHADOWS)
	CFullscreenPass m_passVolFogShadowRaycast;
	CFullscreenPass m_passVolFogShadowHBlur;
	CFullscreenPass m_passVolFogShadowVBlur;
#endif
	CFullscreenPass m_passFog;
	CFullscreenPass m_passLightning;
};
