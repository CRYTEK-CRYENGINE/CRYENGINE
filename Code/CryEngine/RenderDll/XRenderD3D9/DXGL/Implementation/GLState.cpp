// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GLState.cpp
//  Version:     v1.00
//  Created:     29/04/2013 by Valerio Guagliumi.
//  Description: Implementation of the functions used to create states
//               and apply them to an OpenGL device
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>
#include "GLState.hpp"
#include "GLDevice.hpp"

namespace NCryOpenGL
{

bool GetComparisonFunc(GLenum& eGLCompareFunc, D3D11_COMPARISON_FUNC eD3DCompareFunc)
{
	switch (eD3DCompareFunc)
	{
	case D3D11_COMPARISON_NEVER:
		eGLCompareFunc = GL_NEVER;
		break;
	case D3D11_COMPARISON_LESS:
		eGLCompareFunc = GL_LESS;
		break;
	case D3D11_COMPARISON_EQUAL:
		eGLCompareFunc = GL_EQUAL;
		break;
	case D3D11_COMPARISON_LESS_EQUAL:
		eGLCompareFunc = GL_LEQUAL;
		break;
	case D3D11_COMPARISON_GREATER:
		eGLCompareFunc = GL_GREATER;
		break;
	case D3D11_COMPARISON_NOT_EQUAL:
		eGLCompareFunc = GL_NOTEQUAL;
		break;
	case D3D11_COMPARISON_GREATER_EQUAL:
		eGLCompareFunc = GL_GEQUAL;
		break;
	case D3D11_COMPARISON_ALWAYS:
		eGLCompareFunc = GL_ALWAYS;
		break;
	default:
		return false;
	}
	return true;
}

bool GetStencilOperation(GLenum& eGLStencilOp, D3D11_STENCIL_OP eD3DStencilOp)
{
	switch (eD3DStencilOp)
	{
	case D3D11_STENCIL_OP_KEEP:
		eGLStencilOp = GL_KEEP;
		break;
	case D3D11_STENCIL_OP_ZERO:
		eGLStencilOp = GL_ZERO;
		break;
	case D3D11_STENCIL_OP_REPLACE:
		eGLStencilOp = GL_REPLACE;
		break;
	case D3D11_STENCIL_OP_INCR_SAT:
		eGLStencilOp = GL_INCR;
		break;
	case D3D11_STENCIL_OP_DECR_SAT:
		eGLStencilOp = GL_DECR;
		break;
	case D3D11_STENCIL_OP_INVERT:
		eGLStencilOp = GL_INVERT;
		break;
	case D3D11_STENCIL_OP_INCR:
		eGLStencilOp = GL_INCR_WRAP;
		break;
	case D3D11_STENCIL_OP_DECR:
		eGLStencilOp = GL_DECR_WRAP;
		break;
	default:
		return false;
	}
	return true;
}

bool GetBlendEquation(GLenum& eGLBlendEquation, D3D11_BLEND_OP eD3DBlendOp)
{
	switch (eD3DBlendOp)
	{
	case D3D11_BLEND_OP_ADD:
		eGLBlendEquation = GL_FUNC_ADD;
		break;
	case D3D11_BLEND_OP_SUBTRACT:
		eGLBlendEquation = GL_FUNC_SUBTRACT;
		break;
	case D3D11_BLEND_OP_REV_SUBTRACT:
		eGLBlendEquation = GL_FUNC_REVERSE_SUBTRACT;
		break;
	case D3D11_BLEND_OP_MIN:
		eGLBlendEquation = GL_MIN;
		break;
	case D3D11_BLEND_OP_MAX:
		eGLBlendEquation = GL_MAX;
		break;
	default:
		return false;
	}
	return true;
}

bool GetBlendFunction(GLenum& eGLBlendFunction, D3D11_BLEND eD3DBlend)
{
	switch (eD3DBlend)
	{
	case D3D11_BLEND_ZERO:
		eGLBlendFunction = GL_ZERO;
		break;
	case D3D11_BLEND_ONE:
		eGLBlendFunction = GL_ONE;
		break;
	case D3D11_BLEND_SRC_COLOR:
		eGLBlendFunction = GL_SRC_COLOR;
		break;
	case D3D11_BLEND_INV_SRC_COLOR:
		eGLBlendFunction = GL_ONE_MINUS_SRC_COLOR;
		break;
	case D3D11_BLEND_SRC_ALPHA:
		eGLBlendFunction = GL_SRC_ALPHA;
		break;
	case D3D11_BLEND_INV_SRC_ALPHA:
		eGLBlendFunction = GL_ONE_MINUS_SRC_ALPHA;
		break;
	case D3D11_BLEND_DEST_ALPHA:
		eGLBlendFunction = GL_DST_ALPHA;
		break;
	case D3D11_BLEND_INV_DEST_ALPHA:
		eGLBlendFunction = GL_ONE_MINUS_DST_ALPHA;
		break;
	case D3D11_BLEND_DEST_COLOR:
		eGLBlendFunction = GL_DST_COLOR;
		break;
	case D3D11_BLEND_INV_DEST_COLOR:
		eGLBlendFunction = GL_ONE_MINUS_DST_COLOR;
		break;
	case D3D11_BLEND_SRC_ALPHA_SAT:
		eGLBlendFunction = GL_SRC_ALPHA_SATURATE;
		break;
	case D3D11_BLEND_BLEND_FACTOR:
		eGLBlendFunction = GL_CONSTANT_COLOR;
		break;
	case D3D11_BLEND_INV_BLEND_FACTOR:
		eGLBlendFunction = GL_ONE_MINUS_CONSTANT_COLOR;
		break;
#if !DXGLES
	case D3D11_BLEND_SRC1_COLOR:
		eGLBlendFunction = GL_SRC1_COLOR;
		break;
	case D3D11_BLEND_INV_SRC1_COLOR:
		eGLBlendFunction = GL_ONE_MINUS_SRC1_COLOR;
		break;
	case D3D11_BLEND_SRC1_ALPHA:
		eGLBlendFunction = GL_SRC1_ALPHA;
		break;
	case D3D11_BLEND_INV_SRC1_ALPHA:
		eGLBlendFunction = GL_ONE_MINUS_SRC1_ALPHA;
		break;
#endif
	default:
		return false;
	}
	return true;
}

bool InitializeChannelBlendState(
  D3D11_BLEND_OP eBlendOp, D3D11_BLEND eSrcBlend, D3D11_BLEND eDestBlend,
  SChannelBlendState& kChannel)
{
	if (!GetBlendEquation(kChannel.m_eEquation, eBlendOp))
	{
		DXGL_ERROR("Invalid blend operator");
		return false;
	}

	if (!GetBlendFunction(kChannel.m_kFunction.m_eSrc, eSrcBlend) ||
	    !GetBlendFunction(kChannel.m_kFunction.m_eDst, eDestBlend))
	{
		DXGL_ERROR("Invalid blend factor");
		return false;
	}

	return true;
}

bool InitializeBlendState(const D3D11_BLEND_DESC& kDesc, SBlendState& kState, CContext*)
{
#if DXGL_SUPPORT_INDEPENDENT_BLEND_STATES
	static_assert(DXGL_ARRAY_SIZE(kDesc.RenderTarget) == DXGL_ARRAY_SIZE(kState.m_kTargets), "Invalid array size!");
	kState.m_bIndependentBlendEnable = kDesc.IndependentBlendEnable == TRUE;
#else
	if (kDesc.IndependentBlendEnable)
	{
		DXGL_ERROR("Independent blend state is not supported");
		return false;
	}
#endif //DXGL_SUPPORT_INDEPENDENT_BLEND_STATES
	kState.m_bAlphaToCoverageEnable = kDesc.AlphaToCoverageEnable == TRUE;

	uint32 uTarget;
#if DXGL_SUPPORT_INDEPENDENT_BLEND_STATES
	uint32 uTargetEnd(kState.m_bIndependentBlendEnable ? DXGL_ARRAY_SIZE(kDesc.RenderTarget) : 1);
	for (uTarget = 0; uTarget < uTargetEnd; ++uTarget)
#else
	uTarget = 0;
#endif
	{
		const D3D11_RENDER_TARGET_BLEND_DESC& kRTDesc(kDesc.RenderTarget[uTarget]);
		STargetBlendState& kRTState(kState.m_kTargets[uTarget]);

		kRTState.m_kWriteMask.m_abRGBA[0] = (kRTDesc.RenderTargetWriteMask & D3D11_COLOR_WRITE_ENABLE_RED) != 0 ? GL_TRUE : GL_FALSE;
		kRTState.m_kWriteMask.m_abRGBA[1] = (kRTDesc.RenderTargetWriteMask & D3D11_COLOR_WRITE_ENABLE_GREEN) != 0 ? GL_TRUE : GL_FALSE;
		kRTState.m_kWriteMask.m_abRGBA[2] = (kRTDesc.RenderTargetWriteMask & D3D11_COLOR_WRITE_ENABLE_BLUE) != 0 ? GL_TRUE : GL_FALSE;
		kRTState.m_kWriteMask.m_abRGBA[3] = (kRTDesc.RenderTargetWriteMask & D3D11_COLOR_WRITE_ENABLE_ALPHA) != 0 ? GL_TRUE : GL_FALSE;

		kRTState.m_bEnable = kRTDesc.BlendEnable == TRUE;
		if (kRTState.m_bEnable)
		{
			kRTState.m_bSeparateAlpha =
			  (kRTDesc.BlendOp == kRTDesc.BlendOpAlpha) &&
			  (kRTDesc.SrcBlend == kRTDesc.SrcBlendAlpha) &&
			  (kRTDesc.DestBlend == kRTDesc.DestBlendAlpha);

			if (!InitializeChannelBlendState(kRTDesc.BlendOp, kRTDesc.SrcBlend, kRTDesc.DestBlend, kRTState.m_kRGB))
				return false;

			if (kRTState.m_bSeparateAlpha &&
			    !InitializeChannelBlendState(kRTDesc.BlendOpAlpha, kRTDesc.SrcBlendAlpha, kRTDesc.DestBlendAlpha, kRTState.m_kAlpha))
				return false;
		}
	}
	return true;
}

bool InitializeStencilFace(SDepthStencilState::SFace& kStencilFace, const D3D11_DEPTH_STENCILOP_DESC& kDesc, UINT8 uWriteMask, UINT8 uReadMask)
{
	kStencilFace.m_uStencilWriteMask = uWriteMask;
	kStencilFace.m_uStencilReadMask = uReadMask;

	if (!GetComparisonFunc(kStencilFace.m_eFunction, kDesc.StencilFunc))
	{
		DXGL_ERROR("Invalid stencil comparison function");
		return false;
	}

	if (!GetStencilOperation(kStencilFace.m_eStencilFailOperation, kDesc.StencilFailOp) ||
	    !GetStencilOperation(kStencilFace.m_eDepthFailOperation, kDesc.StencilDepthFailOp) ||
	    !GetStencilOperation(kStencilFace.m_eDepthPassOperation, kDesc.StencilPassOp))
	{
		DXGL_ERROR("Invalid stencil operation");
		return false;
	}

	return true;
}

bool InitializeDepthStencilState(const D3D11_DEPTH_STENCIL_DESC& kDesc, SDepthStencilState& kState, CContext*)
{
	kState.m_bDepthTestingEnabled = kDesc.DepthEnable == TRUE;

	switch (kDesc.DepthWriteMask)
	{
	case D3D11_DEPTH_WRITE_MASK_ALL:
		kState.m_bDepthWriteMask = GL_TRUE;
		break;
	case D3D11_DEPTH_WRITE_MASK_ZERO:
		kState.m_bDepthWriteMask = GL_FALSE;
		break;
	default:
		DXGL_ERROR("Invalid depth write mask");
		return false;
	}

	if (kState.m_bDepthTestingEnabled)
	{
		if (!GetComparisonFunc(kState.m_eDepthTestFunc, kDesc.DepthFunc))
		{
			DXGL_ERROR("Invalid depth comparison function");
			return false;
		}
	}

	kState.m_bStencilTestingEnabled = kDesc.StencilEnable == TRUE;

	if (kState.m_bStencilTestingEnabled)
	{
		if (!InitializeStencilFace(kState.m_kStencilFrontFaces, kDesc.FrontFace, kDesc.StencilWriteMask, kDesc.StencilReadMask) ||
		    !InitializeStencilFace(kState.m_kStencilBackFaces, kDesc.BackFace, kDesc.StencilWriteMask, kDesc.StencilReadMask))
			return false;
	}
	return true;
}

bool InitializeRasterizerState(const D3D11_RASTERIZER_DESC& kDesc, SRasterizerState& kState, CContext*)
{
#if !DXGLES
	switch (kDesc.FillMode)
	{
	case D3D11_FILL_SOLID:
		kState.m_ePolygonMode = GL_FILL;
		break;
	case D3D11_FILL_WIREFRAME:
		kState.m_ePolygonMode = GL_LINE;
		break;
	}
#else
	if (kDesc.FillMode != D3D11_FILL_SOLID)
		DXGL_WARNING("Fill modes other than solid are not supported on OpenGL ES. The setting will be ignored");
#endif

	switch (kDesc.CullMode)
	{
	case D3D11_CULL_NONE:
		kState.m_bCullingEnabled = false;
		break;
	case D3D11_CULL_FRONT:
		kState.m_eCullFaceMode = GL_FRONT;
		kState.m_bCullingEnabled = true;
		break;
	case D3D11_CULL_BACK:
		kState.m_eCullFaceMode = GL_BACK;
		kState.m_bCullingEnabled = true;
		break;
	}

#if OGL_FLIP_Y
	kState.m_eFrontFaceMode = kDesc.FrontCounterClockwise ? GL_CW : GL_CCW;
#else
	kState.m_eFrontFaceMode = kDesc.FrontCounterClockwise ? GL_CCW : GL_CW;
#endif

	kState.m_fPolygonOffsetUnits = static_cast<GLfloat>(kDesc.DepthBias); // Same behavior as D3D11 except it's input as float
	kState.m_fPolygonOffsetFactor = kDesc.SlopeScaledDepthBias;
	kState.m_bPolygonOffsetEnabled = kDesc.DepthBias != 0.0f || kDesc.SlopeScaledDepthBias != 0.0f;

	if (kDesc.DepthBiasClamp > 0.0f)
		DXGL_WARNING("Direct3D Depth bias clamping is not supported by OpenGL. This setting will be ignored.");

	kState.m_bScissorEnabled = kDesc.ScissorEnable == TRUE;
	kState.m_bDepthClipEnabled = kDesc.DepthClipEnable == TRUE;
#if !DXGLES
	kState.m_bLineSmoothEnabled = kDesc.AntialiasedLineEnable == TRUE;
	kState.m_bMultisampleEnabled = kDesc.MultisampleEnable == TRUE;
#else
	if (kDesc.AntialiasedLineEnable == TRUE)
		DXGL_WARNING("Smooth line rasterization is not supported on OpenGL ES. This setting will be ignored.");
	if (kDesc.MultisampleEnable == TRUE)
		DXGL_WARNING("Specifying the multisampling mode globally is not supported on OpenGL ES. This setting will be ignored.");
#endif
	return true;
}

bool GetTexFilterParams(GLenum& eMinFilter, GLenum& eMagFilter, bool& bAnisotropic, bool& bComparison, D3D11_FILTER eD3DFilter, bool bMip)
{
	switch (eD3DFilter)
	{
#define _FILTER_CASE(_D3D11FilterID, _MinFilter, _MagFilter, _MipFilter, _Anisotropic, _Comparison) \
  case _D3D11FilterID:                                                                              \
    eMinFilter = bMip ? GL_ ## _MinFilter ## _MIPMAP_ ## _MipFilter : GL_ ## _MinFilter;            \
    eMagFilter = GL_ ## _MagFilter;                                                                 \
    bAnisotropic = _Anisotropic;                                                                    \
    bComparison = _Comparison;                                                                      \
    break;
		_FILTER_CASE(D3D11_FILTER_MIN_MAG_MIP_POINT, NEAREST, NEAREST, NEAREST, false, false);
		_FILTER_CASE(D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR, NEAREST, NEAREST, LINEAR, false, false);
		_FILTER_CASE(D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT, NEAREST, LINEAR, NEAREST, false, false);
		_FILTER_CASE(D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR, NEAREST, LINEAR, LINEAR, false, false);
		_FILTER_CASE(D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT, LINEAR, NEAREST, NEAREST, false, false);
		_FILTER_CASE(D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR, LINEAR, NEAREST, LINEAR, false, false);
		_FILTER_CASE(D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT, LINEAR, LINEAR, NEAREST, false, false);
		_FILTER_CASE(D3D11_FILTER_MIN_MAG_MIP_LINEAR, LINEAR, LINEAR, LINEAR, false, false);
		_FILTER_CASE(D3D11_FILTER_ANISOTROPIC, LINEAR, LINEAR, LINEAR, true, false);
		_FILTER_CASE(D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT, NEAREST, NEAREST, NEAREST, false, true);
		_FILTER_CASE(D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR, NEAREST, NEAREST, LINEAR, false, true);
		_FILTER_CASE(D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT, NEAREST, LINEAR, NEAREST, false, true);
		_FILTER_CASE(D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR, NEAREST, LINEAR, LINEAR, false, true);
		_FILTER_CASE(D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT, LINEAR, NEAREST, NEAREST, false, true);
		_FILTER_CASE(D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR, LINEAR, NEAREST, LINEAR, false, true);
		_FILTER_CASE(D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, LINEAR, LINEAR, NEAREST, false, true);
		_FILTER_CASE(D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, LINEAR, LINEAR, LINEAR, false, true);
		_FILTER_CASE(D3D11_FILTER_COMPARISON_ANISOTROPIC, LINEAR, LINEAR, LINEAR, true, true);
#undef _FILTER_CASE
	default:
		return false;
	}
	return true;
}

bool GetTexWrapParams(GLint& iTexWrap, D3D11_TEXTURE_ADDRESS_MODE eD3DAddressMode)
{
	switch (eD3DAddressMode)
	{
	case D3D11_TEXTURE_ADDRESS_WRAP:
		iTexWrap = GL_REPEAT;
		break;
	case D3D11_TEXTURE_ADDRESS_MIRROR:
		iTexWrap = GL_MIRRORED_REPEAT;
		break;
	case D3D11_TEXTURE_ADDRESS_CLAMP:
		iTexWrap = GL_CLAMP_TO_EDGE;
		break;
	case D3D11_TEXTURE_ADDRESS_BORDER:
		if (!SupportTextureBorderClamp())
		{
			DXGL_WARNING("Sampler with texture border clamping is not supported");
			return false;
		}
		iTexWrap = GL_CLAMP_TO_BORDER_EXT;
		break;
	default:
		DXGL_WARNING("Unsupported sampler address mode");
		return false;
	}
	return true;
}

bool CreateSamplerObject(const D3D11_SAMPLER_DESC& kDesc, GLuint* puSampler, bool bMip, CContext*)
{
	GLenum eComparisonFunc, eMinFilter, eMagFilter;
	bool bAnisotropic, bComparison;
	GLint iWrapS, iWrapT, iWrapR;
	if (!GetTexFilterParams(eMinFilter, eMagFilter, bAnisotropic, bComparison, kDesc.Filter, bMip) ||
	    !GetTexWrapParams(iWrapS, kDesc.AddressU) ||
	    !GetTexWrapParams(iWrapT, kDesc.AddressV) ||
	    !GetTexWrapParams(iWrapR, kDesc.AddressW))
		return false;

	if (bComparison && !GetComparisonFunc(eComparisonFunc, kDesc.ComparisonFunc))
		return false;

	glGenSamplers(1, puSampler);
	if (bComparison)
		glSamplerParameteri(*puSampler, GL_TEXTURE_COMPARE_FUNC, eComparisonFunc);
	glSamplerParameterf(*puSampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, bAnisotropic ? (float)kDesc.MaxAnisotropy : 1.0f);
	glSamplerParameteri(*puSampler, GL_TEXTURE_COMPARE_MODE, bComparison ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE);
	glSamplerParameterf(*puSampler, GL_TEXTURE_MAX_LOD, kDesc.MaxLOD);
	glSamplerParameteri(*puSampler, GL_TEXTURE_MAG_FILTER, eMagFilter);
	glSamplerParameteri(*puSampler, GL_TEXTURE_MIN_FILTER, eMinFilter);
	glSamplerParameterf(*puSampler, GL_TEXTURE_MIN_LOD, kDesc.MinLOD);
	glSamplerParameteri(*puSampler, GL_TEXTURE_WRAP_S, iWrapS);
	glSamplerParameteri(*puSampler, GL_TEXTURE_WRAP_T, iWrapT);
	glSamplerParameteri(*puSampler, GL_TEXTURE_WRAP_R, iWrapR);
	if (SupportTextureBorderClamp())
		glSamplerParameterfv(*puSampler, GL_TEXTURE_BORDER_COLOR_EXT, kDesc.BorderColor);

	if (kDesc.MipLODBias != 0.0f)
	{
#if DXGLES
		DXGL_WARNING("MipLODBias not supported on GL ES");
#else
		glSamplerParameterf(*puSampler, GL_TEXTURE_LOD_BIAS, kDesc.MipLODBias);
#endif
	}

	return true;
}

bool InitializeSamplerState(const D3D11_SAMPLER_DESC& kDesc, SSamplerState& kSamplerState, CContext* pContext)
{
	return
	  CreateSamplerObject(kDesc, &kSamplerState.m_uSamplerObjectMip, true, pContext) &&
	  CreateSamplerObject(kDesc, &kSamplerState.m_uSamplerObjectNoMip, false, pContext);
}

}
