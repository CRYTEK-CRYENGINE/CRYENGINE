// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"
#include "ScaleformPlayback.h"

#if RENDERER_SUPPORT_SCALEFORM
#include "ScaleformRender.h"
#include "DeviceManager/TempDynBuffer.h"

static const D3D11_INPUT_ELEMENT_DESC VertexDeclXY16i[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R16G16_SINT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDeclXY32f[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDeclXY16iC32[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R16G16_SINT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,  4, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDeclXY16iCF32[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R16G16_SINT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,  4, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    1, DXGI_FORMAT_R8G8B8A8_UNORM, 0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDeclGlyph[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

//////////////////////////////////////////////////////////////////////////

CRenderPrimitive* SSF_ResourcesD3D::CRenderPrimitiveHeap::GetUsablePrimitive(int key)
{
	CryAutoCriticalSectionNoRecursive threadSafe(m_lock);

	if (m_freeList.begin() == m_freeList.end())
		m_useList[key].emplace_front();
	else
		m_useList[key].splice_after(m_useList[key].before_begin(), m_freeList, m_freeList.before_begin());

	return &*m_useList[key].begin();
}

void SSF_ResourcesD3D::CRenderPrimitiveHeap::FreeUsedPrimitives(int key)
{
	CryAutoCriticalSectionNoRecursive threadSafe(m_lock);

	for (auto& prim : m_useList[key])
	{
		prim.Reset();
	}

	m_freeList.splice_after(m_freeList.before_begin(), m_useList[key]);
}

void SSF_ResourcesD3D::CRenderPrimitiveHeap::Clear()
{
	m_useList.clear();
	m_freeList.clear();
}

CConstantBuffer* SSF_ResourcesD3D::STransientConstantBufferHeap::GetUsableConstantBuffer()
{
	CryAutoCriticalSectionNoRecursive threadSafe(m_lock);

	if (m_freeList.begin() == m_freeList.end())
		m_useList.emplace_front(gRenDev->m_DevBufMan.CreateConstantBuffer(std::max(
			sizeof(SSF_GlobalDrawParams::SScaleformMeshAttributes),
			sizeof(SSF_GlobalDrawParams::SScaleformRenderParameters)
		)));
	else
		m_useList.splice_after(m_useList.before_begin(), m_freeList, m_freeList.before_begin());

	return *m_useList.begin();
}

void SSF_ResourcesD3D::STransientConstantBufferHeap::FreeUsedConstantBuffers()
{
	CryAutoCriticalSectionNoRecursive threadSafe(m_lock);

	m_freeList.splice_after(m_freeList.before_begin(), m_useList);
}

SSF_ResourcesD3D::STransientConstantBufferHeap::~STransientConstantBufferHeap()
{
	CRY_ASSERT(m_useList.empty());
}

//////////////////////////////////////////////////////////////////////////
SSF_ResourcesD3D::SSF_ResourcesD3D(CD3D9Renderer* pRenderer)
	: m_shTech_SolidColor("SolidColor")
	, m_shTech_GlyphMultiplyTexture("GlyphMultiplyTexture")
	, m_shTech_GlyphTexture("GlyphTexture")
	, m_shTech_GlyphAlphaTexture("GlyphAlphaTexture")
	, m_shTech_GlyphMultiplyTextureYUV("GlyphMultiplyTextureYUV")
	, m_shTech_GlyphTextureYUV("GlyphTextureYUV")
	, m_shTech_GlyphMultiplyTextureYUVA("GlyphMultiplyTextureYUVA")
	, m_shTech_GlyphTextureYUVA("GlyphTextureYUVA")
	, m_shTech_CxformMultiplyTexture("CxformMultiplyTexture")
	, m_shTech_CxformTexture("CxformTexture")
	, m_shTech_CxformGouraudMultiplyNoAddAlpha("CxformGouraudMultiplyNoAddAlpha")
	, m_shTech_CxformGouraudNoAddAlpha("CxformGouraudNoAddAlpha")
	, m_shTech_CxformGouraudMultiply("CxformGouraudMultiply")
	, m_shTech_CxformGouraud("CxformGouraud")
	, m_shTech_CxformGouraudMultiplyTexture("CxformGouraudMultiplyTexture")
	, m_shTech_CxformGouraudTexture("CxformGouraudTexture")
	, m_shTech_CxformMultiply2Texture("CxformMultiply2Texture")
	, m_shTech_Cxform2Texture("Cxform2Texture")
	, m_shTech_GlyphTextureMat("GlyphTextureMat")
	, m_shTech_GlyphTextureMatMul("GlyphTextureMatMul")
	, m_shTech_BlurFilter_Box1("BlurFilter_Box1")
	, m_shTech_BlurFilter_Box2("BlurFilter_Box2")
	, m_shTech_BlurFilterMul_Box1("BlurFilterMul_Box1")
	, m_shTech_BlurFilterMul_Box2("BlurFilterMul_Box2")
	, m_shTech_InnerShadow_Box2("InnerShadow_Box2")
	, m_shTech_InnerShadowHighlight_Box2("InnerShadowHighlight_Box2")
	, m_shTech_InnerShadowMul_Box2("InnerShadowMul_Box2")
	, m_shTech_InnerShadowMulHighlight_Box2("InnerShadowMulHighlight_Box2")
	, m_shTech_InnerShadowKnockout_Box2("InnerShadowKnockout_Box2")
	, m_shTech_InnerShadowHighlightKnockout_Box2("InnerShadowHighlightKnockout_Box2")
	, m_shTech_InnerShadowMulKnockout_Box2("InnerShadowMulKnockout_Box2")
	, m_shTech_InnerShadowMulHighlightKnockout_Box2("InnerShadowMulHighlightKnockout_Box2")
	, m_shTech_Shadow_Box2("Shadow_Box2")
	, m_shTech_ShadowHighlight_Box2("ShadowHighlight_Box2")
	, m_shTech_ShadowMul_Box2("ShadowMul_Box2")
	, m_shTech_ShadowMulHighlight_Box2("ShadowMulHighlight_Box2")
	, m_shTech_ShadowKnockout_Box2("ShadowKnockout_Box2")
	, m_shTech_ShadowHighlightKnockout_Box2("ShadowHighlightKnockout_Box2")
	, m_shTech_ShadowMulKnockout_Box2("ShadowMulKnockout_Box2")
	, m_shTech_ShadowMulHighlightKnockout_Box2("ShadowMulHighlightKnockout_Box2")
	, m_shTech_Shadowonly_Box2("Shadowonly_Box2")
	, m_shTech_ShadowonlyHighlight_Box2("ShadowonlyHighlight_Box2")
	, m_shTech_ShadowonlyMul_Box2("ShadowonlyMul_Box2")
	, m_shTech_ShadowonlyMulHighlight_Box2("ShadowonlyMulHighlight_Box2")

	, m_pShader(0)
	, m_fence(0)
{
	m_vertexDecls[IScaleformPlayback::Vertex_None     ] = InputLayoutHandle::Unspecified;
	m_vertexDecls[IScaleformPlayback::Vertex_XY16i    ] = InputLayoutHandle::Unspecified;
	m_vertexDecls[IScaleformPlayback::Vertex_XY32f    ] = InputLayoutHandle::Unspecified;
	m_vertexDecls[IScaleformPlayback::Vertex_XY16iC32 ] = InputLayoutHandle::Unspecified;
	m_vertexDecls[IScaleformPlayback::Vertex_XY16iCF32] = InputLayoutHandle::Unspecified;
	m_vertexDecls[IScaleformPlayback::Vertex_Glyph    ] = InputLayoutHandle::Unspecified;
	
	m_pVertexDecls[IScaleformPlayback::Vertex_None     ] = nullptr;
	m_pVertexDecls[IScaleformPlayback::Vertex_XY16i    ] = nullptr;
	m_pVertexDecls[IScaleformPlayback::Vertex_XY32f    ] = nullptr;
	m_pVertexDecls[IScaleformPlayback::Vertex_XY16iC32 ] = nullptr;
	m_pVertexDecls[IScaleformPlayback::Vertex_XY16iCF32] = nullptr;
	m_pVertexDecls[IScaleformPlayback::Vertex_Glyph    ] = nullptr;

	m_FilterTechnique[SSF_GlobalDrawParams::None                               ] = nullptr;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2InnerShadow                    ] = &m_shTech_InnerShadow_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2InnerShadowHighlight           ] = &m_shTech_InnerShadowHighlight_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2InnerShadowMul                 ] = &m_shTech_InnerShadowMul_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2InnerShadowMulHighlight        ] = &m_shTech_InnerShadowMulHighlight_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2InnerShadowKnockout            ] = &m_shTech_InnerShadowKnockout_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2InnerShadowHighlightKnockout   ] = &m_shTech_InnerShadowHighlightKnockout_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2InnerShadowMulKnockout         ] = &m_shTech_InnerShadowMulKnockout_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2InnerShadowMulHighlightKnockout] = &m_shTech_InnerShadowMulHighlightKnockout_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2Shadow                         ] = &m_shTech_Shadow_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2ShadowHighlight                ] = &m_shTech_ShadowHighlight_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2ShadowMul                      ] = &m_shTech_ShadowMul_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2ShadowMulHighlight             ] = &m_shTech_ShadowMulHighlight_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2ShadowKnockout                 ] = &m_shTech_ShadowKnockout_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2ShadowHighlightKnockout        ] = &m_shTech_ShadowHighlightKnockout_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2ShadowMulKnockout              ] = &m_shTech_ShadowMulKnockout_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2ShadowMulHighlightKnockout     ] = &m_shTech_ShadowMulHighlightKnockout_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2Shadowonly                     ] = &m_shTech_Shadowonly_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2ShadowonlyHighlight            ] = &m_shTech_ShadowonlyHighlight_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2ShadowonlyMul                  ] = &m_shTech_ShadowonlyMul_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2ShadowonlyMulHighlight         ] = &m_shTech_ShadowonlyMulHighlight_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box1Blur                           ] = &m_shTech_BlurFilter_Box1;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2Blur                           ] = &m_shTech_BlurFilter_Box2;
	m_FilterTechnique[SSF_GlobalDrawParams::Box1BlurMul                        ] = &m_shTech_BlurFilterMul_Box1;
	m_FilterTechnique[SSF_GlobalDrawParams::Box2BlurMul                        ] = &m_shTech_BlurFilterMul_Box2;

	m_FillTechnique[SSF_GlobalDrawParams::SolidColor        ][false] = &m_shTech_SolidColor;
	m_FillTechnique[SSF_GlobalDrawParams::SolidColor        ][true ] = &m_shTech_SolidColor;
	m_FillTechnique[SSF_GlobalDrawParams::Texture           ][false] = &m_shTech_CxformTexture;
	m_FillTechnique[SSF_GlobalDrawParams::Texture           ][true ] = &m_shTech_CxformMultiplyTexture;
	m_FillTechnique[SSF_GlobalDrawParams::GlyphTexture      ][false] = &m_shTech_GlyphTexture;
	m_FillTechnique[SSF_GlobalDrawParams::GlyphTexture      ][true ] = &m_shTech_GlyphMultiplyTexture;
	m_FillTechnique[SSF_GlobalDrawParams::GlyphAlphaTexture ][false] = &m_shTech_GlyphAlphaTexture;
	m_FillTechnique[SSF_GlobalDrawParams::GlyphAlphaTexture ][true ] = &m_shTech_GlyphAlphaTexture;
	m_FillTechnique[SSF_GlobalDrawParams::GlyphTextureMat   ][false] = &m_shTech_GlyphTextureMat;
	m_FillTechnique[SSF_GlobalDrawParams::GlyphTextureMat   ][true ] = &m_shTech_GlyphTextureMat;
	m_FillTechnique[SSF_GlobalDrawParams::GlyphTextureMatMul][false] = &m_shTech_GlyphTextureMatMul;
	m_FillTechnique[SSF_GlobalDrawParams::GlyphTextureMatMul][true ] = &m_shTech_GlyphTextureMatMul;
	m_FillTechnique[SSF_GlobalDrawParams::GlyphTextureYUV   ][false] = &m_shTech_GlyphTextureYUV;
	m_FillTechnique[SSF_GlobalDrawParams::GlyphTextureYUV   ][true ] = &m_shTech_GlyphMultiplyTextureYUV;
	m_FillTechnique[SSF_GlobalDrawParams::GlyphTextureYUVA  ][false] = &m_shTech_GlyphTextureYUVA;
	m_FillTechnique[SSF_GlobalDrawParams::GlyphTextureYUVA  ][true ] = &m_shTech_GlyphMultiplyTextureYUVA;
	m_FillTechnique[SSF_GlobalDrawParams::GColor            ][false] = &m_shTech_CxformGouraud;
	m_FillTechnique[SSF_GlobalDrawParams::GColor            ][true ] = &m_shTech_CxformGouraudMultiply;
	m_FillTechnique[SSF_GlobalDrawParams::GColorNoAddAlpha  ][false] = &m_shTech_CxformGouraudNoAddAlpha;
	m_FillTechnique[SSF_GlobalDrawParams::GColorNoAddAlpha  ][true ] = &m_shTech_CxformGouraudMultiplyNoAddAlpha;
	m_FillTechnique[SSF_GlobalDrawParams::G1Texture         ][false] = &m_shTech_CxformGouraudTexture;
	m_FillTechnique[SSF_GlobalDrawParams::G1Texture         ][true ] = &m_shTech_CxformGouraudMultiplyTexture;
	m_FillTechnique[SSF_GlobalDrawParams::G1TextureColor    ][false] = &m_shTech_CxformGouraudTexture;
	m_FillTechnique[SSF_GlobalDrawParams::G1TextureColor    ][true ] = &m_shTech_CxformGouraudMultiplyTexture;
	m_FillTechnique[SSF_GlobalDrawParams::G2Texture         ][false] = &m_shTech_Cxform2Texture;
	m_FillTechnique[SSF_GlobalDrawParams::G2Texture         ][true ] = &m_shTech_CxformMultiply2Texture;
}

