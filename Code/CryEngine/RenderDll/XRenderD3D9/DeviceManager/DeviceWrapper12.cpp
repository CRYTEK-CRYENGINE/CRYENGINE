// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DeviceWrapper12.h"
#include "DriverD3D.h"
#include "xxhash.h"
#include "../../Common/ReverseDepth.h"
#include "../../Common/Textures/TextureHelpers.h"
#include "../GraphicsPipeline/Common/GraphicsPipelineStateSet.h"

extern uint8 g_StencilFuncLookup[8];
extern uint8 g_StencilOpLookup[8];

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::array<SDeviceObjectHelpers::SShaderInstanceInfo, eHWSC_Num> SDeviceObjectHelpers::GetShaderInstanceInfo(CShader* pShader, const CCryNameTSCRC& technique, uint64 rtFlags, uint32 mdFlags, uint32 mdvFlags, UPipelineState pipelineState[eHWSC_Num], bool bAllowTesselation)
{
	std::array<SDeviceObjectHelpers::SShaderInstanceInfo, eHWSC_Num> result;
	if (SShaderTechnique* pShaderTechnique = pShader->mfFindTechnique(technique))
	{
		SShaderPass& shaderPass = pShaderTechnique->m_Passes[0];

		CHWShader* pHWShaders[] =
		{
			shaderPass.m_VShader,
			shaderPass.m_PShader,
			shaderPass.m_GShader,
			shaderPass.m_CShader,
			shaderPass.m_DShader,
			shaderPass.m_HShader,
		};

		for (EHWShaderClass shaderStage = eHWSC_Vertex; shaderStage < eHWSC_Num; shaderStage = EHWShaderClass(shaderStage + 1))
		{
			if (!bAllowTesselation && (shaderStage == eHWSC_Hull || shaderStage == eHWSC_Domain))
			{
				continue;
			}

			CHWShader_D3D* pHWShaderD3D = reinterpret_cast<CHWShader_D3D*>(pHWShaders[shaderStage]);
			result[shaderStage].pHwShader = pHWShaderD3D;
			result[shaderStage].technique = technique;

			if (pHWShaderD3D)
			{
				SShaderCombIdent Ident;
				Ident.m_LightMask = 0;
				Ident.m_RTMask = rtFlags & pHWShaderD3D->m_nMaskAnd_RT | pHWShaderD3D->m_nMaskOr_RT;
				Ident.m_MDMask = mdFlags & (shaderStage != eHWSC_Pixel ? 0xFFFFFFFF : ~HWMD_TEXCOORD_FLAG_MASK);
				Ident.m_MDVMask = ((shaderStage != eHWSC_Pixel) ? mdvFlags : 0) | CParserBin::m_nPlatform;
				Ident.m_GLMask = pHWShaderD3D->m_nMaskGenShader;
				Ident.m_pipelineState = pipelineState ? pipelineState[shaderStage] : UPipelineState();

				if (auto pInstance = pHWShaderD3D->mfGetInstance(pShader, Ident, 0))
				{
					if (pHWShaderD3D->mfCheckActivation(pShader, pInstance, 0))
					{
						result[shaderStage].pHwShaderInstance = pInstance;
						result[shaderStage].pDeviceShader = pInstance->m_Handle.m_pShader->m_pHandle;
					}
				}
			}
		}
	}

	return result;
}

bool SDeviceObjectHelpers::CheckTessellationSupport(SShaderItem& shaderItem)
{
	// TTYPE_ZPREPASS doesn't support tessellation
	EShaderTechniqueID techniquesInUse[] = { TTYPE_Z, TTYPE_SHADOWGEN };
	CShader* pShader = reinterpret_cast<CShader*>(shaderItem.m_pShader);

	bool bResult = false;
	for (int i = 0; i < CRY_ARRAY_COUNT(techniquesInUse); ++i)
	{
		if (auto pShaderTechnique = pShader->GetTechnique(shaderItem.m_nTechnique, techniquesInUse[i], true))
		{
			SShaderPass& shaderPass = pShaderTechnique->m_Passes[0];
			bResult |= (shaderPass.m_DShader && shaderPass.m_HShader);
		}
	}

	return bResult;
}

bool SDeviceObjectHelpers::GetConstantBuffersFromShader(std::vector<SConstantBufferBindInfo>& outResult, CShader* pShader, const CCryNameTSCRC& technique, uint64 rtFlags, uint32 mdFlags, uint32 mdvFlags)
{
	auto hwShaders = GetShaderInstanceInfo(pShader, technique, rtFlags, mdFlags, mdvFlags, nullptr, true);
	std::vector<SDeviceObjectHelpers::SConstantBufferBindInfo> result;

	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass < eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
	{
		if (hwShaders[shaderClass].pHwShader)
		{
			if (!hwShaders[shaderClass].pHwShaderInstance) // shader instance is not compiled yet
				return false;

			VectorSet<EConstantBufferShaderSlot> usedBuffersSlots;
			auto* pInstance = reinterpret_cast<CHWShader_D3D::SHWSInstance*>(hwShaders[shaderClass].pHwShaderInstance);
			auto* pHwShader = reinterpret_cast<CHWShader_D3D*>(hwShaders[shaderClass].pHwShader);

			const int vectorCount[] =
			{
				pInstance->m_nMaxVecs[eConstantBufferShaderSlot_PerBatch],
				pInstance->m_nMaxVecs[eConstantBufferShaderSlot_PerInstanceLegacy],
				CHWShader_D3D::s_nMax_PF_Vecs[shaderClass]
			};

			for (const auto& bind : pInstance->m_pBindVars)
			{
				if ((bind.m_dwBind & (SHADER_BIND_TEXTURE | SHADER_BIND_SAMPLER)) == 0)
					usedBuffersSlots.insert(EConstantBufferShaderSlot(bind.m_dwCBufSlot));
			}

			if (!pHwShader->s_PF_Params[shaderClass].empty())
				usedBuffersSlots.insert(eConstantBufferShaderSlot_PerFrame);

			for (auto bufferSlot : usedBuffersSlots)
			{
				CRY_ASSERT(bufferSlot >= eConstantBufferShaderSlot_PerBatch && bufferSlot <= eConstantBufferShaderSlot_PerFrame);

				SDeviceObjectHelpers::SConstantBufferBindInfo bindInfo;
				bindInfo.shaderClass = shaderClass;
				bindInfo.shaderSlot = bufferSlot;
				bindInfo.vectorCount = vectorCount[bufferSlot];
				bindInfo.pBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(bindInfo.vectorCount * sizeof(Vec4));
				bindInfo.pBuffer->Release();
				bindInfo.pPreviousBuffer = nullptr;

				bindInfo.shaderInfo.pHwShader = hwShaders[shaderClass].pHwShader;
				bindInfo.shaderInfo.pHwShaderInstance = hwShaders[shaderClass].pHwShaderInstance;
				bindInfo.shaderInfo.pDeviceShader = hwShaders[shaderClass].pDeviceShader;

				result.push_back(bindInfo);
			}
		}
	}

	std::swap(result, outResult); // To prevent partial results from being visible, even though we return false in that case.
	return true;
}

