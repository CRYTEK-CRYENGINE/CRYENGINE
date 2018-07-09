// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "PrimitiveRenderPass.h"

class CFullscreenPass : public CPrimitiveRenderPass
{
	// Use SetCustomViewport instead
	using CPrimitiveRenderPass::SetViewport;

public:
	CFullscreenPass(CRenderPrimitive::EPrimitiveFlags primitiveFlags = CRenderPrimitive::eFlags_ReflectShaderConstants);
	~CFullscreenPass();

	bool IsDirty() const override final
	{
		return m_primitive.IsDirty() || CPrimitiveRenderPass::IsDirty();
	}
	using CRenderPassBase::IsDirty;

	void SetPrimitiveFlags(CRenderPrimitive::EPrimitiveFlags flags);

	ILINE void SetPrimitiveType(CRenderPrimitive::EPrimitiveType type)
	{
		m_primitive.SetPrimitiveType(type);
	}

	ILINE void SetTechnique(CShader* pShader, const CCryNameTSCRC& techName, uint64 rtMask)
	{
		m_primitive.SetTechnique(pShader, techName, rtMask);
	}

	ILINE void SetTexture(uint32 slot, CTexture* pTexture, ResourceViewHandle resourceViewID = EDefaultResourceViews::Default)
	{
		m_primitive.SetTexture(slot, pTexture, resourceViewID);
	}

	ILINE void SetSampler(uint32 slot, SamplerStateHandle sampler)
	{
		m_primitive.SetSampler(slot, sampler);
	}

	ILINE void SetTextureSamplerPair(uint32 slot, CTexture* pTex, SamplerStateHandle sampler, ResourceViewHandle resourceViewID = EDefaultResourceViews::Default)
	{
		m_primitive.SetTexture(slot, pTex, resourceViewID);
		m_primitive.SetSampler(slot, sampler);
	}

	ILINE void SetBuffer(uint32 shaderSlot, CGpuBuffer* pBuffer, ResourceViewHandle resourceViewID = EDefaultResourceViews::Default, EShaderStage shaderStages = EShaderStage_Pixel)
	{
		m_primitive.SetBuffer(shaderSlot, pBuffer, resourceViewID, shaderStages);
	}

	ILINE void SetState(int state)
	{
		m_primitive.SetRenderState(state);
	}

	ILINE void SetStencilState(int state, uint8 stencilRef, uint8 stencilReadMask = 0xFF, uint8 stencilWriteMask = 0xFF)
	{
		m_primitive.SetStencilState(state, stencilRef, stencilReadMask, stencilWriteMask);
	}

	void SetRequirePerViewConstantBuffer(bool bRequirePerViewCB)
	{
		m_bRequirePerViewCB = bRequirePerViewCB;
	}

	void SetRequireWorldPos(bool bRequireWPos, f32 clipSpaceZ = 0.0f)
	{
		m_bRequireWorldPos = bRequireWPos;
		m_clipZ = clipSpaceZ;
	}

	void SetConstant(const CCryNameR& paramName, const Vec4 &param, EHWShaderClass shaderClass = eHWSC_Pixel)
	{
		m_primitive.GetConstantManager().SetNamedConstant(paramName, param, shaderClass);
	}

	void SetConstant(const CCryNameR& paramName, const Matrix44 &param, EHWShaderClass shaderClass = eHWSC_Pixel)
	{
		SetConstantArray(paramName, reinterpret_cast<const Vec4*>(param.GetData()), 4, shaderClass);
	}

	void SetConstantArray(const CCryNameR& paramName, const Vec4 params[], uint32 numParams, EHWShaderClass shaderClass = eHWSC_Pixel)
	{
		m_primitive.GetConstantManager().SetNamedConstantArray(paramName, params, numParams, shaderClass);
	}

	void SetInlineConstantBuffer(EConstantBufferShaderSlot shaderSlot, CConstantBuffer* pBuffer, EShaderStage shaderStages = EShaderStage_Pixel)
	{
		m_primitive.SetInlineConstantBuffer(shaderSlot, pBuffer, shaderStages);
	}

	template<typename T>
	void AllocateTypedConstantBuffer(EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages)
	{
		m_primitive.AllocateTypedConstantBuffer(shaderSlot, sizeof(T), shaderStages);
	}

	void BeginConstantUpdate();

	template<typename T>
	SDeviceObjectHelpers::STypedConstants<T> BeginTypedConstantUpdate(EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages = EShaderStage_Pixel) const
	{
		return m_primitive.GetConstantManager().BeginTypedConstantUpdate<T>(shaderSlot, shaderStages);
	}

	template<typename T>
	void EndTypedConstantUpdate(SDeviceObjectHelpers::STypedConstants<T>& constants) const
	{
		return m_primitive.GetConstantManager().EndTypedConstantUpdate<T>(constants);
	}

	bool Execute();

	void SetCustomViewport(const D3DViewPort& viewport)
	{
		SetViewport(viewport);
		m_bHasCustomViewport = true;
	}
	void SetCustomViewport(const SRenderViewport& viewport);
	void ClearCustomViewport()                              { m_bHasCustomViewport = false; }

private:
	void                     UpdatePrimitive();

	bool                     m_bRequirePerViewCB;
	bool                     m_bRequireWorldPos;
	bool                     m_bPendingConstantUpdate;

	bool                     m_bHasCustomViewport = false;

	f32                      m_clipZ;        // only work for WPos
	buffer_handle_t          m_vertexBuffer; // only required for WPos

	CRenderPrimitive         m_primitive;
	CConstantBufferPtr       m_pPerViewConstantBuffer = nullptr;

	// Default flags for the rendering primitive
	CRenderPrimitive::EPrimitiveFlags m_primitiveFlags;
};

ILINE void CFullscreenPass::SetPrimitiveFlags(CRenderPrimitive::EPrimitiveFlags flags)
{
	m_primitiveFlags = flags;

	auto primitiveFlags = CRenderPrimitive::EPrimitiveFlags(flags & CRenderPrimitive::eFlags_ReflectShaderConstants);
	m_primitive.SetFlags(primitiveFlags);
}