SSF_ResourcesD3D::~SSF_ResourcesD3D()
{
	SAFE_RELEASE(m_pVertexDecls[IScaleformPlayback::Vertex_None     ]);
	SAFE_RELEASE(m_pVertexDecls[IScaleformPlayback::Vertex_XY16i    ]);
	SAFE_RELEASE(m_pVertexDecls[IScaleformPlayback::Vertex_XY32f    ]);
	SAFE_RELEASE(m_pVertexDecls[IScaleformPlayback::Vertex_XY16iC32 ]);
	SAFE_RELEASE(m_pVertexDecls[IScaleformPlayback::Vertex_XY16iCF32]);
	SAFE_RELEASE(m_pVertexDecls[IScaleformPlayback::Vertex_Glyph    ]);

	GetDeviceObjectFactory().ReleaseFence(m_fence);

	m_PrimitiveHeap.Clear();

	for (auto& pTexture : m_renderTargets)
		SAFE_RELEASE(pTexture);
}

CShader* SSF_ResourcesD3D::GetShader(CD3D9Renderer* pRenderer)
{
	return pRenderer->m_cEF.s_ShaderScaleForm;
}

CTexture* SSF_ResourcesD3D::GetColorSurface(CD3D9Renderer* pRenderer, int nWidth, int nHeight, ETEX_Format eFormat, int nMaxWidth, int nMaxHeight)
{
	CryCriticalSectionNonRecursive threadSafePool;

	CTexture* pTex = NULL;
	uint32 i;
	int nBestX = -1;
	int nBestY = -1;
	for (i = 0; i < m_renderTargets.size(); i++)
	{
		pTex = m_renderTargets[i];
		if ((pTex->GetRefCounter() <= 1))
		{
			if (pTex->GetWidth() == nWidth && pTex->GetHeight() == nHeight)
			{
				nBestX = i;
				break;
			}
		//	if (!bExactMatch)
			if (pTex->GetWidth() <= nMaxWidth && pTex->GetHeight() <= nMaxHeight)
			{
				if (nBestX < 0 && pTex->GetWidth() == nWidth && pTex->GetHeight() >= nHeight)
					nBestX = i;
				else if (nBestY < 0 && pTex->GetWidth() >= nWidth && pTex->GetHeight() == nHeight)
					nBestY = i;
			}
		}
	}
	if (nBestX >= 0)
		return m_renderTargets[nBestX];
	if (nBestY >= 0)
		return m_renderTargets[nBestY];

	bool allowUsingLargerRT = true;

#if defined(OGL_DO_NOT_ALLOW_LARGER_RT)
	allowUsingLargerRT = false;
#elif defined(DX11_ALLOW_D3D_DEBUG_RUNTIME)
	if (pRenderer->CV_r_EnableDebugLayer)
		allowUsingLargerRT = false;
#endif
//	if (bExactMatch)
//		allowUsingLargerRT = false;

	if (allowUsingLargerRT)
	{
		for (i = 0; i < m_renderTargets.size(); i++)
		{
			pTex = m_renderTargets[i];
			if ((pTex->GetRefCounter() <= 1))
			{
				if (pTex->GetWidth() <= nMaxWidth && pTex->GetHeight() <= nMaxHeight)
				{
					if (pTex->GetWidth() >= nWidth && pTex->GetHeight() >= nHeight)
						break;
				}
			}
		}
	}
	else
	{
		i = m_renderTargets.size();
	}

	if (i == m_renderTargets.size())
	{
		//allocate new RT
		int texID = pRenderer->CreateRenderTarget(nWidth, nHeight, Clr_Transparent, eFormat);
		pTex = static_cast<CTexture*>(pRenderer->EF_GetTextureByID(texID));

		CDeviceCommandListRef commandList = GetDeviceObjectFactory().GetCoreCommandList();
		commandList.GetGraphicsInterface()->ClearSurface(pTex->GetDevTexture()->LookupRTV(EDefaultResourceViews::RasterizerTarget), Clr_Transparent);

		m_renderTargets.push_back(pTex);
	}

	return pTex;
}