void SDeviceObjectHelpers::BeginUpdateConstantBuffers(std::vector<SDeviceObjectHelpers::SConstantBufferBindInfo>& constantBuffers)
{
	// replace engine constant buffers with our own
	for (auto& cb : constantBuffers)
	{
		if (CHWShader_D3D::s_pDataCB[cb.shaderClass][cb.shaderSlot])
			CHWShader_D3D::mfCommitCB(cb.shaderSlot, (EHWShaderClass)cb.shaderClass, (EHWShaderClass)cb.shaderClass);

		cb.pPreviousBuffer = CHWShader_D3D::s_pCB[cb.shaderClass][cb.shaderSlot][cb.vectorCount];
		CHWShader_D3D::s_pCB[cb.shaderClass][cb.shaderSlot][cb.vectorCount] = cb.pBuffer;
		CHWShader_D3D::s_nCurMaxVecs[cb.shaderClass][cb.shaderSlot] = cb.vectorCount;
	}
}

void SDeviceObjectHelpers::EndUpdateConstantBuffers(std::vector<SDeviceObjectHelpers::SConstantBufferBindInfo>& constantBuffers)
{
	// set per batch and per instance parameters
	auto setParams = [&](EHWShaderClass shaderClass, bool bPerBatch, bool bPerInstance)
	{
		for (auto& cb : constantBuffers)
		{
			if (cb.shaderClass == shaderClass)
			{
				auto pHwShader = cb.shaderInfo.pHwShader;
				auto pHwShaderInstance = reinterpret_cast<CHWShader_D3D::SHWSInstance*>(cb.shaderInfo.pHwShaderInstance);

				if (pHwShader && pHwShaderInstance)
				{
					pHwShader->m_pCurInst = pHwShaderInstance;

					if (bPerBatch)    pHwShader->mfSetParametersPB();
					if (bPerInstance) pHwShader->mfSetParametersPI(NULL, NULL);
				}

				break;
			}
		}
	};

	setParams(eHWSC_Pixel, true, true);
	setParams(eHWSC_Vertex, true, true);
	setParams(eHWSC_Geometry, true, false);
	setParams(eHWSC_Compute, true, false);

	// finalize constant buffers
	for (auto& cb : constantBuffers)
	{
		if (cb.pBuffer->m_buffer)
			cb.pBuffer->EndWrite();

		CHWShader_D3D::s_pDataCB[cb.shaderClass][cb.shaderSlot] = NULL;
		CHWShader_D3D::s_pCurDevCB[cb.shaderClass][cb.shaderSlot] = cb.pBuffer->GetCode();

		// restore previous buffer
		CHWShader_D3D::s_pCB[cb.shaderClass][cb.shaderSlot][cb.vectorCount] = cb.pPreviousBuffer;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDeviceGraphicsPSODesc::CDeviceGraphicsPSODesc(CDeviceResourceLayout* pResourceLayout, const SGraphicsPipelineStateDescription& pipelineDesc)
{
	InitWithDefaults();

	m_pResourceLayout = pResourceLayout;
	m_pShader = reinterpret_cast<CShader*>(pipelineDesc.shaderItem.m_pShader);
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

CDeviceGraphicsPSODesc::CDeviceGraphicsPSODesc(uint32 renderTargetCount, CTexture* const* pRenderTargets, const SDepthTexture* pDepthTarget, SResourceView::KeyType* pRenderTargetViews)
{
	InitWithDefaults();

	for (int i = 0; i < renderTargetCount; ++i)
	{
		ETEX_Format rtFormat = pRenderTargets[i] ? pRenderTargets[i]->GetDstFormat() : eTF_Unknown;
		if (pRenderTargetViews && pRenderTargetViews[i] != SResourceView::DefaultRendertargtView)
		{
			rtFormat = (ETEX_Format)SResourceView(pRenderTargetViews[i]).m_Desc.nFormat;
		}

		m_RenderTargetFormats[i] = rtFormat;
	}

	m_DepthStencilFormat = (pDepthTarget && pDepthTarget->pTexture) ? pDepthTarget->pTexture->GetDstFormat() : eTF_Unknown;
}

void CDeviceGraphicsPSODesc::InitWithDefaults()
{
	ZeroStruct(*this);

	m_StencilState = STENC_FUNC(FSS_STENCFUNC_ALWAYS) | STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(FSS_STENCOP_KEEP);
	m_StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	m_StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	m_VertexFormat = eVF_P3F_C4B_T2S;
	m_CullMode = eCULL_Back;
	m_PrimitiveType = eptTriangleList;
	m_DepthStencilFormat = eTF_Unknown;
	m_RenderTargetFormats.fill(eTF_Unknown);
}

CDeviceGraphicsPSODesc& CDeviceGraphicsPSODesc::operator=(const CDeviceGraphicsPSODesc& other)
{
	// NOTE: need to make sure both structs are binary equivalent for hashing and comparison
	memcpy(this, &other, sizeof(CDeviceGraphicsPSODesc));
	if (m_pShader) m_pShader->AddRef();

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
	const uint32 renderState = m_RenderState;

	ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));
	ZeroMemory(&blendDesc, sizeof(D3D11_BLEND_DESC));
	ZeroMemory(&depthStencilDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));

	// Fill mode
	rasterizerDesc.DepthClipEnable = 1;
	rasterizerDesc.FrontCounterClockwise = 1;
	rasterizerDesc.FillMode = (renderState & GS_WIREFRAME) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
	rasterizerDesc.CullMode =
	  (m_CullMode == eCULL_Back)
	  ? D3D11_CULL_BACK
	  : ((m_CullMode == eCULL_None) ? D3D11_CULL_NONE : D3D11_CULL_FRONT);

	// Blend state
	{
		const bool bBlendEnable = (renderState & GS_BLEND_MASK) != 0;

		for (size_t i = 0; i < CD3D9Renderer::RT_STACK_WIDTH; ++i)
			blendDesc.RenderTarget[i].BlendEnable = bBlendEnable;

		if (bBlendEnable)
		{
			const uint32 SrcFactorShift = 0;
			const uint32 DstFactorShift = 4;
			const uint32 BlendOpShift = 24;
			const uint32 BlendAlphaOpShift = 26;

			struct BlendFactors
			{
				D3D11_BLEND BlendColor;
				D3D11_BLEND BlendAlpha;
			};

			BlendFactors SrcBlendFactors[GS_BLSRC_MASK >> SrcFactorShift] =
			{
				{ (D3D11_BLEND)0,             (D3D11_BLEND)0             },        // UNINITIALIZED BLEND FACTOR
				{ D3D11_BLEND_ZERO,           D3D11_BLEND_ZERO           },        // GS_BLSRC_ZERO
				{ D3D11_BLEND_ONE,            D3D11_BLEND_ONE            },        // GS_BLSRC_ONE
				{ D3D11_BLEND_DEST_COLOR,     D3D11_BLEND_DEST_ALPHA     },        // GS_BLSRC_DSTCOL
				{ D3D11_BLEND_INV_DEST_COLOR, D3D11_BLEND_INV_DEST_ALPHA },        // GS_BLSRC_ONEMINUSDSTCOL
				{ D3D11_BLEND_SRC_ALPHA,      D3D11_BLEND_SRC_ALPHA      },        // GS_BLSRC_SRCALPHA
				{ D3D11_BLEND_INV_SRC_ALPHA,  D3D11_BLEND_INV_SRC_ALPHA  },        // GS_BLSRC_ONEMINUSSRCALPHA
				{ D3D11_BLEND_DEST_ALPHA,     D3D11_BLEND_DEST_ALPHA     },        // GS_BLSRC_DSTALPHA
				{ D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA },        // GS_BLSRC_ONEMINUSDSTALPHA
				{ D3D11_BLEND_SRC_ALPHA_SAT,  D3D11_BLEND_SRC_ALPHA_SAT  },        // GS_BLSRC_ALPHASATURATE
				{ D3D11_BLEND_SRC_ALPHA,      D3D11_BLEND_ZERO           },        // GS_BLSRC_SRCALPHA_A_ZERO
				{ D3D11_BLEND_SRC1_ALPHA,     D3D11_BLEND_SRC1_ALPHA     },        // GS_BLSRC_SRC1ALPHA
			};

			BlendFactors DstBlendFactors[GS_BLDST_MASK >> DstFactorShift] =
			{
				{ (D3D11_BLEND)0,             (D3D11_BLEND)0             },        // UNINITIALIZED BLEND FACTOR
				{ D3D11_BLEND_ZERO,           D3D11_BLEND_ZERO           },        // GS_BLDST_ZERO
				{ D3D11_BLEND_ONE,            D3D11_BLEND_ONE            },        // GS_BLDST_ONE
				{ D3D11_BLEND_SRC_COLOR,      D3D11_BLEND_SRC_ALPHA      },        // GS_BLDST_SRCCOL
				{ D3D11_BLEND_INV_SRC_COLOR,  D3D11_BLEND_INV_SRC_ALPHA  },        // GS_BLDST_ONEMINUSSRCCOL
				{ D3D11_BLEND_SRC_ALPHA,      D3D11_BLEND_SRC_ALPHA      },        // GS_BLDST_SRCALPHA
				{ D3D11_BLEND_INV_SRC_ALPHA,  D3D11_BLEND_INV_SRC_ALPHA  },        // GS_BLDST_ONEMINUSSRCALPHA
				{ D3D11_BLEND_DEST_ALPHA,     D3D11_BLEND_DEST_ALPHA     },        // GS_BLDST_DSTALPHA
				{ D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA },        // GS_BLDST_ONEMINUSDSTALPHA
				{ D3D11_BLEND_ONE,            D3D11_BLEND_ZERO           },        // GS_BLDST_ONE_A_ZERO
				{ D3D11_BLEND_INV_SRC1_ALPHA, D3D11_BLEND_INV_SRC1_ALPHA },        // GS_BLDST_ONEMINUSSRC1ALPHA
			};

			D3D11_BLEND_OP BlendOp[GS_BLEND_OP_MASK >> BlendOpShift] =
			{
				D3D11_BLEND_OP_ADD,     // 0 (unspecified): Default
				D3D11_BLEND_OP_MAX,     // GS_BLOP_MAX / GS_BLALPHA_MAX
				D3D11_BLEND_OP_MIN,     // GS_BLOP_MIN / GS_BLALPHA_MIN
			};

			blendDesc.RenderTarget[0].SrcBlend = SrcBlendFactors[(renderState & GS_BLSRC_MASK) >> SrcFactorShift].BlendColor;
			blendDesc.RenderTarget[0].SrcBlendAlpha = SrcBlendFactors[(renderState & GS_BLSRC_MASK) >> SrcFactorShift].BlendAlpha;
			blendDesc.RenderTarget[0].DestBlend = DstBlendFactors[(renderState & GS_BLDST_MASK) >> DstFactorShift].BlendColor;
			blendDesc.RenderTarget[0].DestBlendAlpha = DstBlendFactors[(renderState & GS_BLDST_MASK) >> DstFactorShift].BlendAlpha;

			for (size_t i = 0; i < CD3D9Renderer::RT_STACK_WIDTH; ++i)
			{
				blendDesc.RenderTarget[i].BlendOp = BlendOp[(renderState & GS_BLEND_OP_MASK) >> BlendOpShift];
				blendDesc.RenderTarget[i].BlendOpAlpha = BlendOp[(renderState & GS_BLALPHA_MASK) >> BlendAlphaOpShift];
			}

			if ((renderState & GS_BLALPHA_MASK) == GS_BLALPHA_MIN)
			{
				blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
				blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			}
		}
	}

	// Color mask
	{
		uint32 mask = 0xfffffff0 | ((renderState & GS_COLMASK_MASK) >> GS_COLMASK_SHIFT);
		mask = (~mask) & 0xf;
		for (uint32 i = 0; i < CD3D9Renderer::RT_STACK_WIDTH; ++i)
			blendDesc.RenderTarget[i].RenderTargetWriteMask = mask;
	}

	// Depth-Stencil
	{
		depthStencilDesc.DepthWriteMask = (renderState & GS_DEPTHWRITE) ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthEnable = !(renderState & GS_NODEPTHTEST);

		const uint32 DepthFuncShift = 20;
		D3D11_COMPARISON_FUNC DepthFunc[GS_DEPTHFUNC_MASK >> DepthFuncShift] =
		{
			D3D11_COMPARISON_LESS_EQUAL,     // GS_DEPTHFUNC_LEQUAL
			D3D11_COMPARISON_EQUAL,          // GS_DEPTHFUNC_EQUAL
			D3D11_COMPARISON_GREATER,        // GS_DEPTHFUNC_GREAT
			D3D11_COMPARISON_LESS,           // GS_DEPTHFUNC_LESS
			D3D11_COMPARISON_GREATER_EQUAL,  // GS_DEPTHFUNC_GEQUAL
			D3D11_COMPARISON_NOT_EQUAL,      // GS_DEPTHFUNC_NOTEQUAL
			D3D11_COMPARISON_EQUAL,          // GS_DEPTHFUNC_HIZEQUAL
		};

		depthStencilDesc.DepthFunc = DepthFunc[(m_RenderState & GS_DEPTHFUNC_MASK) >> DepthFuncShift];

		depthStencilDesc.StencilEnable = (renderState & GS_STENCIL);
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
}

uint8 CDeviceGraphicsPSODesc::CombineVertexStreamMasks(uint8 fromShader, uint8 fromObject) const
{
	uint8 result = fromShader;

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
CDeviceComputePSODesc::CDeviceComputePSODesc(CDeviceResourceLayout* pResourceLayout, const SComputePipelineStateDescription& pipelineDesc)
{
	InitWithDefaults();

	m_pResourceLayout = pResourceLayout;
	m_pShader = reinterpret_cast<CShader*>(pipelineDesc.shaderItem.m_pShader);
	if (auto pTech = m_pShader->GetTechnique(pipelineDesc.shaderItem.m_nTechnique, pipelineDesc.technique, true))
	{
		m_technique = pTech->m_NameCRC;
	}

	m_ShaderFlags_RT = pipelineDesc.objectRuntimeMask;
	m_ShaderFlags_MDV = pipelineDesc.objectFlags_MDV;
}

CDeviceComputePSODesc::CDeviceComputePSODesc(CDeviceResourceLayout* pResourceLayout, CShader* pShader, const CCryNameTSCRC& technique, uint64 rtFlags, uint32 mdFlags, uint32 mdvFlags)
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
	// NOTE: need to make sure both structs are binary equivalent for hashing and comparison
	memcpy(this, &other, sizeof(CDeviceComputePSODesc));
	if (m_pShader) m_pShader->AddRef();

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

CDeviceResourceSet::CDeviceResourceSet(EFlags flags)
	: m_bValid(false)
	, m_bDirty(false)
{
	Clear();
	m_Flags = flags;
}

CDeviceResourceSet::CDeviceResourceSet(const CDeviceResourceSet& other)
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	m_Textures = other.m_Textures;
	m_Samplers = other.m_Samplers;
	m_Buffers = other.m_Buffers;
	m_ConstantBuffers = other.m_ConstantBuffers;

	m_bValid = false;
	m_bDirty = true;
	m_Flags = other.m_Flags;
}

CDeviceResourceSet::~CDeviceResourceSet()
{
	Clear();
}

void CDeviceResourceSet::SetDirty(bool bDirty)
{
	m_bDirty = true;
}

void CDeviceResourceSet::Clear(bool bTextures)
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	if (bTextures)
	{
		for (auto& rsTexBind : m_Textures)
		{
			if (CTexture* pTex = std::get<1>(rsTexBind.second.resource))
				pTex->RemoveInvalidateCallbacks(this);
		}
		m_Textures.clear();
	}
	m_Samplers.clear();
	m_ConstantBuffers.clear();
	m_Buffers.clear();
	m_bDirty = true;
}

