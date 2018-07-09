// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>

#include "xxhash.h"
#include "DevicePSO.h"

static uint8 g_StencilFuncLookup[8] =
{
	D3D11_COMPARISON_ALWAYS,        // FSS_STENCFUNC_ALWAYS   0x0
	D3D11_COMPARISON_NEVER,         // FSS_STENCFUNC_NEVER    0x1
	D3D11_COMPARISON_LESS,          // FSS_STENCFUNC_LESS     0x2
	D3D11_COMPARISON_LESS_EQUAL,    // FSS_STENCFUNC_LEQUAL   0x3
	D3D11_COMPARISON_GREATER,       // FSS_STENCFUNC_GREATER  0x4
	D3D11_COMPARISON_GREATER_EQUAL, // FSS_STENCFUNC_GEQUAL   0x5
	D3D11_COMPARISON_EQUAL,         // FSS_STENCFUNC_EQUAL    0x6
	D3D11_COMPARISON_NOT_EQUAL      // FSS_STENCFUNC_NOTEQUAL 0x7
};

static uint8 g_StencilOpLookup[8] =
{
	D3D11_STENCIL_OP_KEEP,          // FSS_STENCOP_KEEP    0x0
	D3D11_STENCIL_OP_REPLACE,       // FSS_STENCOP_REPLACE 0x1
	D3D11_STENCIL_OP_INCR_SAT,      // FSS_STENCOP_INCR    0x2
	D3D11_STENCIL_OP_DECR_SAT,      // FSS_STENCOP_DECR    0x3
	D3D11_STENCIL_OP_ZERO,          // FSS_STENCOP_ZERO    0x4
	D3D11_STENCIL_OP_INCR,          // FSS_STENCOP_INCR_WRAP 0x5
	D3D11_STENCIL_OP_DECR,          // FSS_STENCOP_DECR_WRAP 0x6
	D3D11_STENCIL_OP_INVERT         // FSS_STENCOP_INVERT 0x7
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDeviceGraphicsPSODesc::CDeviceGraphicsPSODesc(CDeviceResourceLayoutPtr pResourceLayout, const SGraphicsPipelineStateDescription& pipelineDesc)
{
	InitWithDefaults();

	m_pResourceLayout = pResourceLayout;
	m_pShader = reinterpret_cast<::CShader*>(pipelineDesc.shaderItem.m_pShader);
	if (auto pTech = m_pShader->GetTechnique(pipelineDesc.shaderItem.m_nTechnique, pipelineDesc.technique, true))
	{
		m_technique = pTech->m_NameCRC;
		m_RenderState = pTech->m_Passes[0].m_RenderState;
	}

	m_ShaderFlags_RT = pipelineDesc.objectRuntimeMask;
	m_ShaderFlags_MDV = pipelineDesc.objectFlags_MDV;
	m_VertexFormat = pipelineDesc.vertexFormat;
	m_ObjectStreamMask = pipelineDesc.streamMask;
	m_PrimitiveType = (ERenderPrimitiveType)pipelineDesc.primitiveType;
}

CDeviceGraphicsPSODesc::CDeviceGraphicsPSODesc()
{
	InitWithDefaults();
}

void CDeviceGraphicsPSODesc::InitWithDefaults()
{
	ZeroStruct(*this);

	m_StencilState = STENC_FUNC(FSS_STENCFUNC_ALWAYS) | STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(FSS_STENCOP_KEEP);
	m_StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	m_StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	m_VertexFormat = EDefaultInputLayouts::P3F_C4B_T2S;
	m_CullMode = eCULL_Back;
	m_PrimitiveType = eptTriangleList;
	m_bDepthClip = true;
	m_bDepthBoundsTest = false;
	m_bDynamicDepthBias = false;
}

CDeviceGraphicsPSODesc& CDeviceGraphicsPSODesc::operator=(const CDeviceGraphicsPSODesc& other)
{
	// increment ref counts
	m_pRenderPass = other.m_pRenderPass;
	m_pResourceLayout = other.m_pResourceLayout;
	m_pShader = other.m_pShader;

	// NOTE: need to make sure both structs are binary equivalent for hashing and comparison
	memcpy(this, &other, sizeof(CDeviceGraphicsPSODesc));

	return *this;
}

CDeviceGraphicsPSODesc::CDeviceGraphicsPSODesc(const CDeviceGraphicsPSODesc& other)
{
	*this = other;
}

bool CDeviceGraphicsPSODesc::operator==(const CDeviceGraphicsPSODesc& other) const
{
	return memcmp(this, &other, sizeof(CDeviceGraphicsPSODesc)) == 0;
}

uint64 CDeviceGraphicsPSODesc::GetHash() const
{
	uint64 key = XXH64(this, sizeof(CDeviceGraphicsPSODesc), 0);
	return key;
}

void CDeviceGraphicsPSODesc::FillDescs(D3D11_RASTERIZER_DESC& rasterizerDesc, D3D11_BLEND_DESC& blendDesc, D3D11_DEPTH_STENCIL_DESC& depthStencilDesc) const
{
	// 00001110 11111111 11111111 11111111 USED
	// 00001110 11100000 00000000 11111111 BLENDMASK
	// ======== ======== ======== ======== ==================================
	// 00000000 00000000 00000000 00001111 BLSRC
	// 00000000 00000000 00000000 11110000 BLDST
	// 00000000 00000000 00000001 00000000 DEPTHWRITE
	// 00000000 00000000 00000010 00000000 DEPTHTEST
	// 00000000 00000000 00000100 00000000 STENCIL
	// 00000000 00000000 00001000 00000000 ALPHATEST
	// 00000000 00000000 11110000 00000000 NOCOLMASK
	// 00000000 00000011 00000000 00000000 POINT|WIREFRAME
	// 00000000 00011100 00000000 00000000 DEPTHFUNC
	// 00000000 11100000 00000000 00000000 BLENDOP
	// 00001110 00000000 00000000 00000000 BLENDOPALPHA
	const uint32 renderState = m_RenderState;

	ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));
	ZeroMemory(&blendDesc, sizeof(D3D11_BLEND_DESC));
	ZeroMemory(&depthStencilDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));

	// Fill mode
	rasterizerDesc.DepthClipEnable = m_bDepthClip ? 1 : 0;
	rasterizerDesc.FrontCounterClockwise = 1;
	rasterizerDesc.FillMode = (renderState & GS_WIREFRAME) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
	rasterizerDesc.CullMode =
		(m_CullMode == eCULL_Back)
		? D3D11_CULL_BACK
		: ((m_CullMode == eCULL_None) ? D3D11_CULL_NONE : D3D11_CULL_FRONT);

	// scissor is always enabled on DX12.
	rasterizerDesc.ScissorEnable = TRUE;

	// *INDENT-OFF*
	// Blend state
	{
		const bool bBlendEnable = (renderState & GS_BLEND_MASK) != 0;
		blendDesc.RenderTarget[0].BlendEnable = bBlendEnable;

		struct BlendFactors
		{
			D3D11_BLEND BlendColor;
			D3D11_BLEND BlendAlpha;
			bool		bAllowOnAllTargets;
		};

		static BlendFactors SrcBlendFactors[GS_BLSRC_MASK >> GS_BLSRC_SHIFT] =
		{
			{ (D3D11_BLEND)0,             (D3D11_BLEND)0, 					true },        // UNINITIALIZED BLEND FACTOR
			{ D3D11_BLEND_ZERO,           D3D11_BLEND_ZERO,					true },        // GS_BLSRC_ZERO
			{ D3D11_BLEND_ONE,            D3D11_BLEND_ONE,					true },        // GS_BLSRC_ONE
			{ D3D11_BLEND_DEST_COLOR,     D3D11_BLEND_DEST_ALPHA,			true },		   // GS_BLSRC_DSTCOL
			{ D3D11_BLEND_INV_DEST_COLOR, D3D11_BLEND_INV_DEST_ALPHA,		true },        // GS_BLSRC_ONEMINUSDSTCOL
			{ D3D11_BLEND_SRC_ALPHA,      D3D11_BLEND_SRC_ALPHA,			true },        // GS_BLSRC_SRCALPHA
			{ D3D11_BLEND_INV_SRC_ALPHA,  D3D11_BLEND_INV_SRC_ALPHA,		true },        // GS_BLSRC_ONEMINUSSRCALPHA
			{ D3D11_BLEND_DEST_ALPHA,     D3D11_BLEND_DEST_ALPHA,			true },        // GS_BLSRC_DSTALPHA
			{ D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA,		true },        // GS_BLSRC_ONEMINUSDSTALPHA
			{ D3D11_BLEND_SRC_ALPHA_SAT,  D3D11_BLEND_SRC_ALPHA_SAT,		true },        // GS_BLSRC_ALPHASATURATE
			{ D3D11_BLEND_SRC_ALPHA,      D3D11_BLEND_ZERO,					true },        // GS_BLSRC_SRCALPHA_A_ZERO
			{ D3D11_BLEND_SRC1_ALPHA,     D3D11_BLEND_SRC1_ALPHA,			false },       // GS_BLSRC_SRC1ALPHA
		};

		static BlendFactors DstBlendFactors[GS_BLDST_MASK >> GS_BLDST_SHIFT] =
		{
			{ (D3D11_BLEND)0,             (D3D11_BLEND)0,					true },        // UNINITIALIZED BLEND FACTOR
			{ D3D11_BLEND_ZERO,           D3D11_BLEND_ZERO,					true },        // GS_BLDST_ZERO
			{ D3D11_BLEND_ONE,            D3D11_BLEND_ONE,					true },        // GS_BLDST_ONE
			{ D3D11_BLEND_SRC_COLOR,      D3D11_BLEND_SRC_ALPHA,			true },        // GS_BLDST_SRCCOL
			{ D3D11_BLEND_INV_SRC_COLOR,  D3D11_BLEND_INV_SRC_ALPHA,		true },        // GS_BLDST_ONEMINUSSRCCOL
			{ D3D11_BLEND_SRC_ALPHA,      D3D11_BLEND_SRC_ALPHA,			true },        // GS_BLDST_SRCALPHA
			{ D3D11_BLEND_INV_SRC_ALPHA,  D3D11_BLEND_INV_SRC_ALPHA,		true },        // GS_BLDST_ONEMINUSSRCALPHA
			{ D3D11_BLEND_DEST_ALPHA,     D3D11_BLEND_DEST_ALPHA,			true },        // GS_BLDST_DSTALPHA
			{ D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA,		true },        // GS_BLDST_ONEMINUSDSTALPHA
			{ D3D11_BLEND_ONE,            D3D11_BLEND_ZERO,					true },        // GS_BLDST_ONE_A_ZERO
			{ D3D11_BLEND_INV_SRC1_ALPHA, D3D11_BLEND_INV_SRC1_ALPHA,		false },       // GS_BLDST_ONEMINUSSRC1ALPHA
		};

		static D3D11_BLEND_OP BlendOp[GS_BLEND_OP_MASK >> GS_BLEND_OP_SHIFT] =
		{
			D3D11_BLEND_OP_ADD,          // 0 (unspecified): Default
			D3D11_BLEND_OP_MAX,          // GS_BLOP_MAX / GS_BLALPHA_MAX
			D3D11_BLEND_OP_MIN,          // GS_BLOP_MIN / GS_BLALPHA_MIN
			D3D11_BLEND_OP_SUBTRACT,     // GS_BLOP_SUB / GS_BLALPHA_SUB
			D3D11_BLEND_OP_REV_SUBTRACT, // GS_BLOP_SUBREV / GS_BLALPHA_SUBREV
		};

		const bool bSrcBlendAllowAllTargets = SrcBlendFactors[(renderState & GS_BLSRC_MASK) >> GS_BLSRC_SHIFT].bAllowOnAllTargets;
		const bool bDstBlendAllowAllTargets = DstBlendFactors[(renderState & GS_BLDST_MASK) >> GS_BLDST_SHIFT].bAllowOnAllTargets;
		const bool bBlendAllowAllTargets = bSrcBlendAllowAllTargets && bDstBlendAllowAllTargets;

		if (bBlendEnable)
		{
			blendDesc.RenderTarget[0].SrcBlend = SrcBlendFactors[(renderState & GS_BLSRC_MASK) >> GS_BLSRC_SHIFT].BlendColor;
			blendDesc.RenderTarget[0].SrcBlendAlpha = SrcBlendFactors[(renderState & GS_BLSRC_MASK) >> GS_BLSRC_SHIFT].BlendAlpha;
			blendDesc.RenderTarget[0].DestBlend = DstBlendFactors[(renderState & GS_BLDST_MASK) >> GS_BLDST_SHIFT].BlendColor;
			blendDesc.RenderTarget[0].DestBlendAlpha = DstBlendFactors[(renderState & GS_BLDST_MASK) >> GS_BLDST_SHIFT].BlendAlpha;

			blendDesc.RenderTarget[0].BlendOp = BlendOp[(renderState & GS_BLEND_OP_MASK) >> GS_BLEND_OP_SHIFT];
			blendDesc.RenderTarget[0].BlendOpAlpha = BlendOp[(renderState & GS_BLALPHA_MASK) >> GS_BLALPHA_SHIFT];

			if ((renderState & GS_BLALPHA_MASK) == GS_BLALPHA_MIN)
			{
				blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
				blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			}
		}

		// Copy blend state to support independent blend
		for (size_t i = 0; i < CD3D9Renderer::RT_STACK_WIDTH; ++i)
		{
			blendDesc.RenderTarget[i] = blendDesc.RenderTarget[0];

			// Dual source color blend cannot be enabled for any RT slot but 0
			if (!bBlendAllowAllTargets && i > 0)
			{
				blendDesc.RenderTarget[i].BlendEnable = false;
			}
		}
		blendDesc.IndependentBlendEnable = true;
	}

	{
		for (uint32 i = 0; i < 4; ++i)
		{
			uint32 mask = 0xfffffff0 | (ColorMasks[(renderState & GS_COLMASK_MASK) >> GS_COLMASK_SHIFT][i]);
			mask = (~mask) & 0xf;
			blendDesc.RenderTarget[i].RenderTargetWriteMask = mask;
		}
	}

	// Depth-Stencil
	{
		depthStencilDesc.DepthWriteMask = (renderState & GS_DEPTHWRITE) ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthEnable = ((renderState & GS_NODEPTHTEST) && !(renderState & GS_DEPTHWRITE)) ? 0 : 1;

		static D3D11_COMPARISON_FUNC DepthFunc[GS_DEPTHFUNC_MASK >> GS_DEPTHFUNC_SHIFT] =
		{
			D3D11_COMPARISON_LESS_EQUAL,     // GS_DEPTHFUNC_LEQUAL
			D3D11_COMPARISON_EQUAL,          // GS_DEPTHFUNC_EQUAL
			D3D11_COMPARISON_GREATER,        // GS_DEPTHFUNC_GREAT
			D3D11_COMPARISON_LESS,           // GS_DEPTHFUNC_LESS
			D3D11_COMPARISON_GREATER_EQUAL,  // GS_DEPTHFUNC_GEQUAL
			D3D11_COMPARISON_NOT_EQUAL,      // GS_DEPTHFUNC_NOTEQUAL
		};

		depthStencilDesc.DepthFunc =
			(renderState & (GS_NODEPTHTEST | GS_DEPTHWRITE)) == (GS_NODEPTHTEST | GS_DEPTHWRITE)
			? D3D11_COMPARISON_ALWAYS
			: DepthFunc[(m_RenderState & GS_DEPTHFUNC_MASK) >> GS_DEPTHFUNC_SHIFT];

		depthStencilDesc.StencilEnable = (renderState & GS_STENCIL) ? 1 : 0;
		depthStencilDesc.StencilReadMask = m_StencilReadMask;
		depthStencilDesc.StencilWriteMask = m_StencilWriteMask;

		depthStencilDesc.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)g_StencilFuncLookup[m_StencilState & FSS_STENCFUNC_MASK];
		depthStencilDesc.FrontFace.StencilFailOp = (D3D11_STENCIL_OP)g_StencilOpLookup[(m_StencilState & FSS_STENCFAIL_MASK) >> FSS_STENCFAIL_SHIFT];
		depthStencilDesc.FrontFace.StencilDepthFailOp = (D3D11_STENCIL_OP)g_StencilOpLookup[(m_StencilState & FSS_STENCZFAIL_MASK) >> FSS_STENCZFAIL_SHIFT];
		depthStencilDesc.FrontFace.StencilPassOp = (D3D11_STENCIL_OP)g_StencilOpLookup[(m_StencilState & FSS_STENCPASS_MASK) >> FSS_STENCPASS_SHIFT];
		depthStencilDesc.BackFace = depthStencilDesc.FrontFace;

		if (m_StencilState & FSS_STENCIL_TWOSIDED)
		{
			depthStencilDesc.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)g_StencilFuncLookup[(m_StencilState & (FSS_STENCFUNC_MASK << FSS_CCW_SHIFT)) >> FSS_CCW_SHIFT];
			depthStencilDesc.BackFace.StencilFailOp = (D3D11_STENCIL_OP)g_StencilOpLookup[(m_StencilState & (FSS_STENCFAIL_MASK << FSS_CCW_SHIFT)) >> (FSS_STENCFAIL_SHIFT + FSS_CCW_SHIFT)];
			depthStencilDesc.BackFace.StencilDepthFailOp = (D3D11_STENCIL_OP)g_StencilOpLookup[(m_StencilState & (FSS_STENCZFAIL_MASK << FSS_CCW_SHIFT)) >> (FSS_STENCZFAIL_SHIFT + FSS_CCW_SHIFT)];
			depthStencilDesc.BackFace.StencilPassOp = (D3D11_STENCIL_OP)g_StencilOpLookup[(m_StencilState & (FSS_STENCPASS_MASK << FSS_CCW_SHIFT)) >> (FSS_STENCPASS_SHIFT + FSS_CCW_SHIFT)];
		}
	}
	// *INDENT-ON*
}