SDepthTexture* SSF_ResourcesD3D::GetStencilSurface(CD3D9Renderer* pRenderer, int nWidth, int nHeight, ETEX_Format eFormat)
{
	CryCriticalSectionNonRecursive threadSafePool;

	return pRenderer->FX_GetDepthSurface(nWidth, nHeight, false, false);
}

//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::SF_CreateResources()
{
	if (!m_pSFResD3D)
	{
		m_pSFResD3D = new SSF_ResourcesD3D(this);
	}
}

SSF_ResourcesD3D& CD3D9Renderer::SF_GetResources()
{
	assert(m_pSFResD3D);
	return *m_pSFResD3D;
}

void CD3D9Renderer::SF_ResetResources()
{
	if (m_pSFResD3D && m_pSFResD3D->m_fence)
	{
		GetDeviceObjectFactory().ReleaseFence(m_pSFResD3D->m_fence);
	}
}

void CD3D9Renderer::SF_DestroyResources()
{
	SAFE_DELETE(m_pSFResD3D);
}

void CD3D9Renderer::SF_PrecacheShaders()
{
	SSF_ResourcesD3D& Res = SF_GetResources();
	CShader* pShader(Res.GetShader(this));
	if (!pShader)
		return;

	SShaderCombination cmb;
	pShader->mfPrecache(cmb, true, NULL);

	auto shaderInfoXY16i     = SDeviceObjectHelpers::GetShaderInstanceInfo(pShader, Res.m_shTech_SolidColor             , 0, 0, 0, nullptr, false);
	auto shaderInfoXY32f     = SDeviceObjectHelpers::GetShaderInstanceInfo(pShader, Res.m_shTech_SolidColor             , 0, 0, 0, nullptr, false);
	auto shaderInfoXY16iC32  = SDeviceObjectHelpers::GetShaderInstanceInfo(pShader, Res.m_shTech_CxformGouraudNoAddAlpha, 0, 0, 0, nullptr, false);
	auto shaderInfoXY16iCF32 = SDeviceObjectHelpers::GetShaderInstanceInfo(pShader, Res.m_shTech_CxformGouraud          , 0, 0, 0, nullptr, false);
	auto shaderInfoGlyph     = SDeviceObjectHelpers::GetShaderInstanceInfo(pShader, Res.m_shTech_GlyphTexture           , 0, 0, 0, nullptr, false);

	auto* pInstanceXY16i     = reinterpret_cast<CHWShader_D3D::SHWSInstance*>(shaderInfoXY16i    [eHWSC_Vertex].pHwShaderInstance);
	auto* pInstanceXY32f     = reinterpret_cast<CHWShader_D3D::SHWSInstance*>(shaderInfoXY32f    [eHWSC_Vertex].pHwShaderInstance);
	auto* pInstanceXY16iC32  = reinterpret_cast<CHWShader_D3D::SHWSInstance*>(shaderInfoXY16iC32 [eHWSC_Vertex].pHwShaderInstance);
	auto* pInstanceXY16iCF32 = reinterpret_cast<CHWShader_D3D::SHWSInstance*>(shaderInfoXY16iCF32[eHWSC_Vertex].pHwShaderInstance);
	auto* pInstanceGlyph     = reinterpret_cast<CHWShader_D3D::SHWSInstance*>(shaderInfoGlyph    [eHWSC_Vertex].pHwShaderInstance);
	
	Res.m_vertexDecls[IScaleformPlayback::Vertex_XY16i    ] = CDeviceObjectFactory::GetOrCreateInputLayoutHandle(&pInstanceXY16i    ->m_Shader, 1, VertexDeclXY16i    );
	Res.m_vertexDecls[IScaleformPlayback::Vertex_XY32f    ] = CDeviceObjectFactory::GetOrCreateInputLayoutHandle(&pInstanceXY32f    ->m_Shader, 1, VertexDeclXY32f    );
	Res.m_vertexDecls[IScaleformPlayback::Vertex_XY16iC32 ] = CDeviceObjectFactory::GetOrCreateInputLayoutHandle(&pInstanceXY16iC32 ->m_Shader, 2, VertexDeclXY16iC32 );
	Res.m_vertexDecls[IScaleformPlayback::Vertex_XY16iCF32] = CDeviceObjectFactory::GetOrCreateInputLayoutHandle(&pInstanceXY16iCF32->m_Shader, 3, VertexDeclXY16iCF32);
	Res.m_vertexDecls[IScaleformPlayback::Vertex_Glyph    ] = CDeviceObjectFactory::GetOrCreateInputLayoutHandle(&pInstanceGlyph    ->m_Shader, 3, VertexDeclGlyph    );

	const SamplerStateHandle texStateID[8] =
	{
		EDefaultSamplerStates::PointWrap    , EDefaultSamplerStates::PointClamp,
		EDefaultSamplerStates::LinearWrap   , EDefaultSamplerStates::LinearClamp,
		EDefaultSamplerStates::TrilinearWrap, EDefaultSamplerStates::TrilinearClamp,
		EDefaultSamplerStates::Unspecified  , EDefaultSamplerStates::Unspecified
	};

	memcpy(&Res.samplerStateHandles, &texStateID, sizeof(texStateID));
}

//////////////////////////////////////////////////////////////////////////
IScaleformPlayback* CRenderer::SF_CreatePlayback() const
{
	return new CScaleformPlayback();
}

void CRenderer::SF_Playback(IScaleformPlayback* pRenderer, GRendererCommandBufferReadOnly* pBuffer) const
{
	CScaleformPlayback* xRenderer = (CScaleformPlayback*)pRenderer;
	::SF_Playback<CScaleformPlayback>(xRenderer, pBuffer);
}