void CDeviceResourceSet::SetTexture(ShaderSlot shaderSlot, _smart_ptr<CTexture> pTexture, SResourceView::KeyType resourceViewID, EShaderStage shaderStages)
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	CTexture* const pTextureRaw = pTexture.get();
	STextureData texData(std::make_tuple(resourceViewID, std::move(pTexture)), shaderStages);
	const auto it = m_Textures.find(shaderSlot);
	const bool bAlreadyExists = it != m_Textures.end();
	if (bAlreadyExists)
	{
		if (it->second.resource == texData.resource)
		{
			it->second.shaderStages = shaderStages;
			return;
		}

		CTexture* const pPreviousTexture = std::get<1>(it->second.resource);
		if (pPreviousTexture)
		{
			pPreviousTexture->RemoveInvalidateCallbacks(this);
		}

		it->second.swap(texData);
	}
	else
	{
		m_Textures[shaderSlot].swap(texData);
	}

	if (pTextureRaw)
	{
		auto lambdaTextureCallback = [this](uint32 dirtyFlags) { this->OnTextureChanged(dirtyFlags); };
		pTextureRaw->AddInvalidateCallback(this, lambdaTextureCallback);
	}

	m_bDirty = true;
}

void CDeviceResourceSet::SetSampler(ShaderSlot shaderSlot, int sampler, EShaderStage shaderStages)
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	SSamplerData samplerData(sampler, shaderStages);
	if (m_Samplers.find(shaderSlot) != m_Samplers.end() && m_Samplers[shaderSlot] == samplerData)
		return;

	m_Samplers[shaderSlot].swap(samplerData);
	m_bDirty = true;
}

