// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if RENDERER_SUPPORT_SCALEFORM
#include "../Common/CryNameR.h"
#include "ScaleformPlayback.h"

#include <forward_list>

#include "../XRenderD3D9/GraphicsPipeline/Common/PrimitiveRenderPass.h"

class CD3D9Renderer;
class CShader;

//////////////////////////////////////////////////////////////////////////
struct SSF_ResourcesD3D
{
	CCryNameTSCRC m_shTech_SolidColor;
	CCryNameTSCRC m_shTech_GlyphMultiplyTexture;
	CCryNameTSCRC m_shTech_GlyphTexture;
	CCryNameTSCRC m_shTech_GlyphAlphaTexture;
	CCryNameTSCRC m_shTech_GlyphMultiplyTextureYUV;
	CCryNameTSCRC m_shTech_GlyphTextureYUV;
	CCryNameTSCRC m_shTech_GlyphMultiplyTextureYUVA;
	CCryNameTSCRC m_shTech_GlyphTextureYUVA;
	CCryNameTSCRC m_shTech_CxformMultiplyTexture;
	CCryNameTSCRC m_shTech_CxformTexture;
	CCryNameTSCRC m_shTech_CxformGouraudMultiplyNoAddAlpha;
	CCryNameTSCRC m_shTech_CxformGouraudNoAddAlpha;
	CCryNameTSCRC m_shTech_CxformGouraudMultiply;
	CCryNameTSCRC m_shTech_CxformGouraud;
	CCryNameTSCRC m_shTech_CxformGouraudMultiplyTexture;
	CCryNameTSCRC m_shTech_CxformGouraudTexture;
	CCryNameTSCRC m_shTech_CxformMultiply2Texture;
	CCryNameTSCRC m_shTech_Cxform2Texture;
	CCryNameTSCRC m_shTech_GlyphTextureMat;
	CCryNameTSCRC m_shTech_GlyphTextureMatMul;
	CCryNameTSCRC m_shTech_BlurFilter_Box1;
	CCryNameTSCRC m_shTech_BlurFilter_Box2;
	CCryNameTSCRC m_shTech_BlurFilterMul_Box1;
	CCryNameTSCRC m_shTech_BlurFilterMul_Box2;
	CCryNameTSCRC m_shTech_InnerShadow_Box2;
	CCryNameTSCRC m_shTech_InnerShadowHighlight_Box2;
	CCryNameTSCRC m_shTech_InnerShadowMul_Box2;
	CCryNameTSCRC m_shTech_InnerShadowMulHighlight_Box2;
	CCryNameTSCRC m_shTech_InnerShadowKnockout_Box2;
	CCryNameTSCRC m_shTech_InnerShadowHighlightKnockout_Box2;
	CCryNameTSCRC m_shTech_InnerShadowMulKnockout_Box2;
	CCryNameTSCRC m_shTech_InnerShadowMulHighlightKnockout_Box2;
	CCryNameTSCRC m_shTech_Shadow_Box2;
	CCryNameTSCRC m_shTech_ShadowHighlight_Box2;
	CCryNameTSCRC m_shTech_ShadowMul_Box2;
	CCryNameTSCRC m_shTech_ShadowMulHighlight_Box2;
	CCryNameTSCRC m_shTech_ShadowKnockout_Box2;
	CCryNameTSCRC m_shTech_ShadowHighlightKnockout_Box2;
	CCryNameTSCRC m_shTech_ShadowMulKnockout_Box2;
	CCryNameTSCRC m_shTech_ShadowMulHighlightKnockout_Box2;
	CCryNameTSCRC m_shTech_Shadowonly_Box2;
	CCryNameTSCRC m_shTech_ShadowonlyHighlight_Box2;
	CCryNameTSCRC m_shTech_ShadowonlyMul_Box2;
	CCryNameTSCRC m_shTech_ShadowonlyMulHighlight_Box2;
	CCryNameTSCRC* m_FilterTechnique[SSF_GlobalDrawParams::BlurCount];
	CCryNameTSCRC* m_FillTechnique[SSF_GlobalDrawParams::FillCount][2];

	CShader* m_pShader;

	InputLayoutHandle m_vertexDecls[IScaleformPlayback::Vertex_Num];
	D3DVertexDeclaration* m_pVertexDecls[IScaleformPlayback::Vertex_Num];

	SamplerStateHandle samplerStateHandles[8];
	std::vector<CTexture*> m_renderTargets;
	std::vector<CTexture*> m_depthTargets;

	SSF_ResourcesD3D(CD3D9Renderer* pRenderer);
	~SSF_ResourcesD3D();

	CShader*  GetShader(CD3D9Renderer* pRenderer);
	CTexture* GetColorSurface(CD3D9Renderer* pRenderer, int nWidth, int nHeight, ETEX_Format eFormat, int nMaxWidth = 1 << 30, int nMaxHeight = 1 << 30);
	CTexture* GetStencilSurface(CD3D9Renderer* pRenderer, int nWidth, int nHeight, ETEX_Format eFormat);

	void FreeColorSurfaces();
	void FreeStencilSurfaces();

	struct CRenderPrimitiveHeap
	{
		typedef std::forward_list<CRenderPrimitive> CompiledRPList;

		CompiledRPList m_freeList;
		std::unordered_map<int, CompiledRPList> m_useList;
		CryCriticalSectionNonRecursive m_lock;

		CRenderPrimitive* GetUsablePrimitive(int key);
		void FreeUsedPrimitives(int key);
		void Clear();
	}
	m_PrimitiveHeap;

	struct STransientConstantBufferHeap
	{
		typedef std::forward_list<CConstantBufferPtr> TransientCBList;

		TransientCBList m_freeList;
		TransientCBList m_useList;
		CryCriticalSectionNonRecursive m_lock;

		CConstantBuffer* GetUsableConstantBuffer();
		void FreeUsedConstantBuffers();

		~STransientConstantBufferHeap();
	}
	m_CBHeap;
};

#endif