void CRenderer::SF_Drain(GRendererCommandBufferReadOnly* pBuffer) const
{
	CScaleformSink nullRenderer;
	::SF_Playback<CScaleformSink>(&nullRenderer, pBuffer);
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::SF_ConfigMask(int st, uint32 ref)
{
	// Graphics pipeline == 0
	FX_SetStencilState(st, ref, 0xFFFFFFFF, 0xFFFFFFFF);
}

//////////////////////////////////////////////////////////////////////////
void CRenderer::SF_GetMeshMaxSize(int& numVertices, int& numIndices) const
{
	numVertices = m_RP.m_MaxVerts;
	numIndices = m_RP.m_MaxTris * 3;
}

//////////////////////////////////////////////////////////////////////////
bool CD3D9Renderer::SF_SetVertexDeclaration(IScaleformPlayback::VertexFormat vertexFmt)
{
	const SSF_ResourcesD3D& sfRes(SF_GetResources());

	D3DVertexDeclaration* pVD = CDeviceObjectFactory::LookupInputLayout(sfRes.m_vertexDecls[vertexFmt]).second;
	assert(pVD);
	if (!pVD)
		return false;

	if (m_pLastVDeclaration != pVD)
	{
		m_pLastVDeclaration = pVD;
		m_DevMan.BindVtxDecl(pVD);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
CShader* CD3D9Renderer::SF_SetTechnique(const CCryNameTSCRC& techName)
{
	FUNCTION_PROFILER_RENDER_FLAT
		assert(gRenDev->m_pRT->IsRenderThread());

	CShader* pShader(SF_GetResources().GetShader(this));
	if (!pShader)
		return 0;

	SShaderTechnique* pTech(0);
	uint32 i(0);
	for (; i < pShader->m_HWTechniques.Num(); ++i)
	{
		pTech = pShader->m_HWTechniques[i];
		if (techName == pTech->m_NameCRC)
			break;
	}

	if (i == pShader->m_HWTechniques.Num())
		return 0;

	CRenderer* rd(gRenDev);
	rd->m_RP.m_pShader = pShader;
	rd->m_RP.m_nShaderTechnique = i;
	rd->m_RP.m_pCurTechnique = pShader->m_HWTechniques[i];

	return pShader;
}

//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::SF_SetBlendOp(SSF_GlobalDrawParams::EAlphaBlendOp blendOp, bool reset)
{
	if (CV_r_measureoverdraw)
		return;

	if (!reset)
	{
		if (blendOp != SSF_GlobalDrawParams::Add)
		{
			switch (blendOp)
			{
			case SSF_GlobalDrawParams::Substract:
			{
				SStateBlend bl = m_StatesBL[m_nCurStateBL];
				bl.Desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_SUBTRACT;
				bl.Desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_SUBTRACT;
				SetBlendState(&bl);
			}
			break;
			case SSF_GlobalDrawParams::RevSubstract:
			{
				SStateBlend bl = m_StatesBL[m_nCurStateBL];
				bl.Desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_REV_SUBTRACT;
				bl.Desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_REV_SUBTRACT;
				SetBlendState(&bl);
			}
			break;
			case SSF_GlobalDrawParams::Min:
			{
				SStateBlend bl = m_StatesBL[m_nCurStateBL];
				bl.Desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MIN;
				bl.Desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MIN;
				SetBlendState(&bl);
			}
			break;
			case SSF_GlobalDrawParams::Max:
			{
				SStateBlend bl = m_StatesBL[m_nCurStateBL];
				bl.Desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MAX;
				bl.Desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;
				SetBlendState(&bl);
			}
			break;
			default:
				assert(0);
				break;
			}
		}
	}
	else
	{
		if (blendOp != SSF_GlobalDrawParams::Add)
		{
			SStateBlend bl = m_StatesBL[m_nCurStateBL];
			bl.Desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			bl.Desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			SetBlendState(&bl);
		}
	}
}

uint32 CD3D9Renderer::SF_AdjustBlendStateForMeasureOverdraw(uint32 blendModeStates)
{
	if (CV_r_measureoverdraw)
	{
		blendModeStates = (blendModeStates & ~GS_BLEND_MASK) | (GS_BLSRC_ONE | GS_BLDST_ONE);
		blendModeStates &= ~GS_ALPHATEST;
	}
	return blendModeStates;
}

//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::SF_HandleClear(const SSF_GlobalDrawParams& __restrict params)
{
	SSF_GlobalDrawParams::OutputParams& __restrict rCurOutput = *params.pRenderOutput;

	if (rCurOutput.bRenderTargetClear || rCurOutput.bStencilTargetClear)
	{
		RECT rect =
		{
			LONG(params.viewport.TopLeftX),
			LONG(params.viewport.TopLeftY),
			LONG(params.viewport.TopLeftX + params.viewport.Width),
			LONG(params.viewport.TopLeftY + params.viewport.Height)
		};

		// Graphics pipeline >= 1
		if (CRenderer::CV_r_GraphicsPipeline > 0)
		{
			if (rCurOutput.renderPass.GetPrimitiveCount() >= 1)
			{
				rCurOutput.renderPass.Execute();
				rCurOutput.renderPass.BeginAddingPrimitives();
			}
		}

		if (rCurOutput.bRenderTargetClear)
			rCurOutput.clearPass.Execute(rCurOutput.pRenderTarget, Clr_Transparent, 1, &rect);
		if (rCurOutput.bStencilTargetClear)
			rCurOutput.clearPass.Execute(rCurOutput.pStencilTarget, CLEAR_STENCIL, Clr_Unused.r, 0, 1, &rect);
	}

	rCurOutput.bRenderTargetClear = false;
	rCurOutput.bStencilTargetClear = false;
}

//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::SF_DrawIndexedTriList(int baseVertexIndex, int minVertexIndex, int numVertices, int startIndex, int triangleCount, const SSF_GlobalDrawParams& __restrict params)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SYSTEM);

	if (IsDeviceLost())
		return;

	SRenderPipeline& RESTRICT_REFERENCE rRP = m_RP;
	SThreadInfo& RESTRICT_REFERENCE rTI = rRP.m_TI[rRP.m_nProcessThreadID];

	assert(params.vtxData->VertexFormat != IScaleformPlayback::Vertex_Glyph && params.vtxData->VertexFormat != IScaleformPlayback::Vertex_None);

	SSF_ResourcesD3D& sfRes(SF_GetResources());

	// Update constant buffer. NOTE: buffer is assigned to preallocated primitives
	if (params.m_bScaleformMeshAttributesDirty)
		params.m_vsBuffer = sfRes.m_CBHeap.GetUsableConstantBuffer(),
		params.m_vsBuffer->UpdateBuffer(params.m_pScaleformMeshAttributes, params.m_ScaleformMeshAttributesSize),
		params.m_bScaleformMeshAttributesDirty = false;
	if (params.m_bScaleformRenderParametersDirty)
		params.m_psBuffer = sfRes.m_CBHeap.GetUsableConstantBuffer(),
		params.m_psBuffer->UpdateBuffer(params.m_pScaleformRenderParameters, params.m_ScaleformRenderParametersSize),
		params.m_bScaleformRenderParametersDirty = false;

	// Set appropriate shader
	SSF_GlobalDrawParams::EFillType fillType(params.fillType);
	if (fillType == SSF_GlobalDrawParams::GColor && params.vtxData->VertexFormat == IScaleformPlayback::Vertex_XY16iC32)
		fillType = SSF_GlobalDrawParams::GColorNoAddAlpha;

	const CCryNameTSCRC techName = *sfRes.m_FillTechnique[fillType][params.isMultiplyDarkBlendMode];

	SF_HandleClear(params);
	if (CRenderer::CV_r_GraphicsPipeline > 0)
	{
		CPrimitiveRenderPass& __restrict primPass = params.pRenderOutput->renderPass;

		// Initial markup
		CRenderPrimitive* primInit = sfRes.m_PrimitiveHeap.GetUsablePrimitive(params.pRenderOutput->key);

		primInit->SetTechnique(m_cEF.s_ShaderScaleForm, techName, 0);
		primInit->SetRenderState(SF_AdjustBlendStateForMeasureOverdraw(params.blendModeStates) | GS_NODEPTHTEST | params.renderMaskedStates);
		primInit->SetStencilState(params.m_stencil.func, params.m_stencil.ref, uint8(~0U), uint8(~0U));
		primInit->SetCullMode(eCULL_None);

		primInit->SetInlineConstantBuffer(eConstantBufferShaderSlot_ScaleformMeshAttributes, params.m_vsBuffer, EShaderStage_Vertex);
		primInit->SetInlineConstantBuffer(eConstantBufferShaderSlot_ScaleformRenderParameters, params.m_psBuffer, EShaderStage_Pixel);
		primInit->SetSampler(0, sfRes.samplerStateHandles[params.texture[0].texState], EShaderStage_Pixel);
		primInit->SetSampler(1, sfRes.samplerStateHandles[params.texture[1].texState], EShaderStage_Pixel);
		primInit->SetTexture(0, params.texture[0].pTex, EDefaultResourceViews::Default, EShaderStage_Pixel);
		primInit->SetTexture(1, params.texture[1].pTex, EDefaultResourceViews::Default, EShaderStage_Pixel);

		primInit->SetPrimitiveType(CRenderPrimitive::ePrim_Custom);
		primInit->SetCustomVertexStream(params.vtxData->DeviceDataHandle, params.vtxData->eVertexFormat, params.vtxData->StrideSize);
		primInit->SetCustomIndexStream(params.idxData->DeviceDataHandle, RenderIndexType::Index16);
		primInit->SetDrawInfo(eptTriangleList, baseVertexIndex, startIndex, params.idxData->NumElements);
		primInit->Compile(primPass);

		primPass.AddPrimitive(primInit);

		return;
	}

	CShader* pSFShader = SF_SetTechnique(techName);
	if (!pSFShader)
		return;

	if (params.renderMaskedStates & GS_COLMASK_NONE)
	{
		rRP.m_PersFlags2 |= RBPF2_DISABLECOLORWRITES;
		rRP.m_StateOr |= GS_COLMASK_NONE;
	}

	{
		//FRAME_PROFILER("SF_DITL::FxBegin", gEnv->pSystem, PROFILE_SYSTEM);

		uint32 numPasses(0);
		pSFShader->FXBegin(&numPasses, /*FEF_DONTSETTEXTURES |*/ FEF_DONTSETSTATES);
		if (!numPasses)
		{
			return;
		}

		if (!pSFShader->FXBeginPass(0))
		{
			rRP.m_PersFlags2 &= ~RBPF2_DISABLECOLORWRITES;
			rRP.m_StateOr &= ~GS_COLMASK_NONE;
			pSFShader->FXEndPass();
			pSFShader->FXEnd();
			return;
		}
	}
	{
		//FRAME_PROFILER("SF_DITL::SetState", gEnv->pSystem, PROFILE_SYSTEM);

		// Set states
		FX_SetState(SF_AdjustBlendStateForMeasureOverdraw(params.blendModeStates) | /*GS_NODEPTHTEST | */ params.renderMaskedStates);
		D3DSetCull(eCULL_None);
	}
	{
		//FRAME_PROFILER("SF_DITL::FX_Commit", gEnv->pSystem, PROFILE_SYSTEM);

		m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_VS, params.m_vsBuffer, 0);
		m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_PS, params.m_psBuffer, 0);

		CTexture::SetSampler(sfRes.samplerStateHandles[params.texture[0].texState], 0, eHWSC_Pixel);
		CTexture::SetSampler(sfRes.samplerStateHandles[params.texture[1].texState], 1, eHWSC_Pixel);

		params.texture[0].pTex->ApplyTexture(0, eHWSC_Pixel, EDefaultResourceViews::Default);
		params.texture[1].pTex->ApplyTexture(1, eHWSC_Pixel, EDefaultResourceViews::Default);

		// Commit all render changes
		FX_Commit();
	}
	{
		//FRAME_PROFILER("SF_DITL::SetVertexDecl", gEnv->pSystem, PROFILE_SYSTEM);

		// Set vertex declaration
		if (!SF_SetVertexDeclaration(params.vtxData->VertexFormat))
		{
			rRP.m_PersFlags2 &= ~RBPF2_DISABLECOLORWRITES;
			rRP.m_StateOr &= ~GS_COLMASK_NONE;
			pSFShader->FXEndPass();
			pSFShader->FXEnd();
			return;
		}
	}

	// Copy vertex data...
	{
		{
			buffer_size_t bufferOffset = ~0;
			D3DVertexBuffer* pVB = gRenDev->m_DevBufMan.GetD3DVB(params.vtxData->DeviceDataHandle, &bufferOffset);
			gcpRendD3D->FX_SetVStream(0, pVB, bufferOffset, params.vtxData->StrideSize);
		}

		{
			buffer_size_t bufferOffset = ~0;
			D3DIndexBuffer* pIB = gRenDev->m_DevBufMan.GetD3DIB(params.idxData->DeviceDataHandle, &bufferOffset);
			gcpRendD3D->FX_SetIStream(pIB, bufferOffset, Index16);
		}
	}
	{
		//FRAME_PROFILER("SF_DITL::BlendStateAndDraw", gEnv->pSystem, PROFILE_SYSTEM);

		// Override blend op if necessary
		SF_SetBlendOp(params.blendOp);

		// Submit draw call
		FX_DrawIndexedPrimitive(eptTriangleList, baseVertexIndex, minVertexIndex, numVertices, startIndex, triangleCount * 3);

		// Reset overridden blend op if necessary
		SF_SetBlendOp(params.blendOp, true);
	}
	{
		//FRAME_PROFILER("SF_DITL::FXEnd", gEnv->pSystem, PROFILE_SYSTEM);

		// End shader pass
		pSFShader->FXEndPass();
		pSFShader->FXEnd();
	}

	if (params.renderMaskedStates & GS_COLMASK_NONE)
	{
		rRP.m_PersFlags2 &= ~RBPF2_DISABLECOLORWRITES;
		rRP.m_StateOr &= ~GS_COLMASK_NONE;
	}

}