void CDeviceResourceSet::SetConstantBuffer(ShaderSlot shaderSlot, _smart_ptr<CConstantBuffer> pBuffer, EShaderStage shaderStages)
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	SConstantBufferData cbData(pBuffer, shaderStages);
	if (m_ConstantBuffers.find(shaderSlot) != m_ConstantBuffers.end() && m_ConstantBuffers[shaderSlot] == cbData)
		return;

	m_ConstantBuffers[shaderSlot].swap(cbData);
	m_bDirty = true;
}

void CDeviceResourceSet::SetBuffer(ShaderSlot shaderSlot, const CGpuBuffer& buffer, EShaderStage shaderStages)
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	SBufferData bufferData(buffer, shaderStages);
	if (m_Buffers.find(shaderSlot) != m_Buffers.end() && m_Buffers[shaderSlot] == bufferData)
		return;

	m_Buffers[shaderSlot].swap(bufferData);
	m_bDirty = true;
}

EShaderStage CDeviceResourceSet::GetShaderStages() const
{
	EShaderStage result = EShaderStage_None;

	for (const auto& it : m_ConstantBuffers)
		result |= it.second.shaderStages;

	for (const auto& it : m_Textures)
		result |= it.second.shaderStages;

	for (const auto& it : m_Samplers)
		result |= it.second.shaderStages;

	return result;
}