uint32 CDeviceGraphicsPSODesc::CombineVertexStreamMasks(uint32 fromShader, uint32 fromObject) const
{
	uint32 result = fromShader;

	if (fromObject & VSM_NORMALS)
		result |= VSM_NORMALS;

	if (fromObject & BIT(VSF_QTANGENTS))
	{
		result &= ~VSM_TANGENTS;
		result |= BIT(VSF_QTANGENTS);
	}

	if (fromObject & VSM_INSTANCED)
		result |= VSM_INSTANCED;

	return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CDeviceComputePSODesc::CDeviceComputePSODesc(CDeviceResourceLayoutPtr pResourceLayout, ::CShader* pShader, const CCryNameTSCRC& technique, uint64 rtFlags, uint32 mdFlags, uint32 mdvFlags)
{
	InitWithDefaults();

	m_pResourceLayout = pResourceLayout;
	m_pShader = pShader;
	m_technique = technique;
	m_ShaderFlags_RT = rtFlags;
	m_ShaderFlags_MD = mdFlags;
	m_ShaderFlags_MDV = mdvFlags;
}

void CDeviceComputePSODesc::InitWithDefaults()
{
	ZeroStruct(*this);
}

CDeviceComputePSODesc& CDeviceComputePSODesc::operator=(const CDeviceComputePSODesc& other)
{
	// increment ref counts
	m_pResourceLayout = other.m_pResourceLayout;
	m_pShader = other.m_pShader;

	// NOTE: need to make sure both structs are binary equivalent for hashing and comparison
	memcpy(this, &other, sizeof(CDeviceComputePSODesc));

	return *this;
}

CDeviceComputePSODesc::CDeviceComputePSODesc(const CDeviceComputePSODesc& other)
{
	*this = other;
}

bool CDeviceComputePSODesc::operator==(const CDeviceComputePSODesc& other) const
{
	return memcmp(this, &other, sizeof(CDeviceComputePSODesc)) == 0;
}

uint64 CDeviceComputePSODesc::GetHash() const
{
	uint64 key = XXH64(this, sizeof(CDeviceComputePSODesc), 0);
	return key;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CDeviceGraphicsPSO::ValidateShadersAndTopologyCombination(const CDeviceGraphicsPSODesc& psoDesc, const std::array<void*, eHWSC_Num>& hwShaderInstances)
{
	const bool bTessellationSupport = hwShaderInstances[eHWSC_Domain] && hwShaderInstances[eHWSC_Hull];

	const bool bControlPointPatchPrimitive =
		(ept1ControlPointPatchList == psoDesc.m_PrimitiveType) ||
		(ept2ControlPointPatchList == psoDesc.m_PrimitiveType) ||
		(ept3ControlPointPatchList == psoDesc.m_PrimitiveType) ||
		(ept4ControlPointPatchList == psoDesc.m_PrimitiveType);

	const bool bShadersAndTopologyCombination =
		(!bTessellationSupport && !bControlPointPatchPrimitive) ||
		(bTessellationSupport && bControlPointPatchPrimitive);

	return bShadersAndTopologyCombination;
}