//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::SF_DrawLineStrip(int baseVertexIndex, int lineCount, const SSF_GlobalDrawParams& __restrict params)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SYSTEM);

	if (IsDeviceLost())
		return;

	assert(params.vtxData->VertexFormat == IScaleformPlayback::Vertex_XY16i);

	SRenderPipeline& RESTRICT_REFERENCE rRP = m_RP;

	SSF_ResourcesD3D& sfRes(SF_GetResources());

	// Update constant buffer. NOTE: buffer is assigned to preallocated primitives
	if (params.m_bScaleformMeshAttributesDirty)
		params.m_vsBuffer = sfRes.m_CBHeap.GetUsableConstantBuffer(),
		params.m_vsBuffer->UpdateBuffer(params.m_pScaleformMeshAttributes, params.m_ScaleformMeshAttributesSize),
		params.m_bScaleformMeshAttributesDirty = false;
	if (params.m_bScaleformRenderParametersDirty)
		params.m_psBuffer = sfRes.m_CBHeap.GetUsableConstantBuffer(),
		params.m_psBuffer->UpdateBuffer(params.m_pScaleformRenderParameters, params.m_ScaleformRenderParametersSize),
		params.m_bScaleformRenderParametersDirty = false;

	// Set appropriate shader
	const CCryNameTSCRC techName = *sfRes.m_FillTechnique[params.fillType][false];

	SF_HandleClear(params);
	if (CRenderer::CV_r_GraphicsPipeline > 0)
	{
		CPrimitiveRenderPass& __restrict primPass = params.pRenderOutput->renderPass;

		// Initial markup
		CRenderPrimitive* primInit = sfRes.m_PrimitiveHeap.GetUsablePrimitive(params.pRenderOutput->key);

		primInit->SetTechnique(m_cEF.s_ShaderScaleForm, techName, 0);
		primInit->SetRenderState(SF_AdjustBlendStateForMeasureOverdraw(params.blendModeStates) | GS_NODEPTHTEST | params.renderMaskedStates);
		primInit->SetStencilState(params.m_stencil.func, params.m_stencil.ref, uint8(~0U), uint8(~0U));
		primInit->SetCullMode(eCULL_None);

		primInit->SetInlineConstantBuffer(eConstantBufferShaderSlot_ScaleformMeshAttributes, params.m_vsBuffer, EShaderStage_Vertex);
		primInit->SetInlineConstantBuffer(eConstantBufferShaderSlot_ScaleformRenderParameters, params.m_psBuffer, EShaderStage_Pixel);
		primInit->SetSampler(0, sfRes.samplerStateHandles[params.texture[0].texState], EShaderStage_Pixel);
		primInit->SetSampler(1, sfRes.samplerStateHandles[params.texture[1].texState], EShaderStage_Pixel);
		primInit->SetTexture(0, params.texture[0].pTex, EDefaultResourceViews::Default, EShaderStage_Pixel);
		primInit->SetTexture(1, params.texture[1].pTex, EDefaultResourceViews::Default, EShaderStage_Pixel);

		primInit->SetPrimitiveType(CRenderPrimitive::ePrim_Custom);
		primInit->SetCustomVertexStream(params.vtxData->DeviceDataHandle, params.vtxData->eVertexFormat, params.vtxData->StrideSize);
		primInit->SetCustomIndexStream(~0u, RenderIndexType(0));
		primInit->SetDrawInfo(eptLineStrip, 0, baseVertexIndex, params.vtxData->NumElements);
		primInit->Compile(primPass);

		primPass.AddPrimitive(primInit);

		return;
	}

	CShader* pSFShader = SF_SetTechnique(techName);
	if (!pSFShader)
		return;

	{
		//FRAME_PROFILER("SF_DLS::FxBegin", gEnv->pSystem, PROFILE_SYSTEM);

		if (params.renderMaskedStates & GS_COLMASK_NONE)
		{
			rRP.m_PersFlags2 |= RBPF2_DISABLECOLORWRITES;
			rRP.m_StateOr |= GS_COLMASK_NONE;
		}
		uint32 numPasses(0);
		pSFShader->FXBegin(&numPasses, /*FEF_DONTSETTEXTURES |*/ FEF_DONTSETSTATES);
		if (!numPasses)
		{
			return;
		}
		pSFShader->FXBeginPass(0);
	}
	{
		//FRAME_PROFILER("SF_DLS::SetState", gEnv->pSystem, PROFILE_SYSTEM);

		// Set states
		FX_SetState(SF_AdjustBlendStateForMeasureOverdraw(params.blendModeStates) | /*GS_NODEPTHTEST | */ params.renderMaskedStates);
		D3DSetCull(eCULL_None);
	}
	{
		//FRAME_PROFILER("SF_DLS::FX_Commit", gEnv->pSystem, PROFILE_SYSTEM);
		
		m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_VS, params.m_vsBuffer, 0);
		m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_PS, params.m_psBuffer, 0);

		CTexture::SetSampler(sfRes.samplerStateHandles[params.texture[0].texState], 0, eHWSC_Pixel);
		CTexture::SetSampler(sfRes.samplerStateHandles[params.texture[1].texState], 1, eHWSC_Pixel);

		params.texture[0].pTex->ApplyTexture(0, eHWSC_Pixel, EDefaultResourceViews::Default);
		params.texture[1].pTex->ApplyTexture(1, eHWSC_Pixel, EDefaultResourceViews::Default);

		// Commit all render changes
		FX_Commit();
	}
	{
		//FRAME_PROFILER("SF_DLS::SetVertexDecl", gEnv->pSystem, PROFILE_SYSTEM);

		// Set vertex declaration
		if (!SF_SetVertexDeclaration(params.vtxData->VertexFormat))
		{
			rRP.m_PersFlags2 &= ~RBPF2_DISABLECOLORWRITES;
			rRP.m_StateOr &= ~GS_COLMASK_NONE;
			pSFShader->FXEndPass();
			pSFShader->FXEnd();
			return;
		}
	}

	// Copy vertex data...
	{
		{
			buffer_size_t bufferOffset = ~0;
			D3DVertexBuffer* pVB = gRenDev->m_DevBufMan.GetD3DVB(params.vtxData->DeviceDataHandle, &bufferOffset);
			gcpRendD3D->FX_SetVStream(0, pVB, bufferOffset, params.vtxData->StrideSize);
		}
	}
	{
		//FRAME_PROFILER("SF_DLS::BlendStateAndDraw", gEnv->pSystem, PROFILE_SYSTEM);

		// Override blend op if necessary
		SF_SetBlendOp(params.blendOp);

		// Submit draw call
		FX_DrawPrimitive(eptLineStrip, baseVertexIndex, params.vtxData->NumElements);

		// Reset overridden blend op if necessary
		SF_SetBlendOp(params.blendOp, true);
	}
	{
		//FRAME_PROFILER("SF_DLS::FXEnd", gEnv->pSystem, PROFILE_SYSTEM);

		// End shader pass
		pSFShader->FXEndPass();
		pSFShader->FXEnd();
	}

	if (params.renderMaskedStates & GS_COLMASK_NONE)
	{
		rRP.m_PersFlags2 &= ~RBPF2_DISABLECOLORWRITES;
		rRP.m_StateOr &= ~GS_COLMASK_NONE;
	}
}