bool CDeviceResourceSet::Fill(CShader* pShader, CShaderResources* pResources, EShaderStage shaderStages)
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	Clear(false);

	uint64 bindSlotMask = 0;
	static_assert(MAX_TMU <= sizeof(bindSlotMask) * 8, "Need a larger type for mask");

	for (auto& texture : m_Textures)
	{
		const uint32 bindSlot = texture.first;
		bindSlotMask |= uint64(1) << bindSlot;
	}

	for (EEfResTextures texType = EFTT_DIFFUSE; texType < EFTT_MAX; texType = EEfResTextures(texType + 1))
	{
		SResourceView::KeyType view = SResourceView::DefaultView;
		CTexture* pTex = TextureHelpers::LookupTexDefault(texType);
		if (pResources->m_Textures[texType])
		{
			const STexSamplerRT& smp = pResources->m_Textures[texType]->m_Sampler;
			if (smp.m_pDynTexSource)
			{
				view = SResourceView::DefaultViewSRGB;
				m_Flags = EFlags(m_Flags | EFlags_DynamicUpdates);

				if (ITexture* pITex = smp.m_pDynTexSource->GetTexture())
				{
					int texID = pITex->GetTextureID();
					m_Flags = EFlags(m_Flags & ~EFlags_PendingAllocation);
					pTex = CTexture::GetByID(texID);
				}
				else
				{
					m_Flags = EFlags(m_Flags | EFlags_PendingAllocation);
					pTex = CTexture::s_ptexBlackAlpha;
				}
			}
			else
			{
				pTex = smp.m_pTex;
			}
		}

		CRY_ASSERT(pTex);
		auto bindSlot = IShader::GetTextureSlot(texType);
		SetTexture(bindSlot, pTex, view, shaderStages);
		bindSlotMask &= ~(uint64(1) << bindSlot);
	}

	// Any texture that was not set, reset it now.
	for (uint32 bindSlot = 0; bindSlot < MAX_TMU; ++bindSlot)
	{
		if ((bindSlotMask & (uint64(1) << bindSlot)) != 0)
		{
			m_Textures.erase(bindSlot);
		}
	}

	// Eventually we should only have one constant buffer for all shader stages. for now just pick the one from the pixel shader
	m_ConstantBuffers[eConstantBufferShaderSlot_PerMaterial] = SConstantBufferData(pResources->m_pCB, shaderStages);
	m_bDirty = true;

	return true;
}

void CDeviceResourceSet::OnTextureChanged(uint32 dirtyFlags)
{
	SetDirty(true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SDeviceResourceLayoutDesc::SetInlineConstants(uint32 numConstants)
{
	m_InlineConstantCount += numConstants;
}

void SDeviceResourceLayoutDesc::SetResourceSet(uint32 bindSlot, CDeviceResourceSetPtr pResourceSet)
{
	m_ResourceSets[bindSlot] = pResourceSet;
}

void SDeviceResourceLayoutDesc::SetConstantBuffer(uint32 bindSlot, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages)
{
	m_ConstantBuffers[bindSlot].shaderSlot = shaderSlot;
	m_ConstantBuffers[bindSlot].shaderStages = shaderStages;
}

// local helper functions for comparing arrays of resources by bind info
enum eComparisonResult { eLess = -1, eEqual, eGreater };

eComparisonResult compareResourceBindings(int shaderSlotA, EShaderStage shaderStagesA, int shaderSlotB, EShaderStage shaderStagesB)
{
	if (shaderSlotA != shaderSlotB)
		return shaderSlotA < shaderSlotB ? eLess : eGreater;

	if (shaderStagesA != shaderStagesB)
		return shaderStagesA < shaderStagesB ? eLess : eGreater;

	return eEqual;
}

template<typename T>
eComparisonResult compareResourceLists(const T& a, const T& b)
{
	if (a.size() != b.size())
		return a.size() < b.size() ? eLess : eGreater;

	for (auto itA = a.begin(), itB = b.begin(); itA != a.end(); ++itA, ++itB)
	{
		if (auto cmp = compareResourceBindings(itA->first, itA->second.shaderStages, itB->first, itB->second.shaderStages))
			return cmp;
	}

	return eEqual;
}

bool SDeviceResourceLayoutDesc::operator<(const SDeviceResourceLayoutDesc& other) const
{
	if (m_InlineConstantCount != other.m_InlineConstantCount)
		return m_InlineConstantCount < other.m_InlineConstantCount;

	if (m_ResourceSets.size() != other.m_ResourceSets.size())
		return m_ResourceSets.size() < other.m_ResourceSets.size();

	if (m_ConstantBuffers.size() != other.m_ConstantBuffers.size())
		return m_ConstantBuffers.size() < other.m_ConstantBuffers.size();

	// constant buffers
	for (auto cb0 = m_ConstantBuffers.begin(), cb1 = other.m_ConstantBuffers.begin(); cb0 != m_ConstantBuffers.end(); ++cb0, ++cb1)
	{
		if (cb0->first != cb1->first)
			return cb0->first < cb1->first;

		if (auto cmp = compareResourceBindings(cb0->second.shaderSlot, cb0->second.shaderStages, cb1->second.shaderSlot, cb1->second.shaderStages))
			return cmp == eLess;
	}

	// resource sets
	for (auto rs0 = m_ResourceSets.begin(), rs1 = other.m_ResourceSets.begin(); rs0 != m_ResourceSets.end(); ++rs0, ++rs1)
	{
		if (rs0->first != rs1->first)
			return rs0->first < rs1->first;

		if (auto cmp = compareResourceLists(rs0->second->m_ConstantBuffers, rs1->second->m_ConstantBuffers))
			return cmp == eLess;

		if (auto cmp = compareResourceLists(rs0->second->m_Textures, rs1->second->m_Textures))
			return cmp == eLess;

		if (auto cmp = compareResourceLists(rs0->second->m_Buffers, rs1->second->m_Buffers))
			return cmp == eLess;
	}

	return false;
}

UsedBindSlotSet SDeviceResourceLayoutDesc::GetRequiredResourceBindings() const
{
	UsedBindSlotSet result = 0;

	for (const auto& cb : m_ConstantBuffers)
		result[cb.first] = true;

	for (const auto& rs : m_ResourceSets)
		result[rs.first] = true;

	return result;
}

bool SDeviceResourceLayoutDesc::IsValid() const
{
	// Need to have at least one resource set or constant buffer/inline constants
	if (m_ResourceSets.empty() && m_ConstantBuffers.empty() && m_InlineConstantCount == 0)
	{
		CRY_ASSERT_MESSAGE(false, "Invalid Resource Layout: no data");
		return false;
	}

	// Check for overlapping resource set and constant buffer bind slots
	std::set<uint32> usedBindSlots;
	std::multimap<ShaderSlot, EShaderStage> shaderTSlotBinds;
	std::multimap<ShaderSlot, EShaderStage> shaderCBufferBinds;

	if (m_InlineConstantCount > 0)
		usedBindSlots.insert(0);

	auto validateShaderSlotBind = [](int shaderSlot, EShaderStage shaderStages, std::multimap<ShaderSlot, EShaderStage>& usedShaderSlots)
	{
		auto range = usedShaderSlots.equal_range(shaderSlot);
		for (auto it = range.first; it != range.second; ++it)
		{
			if (it->second & shaderStages)
			{
				if (shaderSlot == EReservedTextureSlot_TerrainBaseMap) // TODO: Remove once we are not deliberately overwriting terrain base map from
					return true;                                         //       per pass resources with custom version from per instance extra resources

				CRY_ASSERT_MESSAGE(false, "Invalid Resource Layout: Cannot bind multiple resources to same shader slot and shader stages");
				return false;
			}
		}

		usedShaderSlots.insert(std::make_pair(shaderSlot, shaderStages));
		return true;
	};

	auto validateResourceSetShaderStages = [](EShaderStage& resourceSetShaderStages, EShaderStage currentItemShaderStages)
	{
		if (resourceSetShaderStages != EShaderStage_None && currentItemShaderStages != resourceSetShaderStages)
		{
			CRY_ASSERT_MESSAGE(false, "Invalid Resource Layout: Cannot bind multiple resources to same shader slot (even when shader stages differ) - DX12 limitation");
			return false;
		}

		resourceSetShaderStages = currentItemShaderStages;
		return true;
	};

	for (const auto& rs : m_ResourceSets)
	{
		const CDeviceResourceSetPtr& resourceSet = rs.second;

		if (resourceSet == nullptr)
		{
			CRY_ASSERT_MESSAGE(false, "Invalid Resource Layout: NULL resource set");
			return false;
		}

		if (usedBindSlots.insert(rs.first).second == false)
		{
			CRY_ASSERT_MESSAGE(false, "Invalid Resource Layout: Multiple resources on same bind slot");
			return false;
		}

		EShaderStage currentShaderStages = EShaderStage_None;

		for (const auto& rsTexBind : resourceSet->m_Textures)
		{
			const ShaderSlot slot = rsTexBind.first;
			const CDeviceResourceSet::STextureData& textureData = rsTexBind.second;

			if (!validateResourceSetShaderStages(currentShaderStages, textureData.shaderStages))
				return false;

			if (!validateShaderSlotBind(slot, textureData.shaderStages, shaderTSlotBinds))
				return false;
		}

		std::array<int, eHWSC_Num> bufferCountPerShaderStage;
		bufferCountPerShaderStage.fill(0);

		for (const auto& rsBufferBind : resourceSet->m_Buffers)
		{
			const ShaderSlot slot = rsBufferBind.first;
			const CDeviceResourceSet::SBufferData& bufferData = rsBufferBind.second;

			if (!validateResourceSetShaderStages(currentShaderStages, bufferData.shaderStages))
				return false;

			if (!validateShaderSlotBind(slot, bufferData.shaderStages, shaderTSlotBinds))
				return false;

			for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass != eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
			{
				if (bufferData.shaderStages & SHADERSTAGE_FROM_SHADERCLASS(shaderClass))
				{
					if (++bufferCountPerShaderStage[shaderClass] > ResourceSetBufferCount)
					{
						CRY_ASSERT_MESSAGE(false, "Invalid Resource Layout: Too many buffers in resource set. Consider increasing ResourceSetBufferCount");
						return false;
					}
				}
			}
		}

		for (const auto& rsCbBind : resourceSet->m_ConstantBuffers)
		{
			const ShaderSlot slot = rsCbBind.first;
			const CDeviceResourceSet::SConstantBufferData& constantBufferData = rsCbBind.second;

			if (!validateShaderSlotBind(slot, constantBufferData.shaderStages, shaderCBufferBinds))
				return false;

			if (!validateResourceSetShaderStages(currentShaderStages, constantBufferData.shaderStages))
				return false;
		}
	}

	for (const auto& cbBind : m_ConstantBuffers)
	{
		const SInlineConstantBuffer& inlineConstantBuffer = cbBind.second;

		if (usedBindSlots.insert(cbBind.first).second == false)
		{
			CRY_ASSERT_MESSAGE(false, "Invalid Resource Layout: Multiple resources on same bind slot");
			return false;
		}

		if (!validateShaderSlotBind(inlineConstantBuffer.shaderSlot, inlineConstantBuffer.shaderStages, shaderCBufferBinds))
			return false;
	}

	// Make sure there are no 'holes' in the used binding slots
	{
		int previousSlot = -1;
		for (auto slot : usedBindSlots)
		{
			if (slot != previousSlot + 1)
			{
				CRY_ASSERT_MESSAGE(false, "Invalid Resource Layout: gap in bind slots");
				return false;
			}

			previousSlot = slot;
		}

		if (previousSlot > EResourceLayoutSlot_Max)
			return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeviceGraphicsCommandList::Reset()
{
	m_pCurrentPipelineState = nullptr;
	m_pCurrentResourceLayout = nullptr;
	m_pCurrentResources.fill(nullptr);
	m_CurrentStencilRef = -1;
	m_RequiredResourceBindings = 0;
	m_ValidResourceBindings = 0;

	m_CurrentVertexStreams = nullptr;
	m_CurrentIndexStream = nullptr;

	ResetImpl();
}

void CDeviceGraphicsCommandList::PrepareResourcesForUse(uint32 bindSlot, CDeviceResourceSet* pResources) const
{
	PrepareResourcesForUseImpl(bindSlot, pResources);
}

void CDeviceGraphicsCommandList::PrepareInlineConstantBufferForUse(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass) const
{
	PrepareInlineConstantBufferForUseImpl(bindSlot, pBuffer, shaderSlot, shaderClass);
}

void CDeviceGraphicsCommandList::PrepareInlineConstantBufferForUse(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages) const
{
	PrepareInlineConstantBufferForUseImpl(bindSlot, pBuffer, shaderSlot, shaderStages);
}

void CDeviceGraphicsCommandList::SetPipelineState(CDeviceGraphicsPSOConstPtr devicePSO)
{
	if (m_pCurrentPipelineState != devicePSO)
	{
		std::swap(m_pCurrentPipelineState, devicePSO);
		SetPipelineStateImpl(m_pCurrentPipelineState);

#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumPSOSwitches);
#endif
	}
}

void CDeviceGraphicsCommandList::SetResourceLayout(CDeviceResourceLayoutConstPtr resourceLayout)
{
	if (m_pCurrentResourceLayout != resourceLayout)
	{
		// If a root signature is changed on a command list, all previous root signature bindings
		// become stale and all newly expected bindings must be set before Draw/Dispatch
		m_pCurrentResources.fill(nullptr);

		m_RequiredResourceBindings = resourceLayout->GetRequiredResourceBindings();
		m_ValidResourceBindings = 0;

		std::swap(m_pCurrentResourceLayout, resourceLayout);
		SetResourceLayoutImpl(m_pCurrentResourceLayout);

#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumLayoutSwitches);
#endif
	}
}

void CDeviceGraphicsCommandList::SetResources(uint32 bindSlot, const CDeviceResourceSet* pResources)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);
	if (m_pCurrentResources[bindSlot] != pResources)
	{
		m_pCurrentResources[bindSlot] = pResources;
		m_ValidResourceBindings[bindSlot] = pResources->IsValid();

		SetResourcesImpl(bindSlot, pResources);

#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumResourceSetSwitches);
#endif
	}
}

void CDeviceGraphicsCommandList::SetInlineConstantBuffer(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass)
{
	// TODO: remove redundant InlineConstantBuffer sets
	//	if (m_pCurrentResources[bindSlot] != ((char*)pBuffer->m_base_ptr + pBuffer->m_offset))
	{
		//		m_pCurrentResources[bindSlot] = ((char*)pBuffer->m_base_ptr + pBuffer->m_offset);

#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumInlineSets);
#endif

		m_ValidResourceBindings[bindSlot] = pBuffer != nullptr;
		SetInlineConstantBufferImpl(bindSlot, pBuffer, shaderSlot, shaderClass);
	}
}

void CDeviceGraphicsCommandList::SetInlineConstantBuffer(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages)
{
	// TODO: remove redundant InlineConstantBuffer sets
	//	if (m_pCurrentResources[bindSlot] != ((char*)pBuffer->m_base_ptr + pBuffer->m_offset))
	{
		//		m_pCurrentResources[bindSlot] = ((char*)pBuffer->m_base_ptr + pBuffer->m_offset);

#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumInlineSets);
#endif

		m_ValidResourceBindings[bindSlot] = pBuffer != nullptr;
		SetInlineConstantBufferImpl(bindSlot, pBuffer, shaderSlot, shaderStages);
	}
}

void CDeviceGraphicsCommandList::SetInlineConstants(uint32 bindSlot, uint32 constantCount, float* pConstants)
{
	m_ValidResourceBindings[bindSlot] = true;
	SetInlineConstantsImpl(bindSlot, constantCount, pConstants);

#if defined(ENABLE_PROFILING_CODE)
	CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumInlineSets);
#endif
}

void CDeviceGraphicsCommandList::SetStencilRef(uint8 stencilRefValue)
{
	if (m_CurrentStencilRef != int(stencilRefValue))
	{
		m_CurrentStencilRef = stencilRefValue;
		SetStencilRefImpl(stencilRefValue);
	}
}

void CDeviceGraphicsCommandList::Draw(uint32 VertexCountPerInstance, uint32 InstanceCount, uint32 StartVertexLocation, uint32 StartInstanceLocation)
{
	if (m_ValidResourceBindings == m_RequiredResourceBindings)
	{
		DrawImpl(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);

#if defined(ENABLE_PROFILING_CODE)
		int nPrimitives;

		switch (m_pCurrentPipelineState->m_PrimitiveTypeForProfiling)
		{
		case eptTriangleList:
			nPrimitives = VertexCountPerInstance / 3;
			assert(VertexCountPerInstance % 3 == 0);
			break;
		case eptTriangleStrip:
			nPrimitives = VertexCountPerInstance - 2;
			assert(VertexCountPerInstance > 2);
			break;
		case eptLineList:
			nPrimitives = VertexCountPerInstance / 2;
			assert(VertexCountPerInstance % 2 == 0);
			break;
		case eptLineStrip:
			nPrimitives = VertexCountPerInstance - 1;
			assert(VertexCountPerInstance > 1);
			break;
		case eptPointList:
			nPrimitives = VertexCountPerInstance;
			assert(VertexCountPerInstance > 0);
			break;
		case ept1ControlPointPatchList:
			nPrimitives = VertexCountPerInstance;
			assert(VertexCountPerInstance > 0);
			break;

	#ifndef _RELEASE
		default:
			assert(0);
	#endif
		}

		CD3D9Renderer* const __restrict rd = gcpRendD3D;

		CryInterlockedAdd(&rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_nPolygons[rd->m_RP.m_nPassGroupDIP], nPrimitives * InstanceCount);
		CryInterlockedIncrement(&rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_nDIPs[rd->m_RP.m_nPassGroupDIP]);
#endif
	}
	else
	{
		gcpRendD3D->GetGraphicsPipeline().IncrementNumInvalidDrawcalls();
	}
}

void CDeviceGraphicsCommandList::DrawIndexed(uint32 IndexCountPerInstance, uint32 InstanceCount, uint32 StartIndexLocation, int BaseVertexLocation, uint32 StartInstanceLocation)
{
	DrawIndexedImpl(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);

#if defined(ENABLE_PROFILING_CODE)
	int nPrimitives;

	switch (m_pCurrentPipelineState->m_PrimitiveTypeForProfiling)
	{
	case eptTriangleList:
		nPrimitives = IndexCountPerInstance / 3;
		assert(IndexCountPerInstance % 3 == 0);
		break;
	case ept4ControlPointPatchList:
		nPrimitives = IndexCountPerInstance >> 2;
		assert(IndexCountPerInstance % 4 == 0);
		break;
	case ept3ControlPointPatchList:
		nPrimitives = IndexCountPerInstance / 3;
		assert(IndexCountPerInstance % 3 == 0);
		break;
	case eptTriangleStrip:
		nPrimitives = IndexCountPerInstance - 2;
		assert(IndexCountPerInstance > 2);
		break;
	case eptLineList:
		nPrimitives = IndexCountPerInstance >> 1;
		assert(IndexCountPerInstance % 2 == 0);
		break;
	#ifdef _DEBUG
	default:
		assert(0);
	#endif
	}

	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	CryInterlockedAdd(&rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_nPolygons[rd->m_RP.m_nPassGroupDIP], nPrimitives * InstanceCount);
	CryInterlockedIncrement(&rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_nDIPs[rd->m_RP.m_nPassGroupDIP]);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeviceComputeCommandList::Reset()
{
	m_pCurrentPipelineState = nullptr;
	m_pCurrentResourceLayout = nullptr;
	m_pCurrentResources.fill(nullptr);
	m_RequiredResourceBindings = 0;
	m_ValidResourceBindings = 0;

	ResetImpl();
}

void CDeviceComputeCommandList::PrepareResourcesForUse(uint32 bindSlot, CDeviceResourceSet* pResources) const
{
	PrepareResourcesForUseImpl(bindSlot, pResources);
}

void CDeviceComputeCommandList::PrepareInlineConstantBufferForUse(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass) const
{
	PrepareInlineConstantBufferForUseImpl(bindSlot, pBuffer, shaderSlot, shaderClass);
}

void CDeviceComputeCommandList::SetPipelineState(CDeviceComputePSOConstPtr devicePSO)
{
	if (m_pCurrentPipelineState != devicePSO)
	{
		std::swap(m_pCurrentPipelineState, devicePSO);
		SetPipelineStateImpl(m_pCurrentPipelineState);

#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumPSOSwitches);
#endif
	}
}

void CDeviceComputeCommandList::SetResourceLayout(CDeviceResourceLayoutConstPtr resourceLayout)
{
	if (m_pCurrentResourceLayout != resourceLayout)
	{
		// If a root signature is changed on a command list, all previous root signature bindings
		// become stale and all newly expected bindings must be set before Draw/Dispatch
		m_pCurrentResources.fill(nullptr);

		m_RequiredResourceBindings = resourceLayout->GetRequiredResourceBindings();
		m_ValidResourceBindings = 0;

		std::swap(m_pCurrentResourceLayout, resourceLayout);
		SetResourceLayoutImpl(m_pCurrentResourceLayout);

#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumLayoutSwitches);
#endif
	}
}

void CDeviceComputeCommandList::SetResources(uint32 bindSlot, const CDeviceResourceSet* pResources)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);
	if (m_pCurrentResources[bindSlot] != pResources)
	{
		m_pCurrentResources[bindSlot] = pResources;
		m_ValidResourceBindings[bindSlot] = pResources->IsValid();

		SetResourcesImpl(bindSlot, pResources);

#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumResourceSetSwitches);
#endif
	}
}