//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::SF_DrawGlyphClear(const IScaleformPlayback::DeviceData* vtxData, int baseVertexIndex, const SSF_GlobalDrawParams& __restrict params)
{
	FUNCTION_PROFILER_RENDER_FLAT

	if (IsDeviceLost())
		return;

	SRenderPipeline& RESTRICT_REFERENCE rRP = m_RP;
	SThreadInfo& RESTRICT_REFERENCE rTI = rRP.m_TI[rRP.m_nProcessThreadID];

	assert(vtxData->VertexFormat == IScaleformPlayback::Vertex_Glyph || vtxData->VertexFormat == IScaleformPlayback::Vertex_XY16i);

	SSF_ResourcesD3D& sfRes(SF_GetResources());

	// Update constant buffer. NOTE: buffer is assigned to preallocated primitives
	if (params.m_bScaleformMeshAttributesDirty)
		params.m_vsBuffer = sfRes.m_CBHeap.GetUsableConstantBuffer(),
		params.m_vsBuffer->UpdateBuffer(params.m_pScaleformMeshAttributes, params.m_ScaleformMeshAttributesSize),
		params.m_bScaleformMeshAttributesDirty = false;
	if (params.m_bScaleformRenderParametersDirty)
		params.m_psBuffer = sfRes.m_CBHeap.GetUsableConstantBuffer(),
		params.m_psBuffer->UpdateBuffer(params.m_pScaleformRenderParameters, params.m_ScaleformRenderParametersSize),
		params.m_bScaleformRenderParametersDirty = false;

	// Set appropriate shader
	const CCryNameTSCRC techName = *sfRes.m_FillTechnique[params.fillType][params.isMultiplyDarkBlendMode];

	SF_HandleClear(params);
	if (CRenderer::CV_r_GraphicsPipeline > 0)
	{
		CPrimitiveRenderPass& __restrict primPass = params.pRenderOutput->renderPass;

		// Initial markup
		CRenderPrimitive* primInit = sfRes.m_PrimitiveHeap.GetUsablePrimitive(params.pRenderOutput->key);

		primInit->SetTechnique(m_cEF.s_ShaderScaleForm, techName, 0);
		primInit->SetRenderState(SF_AdjustBlendStateForMeasureOverdraw(params.blendModeStates) | GS_NODEPTHTEST | params.renderMaskedStates);
		primInit->SetStencilState(params.m_stencil.func, params.m_stencil.ref, uint8(~0U), uint8(~0U));
		primInit->SetCullMode(eCULL_None);

		primInit->SetInlineConstantBuffer(eConstantBufferShaderSlot_ScaleformMeshAttributes, params.m_vsBuffer, EShaderStage_Vertex);
		primInit->SetInlineConstantBuffer(eConstantBufferShaderSlot_ScaleformRenderParameters, params.m_psBuffer, EShaderStage_Pixel);
		primInit->SetSampler(0, sfRes.samplerStateHandles[params.texture[0].texState], EShaderStage_Pixel);
		primInit->SetTexture(0, params.texture[0].pTex, EDefaultResourceViews::Default, EShaderStage_Pixel);
		if (params.fillType >= SSF_GlobalDrawParams::GlyphTextureYUV)
		{
			primInit->SetTexture(1, params.texture[1].pTex, EDefaultResourceViews::Default, EShaderStage_Pixel);
			primInit->SetTexture(2, params.texture[2].pTex, EDefaultResourceViews::Default, EShaderStage_Pixel);
			if (params.fillType >= SSF_GlobalDrawParams::GlyphTextureYUVA)
			{
				primInit->SetTexture(3, params.texture[3].pTex, EDefaultResourceViews::Default, EShaderStage_Pixel);
			}
		}

		primInit->SetPrimitiveType(CRenderPrimitive::ePrim_Custom);
		primInit->SetCustomVertexStream(vtxData->DeviceDataHandle, vtxData->eVertexFormat, vtxData->StrideSize);
		primInit->SetCustomIndexStream(~0u, RenderIndexType(0));
		primInit->SetDrawInfo(eptTriangleStrip, 0, baseVertexIndex, vtxData->NumElements);
		primInit->Compile(primPass);

		primPass.AddPrimitive(primInit);

		return;
	}

	CShader* pSFShader = SF_SetTechnique(techName);
	if (!pSFShader)
		return;

	{
		//FRAME_PROFILER("SF_DG::FxBegin", gEnv->pSystem, PROFILE_SYSTEM);

		if (params.renderMaskedStates & GS_COLMASK_NONE)
		{
			rRP.m_PersFlags2 |= RBPF2_DISABLECOLORWRITES;
			rRP.m_StateOr |= GS_COLMASK_NONE;
		}

		uint32 numPasses(0);
		pSFShader->FXBegin(&numPasses, /*FEF_DONTSETTEXTURES |*/ FEF_DONTSETSTATES);
		if (!numPasses)
		{
			return;
		}

		if (!pSFShader->FXBeginPass(0))
		{
			rRP.m_PersFlags2 &= ~RBPF2_DISABLECOLORWRITES;
			rRP.m_StateOr &= ~GS_COLMASK_NONE;
			pSFShader->FXEndPass();
			pSFShader->FXEnd();
			return;
		}
	}
	{
		//FRAME_PROFILER("SF_DG::SetState", gEnv->pSystem, PROFILE_SYSTEM);

		// Set states
		FX_SetState(SF_AdjustBlendStateForMeasureOverdraw(params.blendModeStates) | /*GS_NODEPTHTEST | */ params.renderMaskedStates);
		D3DSetCull(eCULL_None);
	}
	{
		//FRAME_PROFILER("SF_DG::FX_Commit", gEnv->pSystem, PROFILE_SYSTEM);
		
		m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_VS, params.m_vsBuffer, 0);
		m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_PS, params.m_psBuffer, 0);

		CTexture::SetSampler(sfRes.samplerStateHandles[params.texture[0].texState], 0, eHWSC_Pixel);

		params.texture[0].pTex->ApplyTexture(0, eHWSC_Pixel, EDefaultResourceViews::Default);
		if (params.fillType >= SSF_GlobalDrawParams::GlyphTextureYUV)
		{
			params.texture[1].pTex->ApplyTexture(1, eHWSC_Pixel, EDefaultResourceViews::Default);
			params.texture[2].pTex->ApplyTexture(2, eHWSC_Pixel, EDefaultResourceViews::Default);
			if (params.fillType >= SSF_GlobalDrawParams::GlyphTextureYUVA)
			{
				params.texture[3].pTex->ApplyTexture(3, eHWSC_Pixel, EDefaultResourceViews::Default);
			}
		}

		// Commit all render changes
		FX_Commit();
	}
	{
		//FRAME_PROFILER("SF_DG::SetVertexDecl", gEnv->pSystem, PROFILE_SYSTEM);

		// Set vertex declaration
		if (!SF_SetVertexDeclaration(vtxData->VertexFormat))
		{
			rRP.m_PersFlags2 &= ~RBPF2_DISABLECOLORWRITES;
			rRP.m_StateOr &= ~GS_COLMASK_NONE;
			pSFShader->FXEndPass();
			pSFShader->FXEnd();
			return;
		}
	}

	// Copy vertex data...
	{
		{
			buffer_size_t bufferOffset = ~0;
			D3DVertexBuffer* pVB = gRenDev->m_DevBufMan.GetD3DVB(vtxData->DeviceDataHandle, &bufferOffset);
			gcpRendD3D->FX_SetVStream(0, pVB, bufferOffset, vtxData->StrideSize);
		}
	}
	{
		//FRAME_PROFILER("SF_DG::BlendStateAndDraw", gEnv->pSystem, PROFILE_SYSTEM);

		// Override blend op if necessary
		SF_SetBlendOp(params.blendOp);

		// Submit draw call
		FX_DrawPrimitive(eptTriangleStrip, baseVertexIndex, vtxData->NumElements);

		// Reset overridden blend op if necessary
		SF_SetBlendOp(params.blendOp, true);
	}
	{
		//FRAME_PROFILER("SF_DG::FXEnd", gEnv->pSystem, PROFILE_SYSTEM);

		// End shader pass
		pSFShader->FXEndPass();
		pSFShader->FXEnd();
	}

	if (params.renderMaskedStates & GS_COLMASK_NONE)
	{
		rRP.m_PersFlags2 &= ~RBPF2_DISABLECOLORWRITES;
		rRP.m_StateOr &= ~GS_COLMASK_NONE;
	}
}