void CDeviceComputeCommandList::SetInlineConstantBuffer(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass)
{
	m_ValidResourceBindings[bindSlot] = pBuffer != nullptr;
	SetInlineConstantBufferImpl(bindSlot, pBuffer, shaderSlot, shaderClass);
}

void CDeviceComputeCommandList::SetInlineConstants(uint32 bindSlot, uint32 constantCount, float* pConstants)
{
	m_ValidResourceBindings[bindSlot] = true;
	SetInlineConstantsImpl(bindSlot, constantCount, pConstants);
}

void CDeviceComputeCommandList::Dispatch(uint32 X, uint32 Y, uint32 Z)
{
	DispatchImpl(X, Y, Z);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CDeviceGraphicsPSOPtr CDeviceObjectFactory::CreateGraphicsPSO(const CDeviceGraphicsPSODesc& psoDesc)
{
	auto it = m_GraphicsPsoCache.find(psoDesc);
	if (it != m_GraphicsPsoCache.end())
		return it->second;

	auto pPso = CreateGraphicsPSOImpl(psoDesc);
	m_GraphicsPsoCache.emplace(psoDesc, pPso);

	if (!pPso->IsValid())
		m_InvalidGraphicsPsos.emplace(psoDesc, pPso);

	return pPso;
}

CDeviceComputePSOPtr CDeviceObjectFactory::CreateComputePSO(const CDeviceComputePSODesc& psoDesc)
{
	auto it = m_ComputePsoCache.find(psoDesc);
	if (it != m_ComputePsoCache.end())
		return it->second;

	auto pPso = CreateComputePSOImpl(psoDesc);
	m_ComputePsoCache.emplace(psoDesc, pPso);

	if (!pPso->IsValid())
		m_InvalidComputePsos.emplace(psoDesc, pPso);

	return pPso;
}

CDeviceResourceLayoutPtr CDeviceObjectFactory::CreateResourceLayout(const SDeviceResourceLayoutDesc& resourceLayoutDesc)
{
	CDeviceResourceLayoutPtr pResult = nullptr;

	if (resourceLayoutDesc.IsValid())
	{
		auto it = m_ResourceLayoutCache.find(resourceLayoutDesc);
		if (it != m_ResourceLayoutCache.end())
			return it->second;

		if (pResult = CreateResourceLayoutImpl(resourceLayoutDesc))
		{
			m_ResourceLayoutCache[resourceLayoutDesc] = pResult;
		}
	}

	return pResult;
}

CDeviceInputStream* CDeviceObjectFactory::CreateVertexStreamSet(uint32 streamCount, const SStreamInfo* streams)
{
	CDeviceInputStream* vertexStreams = new CDeviceInputStream[VSF_NUM];
	CDeviceInputStream* vertexStreamSet = nullptr;
	bool vertexFilled = false;

	for (int i = 0; i < streamCount; ++i)
		vertexFilled |= !!(vertexStreams[i] = streams[i]);

	if ((vertexStreamSet = (vertexFilled ? *m_UniqueVertexStreams.insert(vertexStreams).first : nullptr)) != vertexStreams)
		delete [] vertexStreams;

	return vertexStreamSet;
}

CDeviceInputStream* CDeviceObjectFactory::CreateIndexStreamSet(const SStreamInfo* stream)
{
	CDeviceInputStream* indexStream = new CDeviceInputStream();
	CDeviceInputStream* indexStreamSet = nullptr;
	bool indexFilled = false;

	indexFilled |= !!(*indexStream = *stream);

	if ((indexStreamSet = (indexFilled ? *m_UniqueIndexStreams.insert(indexStream).first : nullptr)) != indexStream)
		delete indexStream;

	return indexStreamSet;
}

void CDeviceObjectFactory::ReleaseResources()
{
	ReleaseResourcesImpl();

	m_GraphicsPsoCache.clear();
	m_InvalidGraphicsPsos.clear();

	m_ComputePsoCache.clear();
	m_InvalidComputePsos.clear();

	m_ResourceLayoutCache.clear();
}

void CDeviceObjectFactory::ReloadPipelineStates()
{
	for (auto& it : m_GraphicsPsoCache)
	{
		if (!it.second->Init(it.first))
			m_InvalidGraphicsPsos.emplace(it.first, it.second);
	}

	for (auto& it : m_ComputePsoCache)
	{
		if (!it.second->Init(it.first))
			m_InvalidComputePsos.emplace(it.first, it.second);
	}
}

void CDeviceObjectFactory::UpdatePipelineStates()
{
	for (auto it = m_InvalidGraphicsPsos.begin(), itEnd = m_InvalidGraphicsPsos.end(); it != itEnd; )
	{
		auto itCurrentPSO = it++;
		auto pPso = itCurrentPSO->second.lock();

		if (!pPso || pPso->Init(itCurrentPSO->first))
			m_InvalidGraphicsPsos.erase(itCurrentPSO);
	}

	for (auto it = m_InvalidComputePsos.begin(), itEnd = m_InvalidComputePsos.end(); it != itEnd; )
	{
		auto itCurrentPSO = it++;
		auto pPso = itCurrentPSO->second.lock();

		if (!pPso || pPso->Init(itCurrentPSO->first))
			m_InvalidComputePsos.erase(itCurrentPSO);
	}
}