void CD3D9Renderer::SF_DrawBlurRect(const IScaleformPlayback::DeviceData* vtxData, const SSF_GlobalDrawParams& __restrict params)
{
	//TODO figure out how many blur shaders we need
	assert((params.blurType > SSF_GlobalDrawParams::BlurNone) && (params.blurType < SSF_GlobalDrawParams::BlurCount));

	SSF_ResourcesD3D& sfRes(SF_GetResources());

	// Update constant buffer. NOTE: buffer is assigned to preallocated primitives
	if (params.m_bScaleformMeshAttributesDirty)
		params.m_vsBuffer = sfRes.m_CBHeap.GetUsableConstantBuffer(),
		params.m_vsBuffer->UpdateBuffer(params.m_pScaleformMeshAttributes, params.m_ScaleformMeshAttributesSize),
		params.m_bScaleformMeshAttributesDirty = false;
	if (params.m_bScaleformRenderParametersDirty)
		params.m_psBuffer = sfRes.m_CBHeap.GetUsableConstantBuffer(),
		params.m_psBuffer->UpdateBuffer(params.m_pScaleformRenderParameters, params.m_ScaleformRenderParametersSize),
		params.m_bScaleformRenderParametersDirty = false;

	// Set appropriate shader
	const CCryNameTSCRC techName = *sfRes.m_FilterTechnique[params.blurType];

	SF_HandleClear(params);
	if (CRenderer::CV_r_GraphicsPipeline > 0)
	{
		CPrimitiveRenderPass& __restrict primPass = params.pRenderOutput->renderPass;

		// Initial markup
		CRenderPrimitive* primInit = sfRes.m_PrimitiveHeap.GetUsablePrimitive(params.pRenderOutput->key);

		primInit->SetTechnique(m_cEF.s_ShaderScaleForm, techName, 0);
		primInit->SetRenderState(SF_AdjustBlendStateForMeasureOverdraw(params.blendModeStates) | GS_NODEPTHTEST | params.renderMaskedStates);
		primInit->SetStencilState(params.m_stencil.func, params.m_stencil.ref, uint8(~0U), uint8(~0U));
		primInit->SetCullMode(eCULL_None);

		primInit->SetInlineConstantBuffer(eConstantBufferShaderSlot_ScaleformMeshAttributes, params.m_vsBuffer, EShaderStage_Vertex);
		primInit->SetInlineConstantBuffer(eConstantBufferShaderSlot_ScaleformRenderParameters, params.m_psBuffer, EShaderStage_Pixel);
		primInit->SetSampler(0, sfRes.samplerStateHandles[params.texture[0].texState], EShaderStage_Pixel);
		primInit->SetSampler(1, sfRes.samplerStateHandles[params.texture[1].texState], EShaderStage_Pixel);
		primInit->SetTexture(0, params.texture[0].pTex, EDefaultResourceViews::Default, EShaderStage_Pixel);
		primInit->SetTexture(1, params.texture[1].pTex, EDefaultResourceViews::Default, EShaderStage_Pixel);

		primInit->SetPrimitiveType(CRenderPrimitive::ePrim_Custom);
		primInit->SetCustomVertexStream(vtxData->DeviceDataHandle, vtxData->eVertexFormat, vtxData->StrideSize);
		primInit->SetCustomIndexStream(~0u, RenderIndexType(0));
		primInit->SetDrawInfo(eptTriangleStrip, 0, 0, vtxData->NumElements);
		primInit->Compile(primPass);

		primPass.AddPrimitive(primInit);

		return;
	}

	CShader* pSFShader = SF_SetTechnique(techName);
	if (!pSFShader)
		return;

	{
		SRenderPipeline& RESTRICT_REFERENCE rRP = m_RP;

		if (params.renderMaskedStates & GS_COLMASK_NONE)
		{
			rRP.m_PersFlags2 |= RBPF2_DISABLECOLORWRITES;
			rRP.m_StateOr |= GS_COLMASK_NONE;
		}

		uint32 numPasses(0);
		pSFShader->FXBegin(&numPasses, /*FEF_DONTSETTEXTURES |*/ FEF_DONTSETSTATES);
		if (!numPasses)
		{
			return;
		}
		pSFShader->FXBeginPass(0);

		FX_SetState(SF_AdjustBlendStateForMeasureOverdraw(params.blendModeStates) | /*GS_NODEPTHTEST | */ params.renderMaskedStates);
		D3DSetCull(eCULL_None);

		{
			m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_VS, params.m_vsBuffer, 0);
			m_DevMan.BindConstantBuffer(CSubmissionQueue_DX11::TYPE_PS, params.m_psBuffer, 0);

			CTexture::SetSampler(sfRes.samplerStateHandles[params.texture[0].texState], 0, eHWSC_Pixel);
			CTexture::SetSampler(sfRes.samplerStateHandles[params.texture[1].texState], 1, eHWSC_Pixel);

			params.texture[0].pTex->ApplyTexture(0, eHWSC_Pixel, EDefaultResourceViews::Default);
			params.texture[1].pTex->ApplyTexture(1, eHWSC_Pixel, EDefaultResourceViews::Default);

			// Commit all render changes
			FX_Commit();
		}

		// Set vertex declaration
		if (!SF_SetVertexDeclaration(vtxData->VertexFormat))
		{
			rRP.m_PersFlags2 &= ~RBPF2_DISABLECOLORWRITES;
			rRP.m_StateOr &= ~GS_COLMASK_NONE;
			pSFShader->FXEndPass();
			pSFShader->FXEnd();
			return;
		}

		// Copy vertex data...
		{
			{
				buffer_size_t bufferOffset = ~0;
				D3DVertexBuffer* pVB = gRenDev->m_DevBufMan.GetD3DVB(vtxData->DeviceDataHandle, &bufferOffset);
				gcpRendD3D->FX_SetVStream(0, pVB, bufferOffset, vtxData->StrideSize);
			}
		}

		//draw
		{
			// Override blend op if necessary
			SF_SetBlendOp(params.blendOp);

			// Submit draw call
			FX_DrawPrimitive(eptTriangleStrip, 0, vtxData->NumElements);

			// Reset overridden blend op if necessary
			SF_SetBlendOp(params.blendOp, true);
		}

		// End shader pass
		pSFShader->FXEndPass();
		pSFShader->FXEnd();
	}
}

//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::SF_Flush()
{
	if (IsDeviceLost())
		return;

	SSF_ResourcesD3D& sfRes(SF_GetResources());
	if (!sfRes.m_fence)
	{
		if (FAILED(GetDeviceObjectFactory().CreateFence(sfRes.m_fence)))
		{
			return;
		}
	}

	GetDeviceObjectFactory().IssueFence(sfRes.m_fence);
	GetDeviceObjectFactory().SyncFence(sfRes.m_fence, true);
}

//////////////////////////////////////////////////////////////////////////
int CRenderer::SF_CreateTexture(int width, int height, int numMips, const unsigned char* pData, ETEX_Format eSrcFormat, int flags)
{
	char name[128];
	cry_sprintf(name, "$SF_%d", m_TexGenID++);

	flags |= !numMips ? FT_FORCE_MIPS : 0;

	CTexture* pTexture(CTexture::GetOrCreate2DTexture(name, width, height, numMips, flags, (byte*)pData, eSrcFormat));

	int texId = (pTexture != 0) ? pTexture->GetID() : -1;
	return texId;
}

//////////////////////////////////////////////////////////////////////////
bool CD3D9Renderer::SF_UpdateTexture(int texId, int mipLevel, int numRects, const SUpdateRect* pRects, const unsigned char* pSrcData, size_t rowPitch, size_t size, ETEX_Format eSrcFormat)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SYSTEM);

	assert(texId > 0 && numRects > 0 && pRects != 0 && pSrcData != 0 && rowPitch > 0);

	CTexture* pTexture(CTexture::GetByID(texId));
	assert(pTexture);

	if (pTexture->GetTextureType() != eTT_2D || pTexture->GetDstFormat() != eSrcFormat)
	{
		assert(0);
		return false;
	}

	CDeviceTexture* pDevTex = pTexture->GetDevTexture();
	if (!pDevTex)
		return false;

	GPUPIN_DEVICE_TEXTURE(GetPerformanceDeviceContext(), pDevTex);
	for (int i = 0; i < numRects; ++i)
	{
		int sizePixel(CTexture::BytesPerPixel(eSrcFormat));
		const unsigned char* pSrc(&pSrcData[pRects[i].srcY * rowPitch + sizePixel * pRects[i].srcX]);

		// TODO: batch upload (instead of loop)
		const size_t planePitch = rowPitch * pRects[i].height;
		const SResourceMemoryMapping mapping =
		{
			{ sizePixel, rowPitch, planePitch, planePitch }, // src alignment
			{ pRects[i].dstX, pRects[i].dstY, 0, 0 },     // dst position
			{ pRects[i].width, pRects[i].height, 1, 1 }   // dst size
		};

		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(pSrc, pDevTex, mapping);
	}

	return true;
}

bool CD3D9Renderer::SF_ClearTexture(int texId, int mipLevel, int numRects, const SUpdateRect* pRects, const unsigned char* pData)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_SYSTEM);
	__debugbreak();

	assert(texId > 0 && numRects > 0 && pRects != 0 && pData != 0);

	CTexture* pTexture(CTexture::GetByID(texId));
	assert(pTexture);

	if (pTexture->GetTextureType() != eTT_2D)
	{
		assert(0);
		return false;
	}

	CDeviceTexture* pDevTex = pTexture->GetDevTexture();
	if (!pDevTex)
		return false;

	// TODO: batch rect clears
	const ColorF clearValue(pData[0], pData[1], pData[2], pData[3]);
	for (int i(0); i < numRects; ++i)
	{
		if (!pRects)
		{
			CDeviceCommandListRef commandList = GetDeviceObjectFactory().GetCoreCommandList();
			commandList.GetGraphicsInterface()->ClearSurface(pTexture->GetSurface(0, mipLevel), clearValue);
		}
		else
		{
			D3D11_RECT box = { pRects[i].dstX, pRects[i].dstY, pRects[i].dstX + pRects[i].width, pRects[i].dstY + pRects[i].height };
			CDeviceCommandListRef commandList = GetDeviceObjectFactory().GetCoreCommandList();
			commandList.GetGraphicsInterface()->ClearSurface(pTexture->GetSurface(0, mipLevel), clearValue, 1, &box);
		}
	}
	return true;
}

#else // RENDERER_SUPPORT_SCALEFORM

IScaleformPlayback* CRenderer::SF_CreatePlayback() const
{
	return new CScaleformSink();
}

#endif // RENDERER_SUPPORT_SCALEFORM
