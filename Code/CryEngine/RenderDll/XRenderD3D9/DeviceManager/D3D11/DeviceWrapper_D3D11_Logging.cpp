// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"

#ifdef DO_RENDERLOG

	#define ecase(e) case e: \
	  return # e

static char sLogStr[1024];
	#define edefault(e) default: \
	  cry_sprintf(sLogStr, "0x%x", e); return sLogStr;

static char* sD3DFMT(DXGI_FORMAT Value)
{
	switch (Value)
	{
		ecase(DXGI_FORMAT_R32G32B32A32_TYPELESS);
		ecase(DXGI_FORMAT_R32G32B32A32_FLOAT);
		ecase(DXGI_FORMAT_R32G32B32A32_UINT);
		ecase(DXGI_FORMAT_R32G32B32A32_SINT);
		ecase(DXGI_FORMAT_R32G32B32_TYPELESS);
		ecase(DXGI_FORMAT_R32G32B32_FLOAT);
		ecase(DXGI_FORMAT_R32G32B32_UINT);
		ecase(DXGI_FORMAT_R32G32B32_SINT);
		ecase(DXGI_FORMAT_R16G16B16A16_TYPELESS);
		ecase(DXGI_FORMAT_R16G16B16A16_FLOAT);
		ecase(DXGI_FORMAT_R16G16B16A16_UNORM);
		ecase(DXGI_FORMAT_R16G16B16A16_UINT);
		ecase(DXGI_FORMAT_R16G16B16A16_SNORM);
		ecase(DXGI_FORMAT_R16G16B16A16_SINT);
		ecase(DXGI_FORMAT_R32G32_TYPELESS);
		ecase(DXGI_FORMAT_R32G32_FLOAT);
		ecase(DXGI_FORMAT_R32G32_UINT);
		ecase(DXGI_FORMAT_R32G32_SINT);
		ecase(DXGI_FORMAT_R32G8X24_TYPELESS);
		ecase(DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
		ecase(DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS);
		ecase(DXGI_FORMAT_X32_TYPELESS_G8X24_UINT);
		ecase(DXGI_FORMAT_R10G10B10A2_TYPELESS);
		ecase(DXGI_FORMAT_R10G10B10A2_UNORM);
		ecase(DXGI_FORMAT_R10G10B10A2_UINT);
		ecase(DXGI_FORMAT_R11G11B10_FLOAT);
		ecase(DXGI_FORMAT_R8G8B8A8_TYPELESS);
		ecase(DXGI_FORMAT_R8G8B8A8_UNORM);
		ecase(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
		ecase(DXGI_FORMAT_R8G8B8A8_UINT);
		ecase(DXGI_FORMAT_R8G8B8A8_SNORM);
		ecase(DXGI_FORMAT_R8G8B8A8_SINT);
		ecase(DXGI_FORMAT_R16G16_TYPELESS);
		ecase(DXGI_FORMAT_R16G16_FLOAT);
		ecase(DXGI_FORMAT_R16G16_UNORM);
		ecase(DXGI_FORMAT_R16G16_UINT);
		ecase(DXGI_FORMAT_R16G16_SNORM);
		ecase(DXGI_FORMAT_R16G16_SINT);
		ecase(DXGI_FORMAT_R32_TYPELESS);
		ecase(DXGI_FORMAT_D32_FLOAT);
		ecase(DXGI_FORMAT_R32_FLOAT);
		ecase(DXGI_FORMAT_R32_UINT);
		ecase(DXGI_FORMAT_R32_SINT);
		ecase(DXGI_FORMAT_R24G8_TYPELESS);
		ecase(DXGI_FORMAT_D24_UNORM_S8_UINT);
		ecase(DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
		ecase(DXGI_FORMAT_X24_TYPELESS_G8_UINT);
		ecase(DXGI_FORMAT_R8G8_TYPELESS);
		ecase(DXGI_FORMAT_R8G8_UNORM);
		ecase(DXGI_FORMAT_R8G8_UINT);
		ecase(DXGI_FORMAT_R8G8_SNORM);
		ecase(DXGI_FORMAT_R8G8_SINT);
		ecase(DXGI_FORMAT_R16_TYPELESS);
		ecase(DXGI_FORMAT_R16_FLOAT);
		ecase(DXGI_FORMAT_D16_UNORM);
		ecase(DXGI_FORMAT_R16_UNORM);
		ecase(DXGI_FORMAT_R16_UINT);
		ecase(DXGI_FORMAT_R16_SNORM);
		ecase(DXGI_FORMAT_R16_SINT);
		ecase(DXGI_FORMAT_R8_TYPELESS);
		ecase(DXGI_FORMAT_R8_UNORM);
		ecase(DXGI_FORMAT_R8_UINT);
		ecase(DXGI_FORMAT_R8_SNORM);
		ecase(DXGI_FORMAT_R8_SINT);
		ecase(DXGI_FORMAT_A8_UNORM);
		ecase(DXGI_FORMAT_R1_UNORM);
		ecase(DXGI_FORMAT_R9G9B9E5_SHAREDEXP);
		ecase(DXGI_FORMAT_R8G8_B8G8_UNORM);
		ecase(DXGI_FORMAT_G8R8_G8B8_UNORM);
		ecase(DXGI_FORMAT_BC1_TYPELESS);
		ecase(DXGI_FORMAT_BC1_UNORM);
		ecase(DXGI_FORMAT_BC1_UNORM_SRGB);
		ecase(DXGI_FORMAT_BC2_TYPELESS);
		ecase(DXGI_FORMAT_BC2_UNORM);
		ecase(DXGI_FORMAT_BC2_UNORM_SRGB);
		ecase(DXGI_FORMAT_BC3_TYPELESS);
		ecase(DXGI_FORMAT_BC3_UNORM);
		ecase(DXGI_FORMAT_BC3_UNORM_SRGB);
		ecase(DXGI_FORMAT_BC4_TYPELESS);
		ecase(DXGI_FORMAT_BC4_UNORM);
		ecase(DXGI_FORMAT_BC4_SNORM);
		ecase(DXGI_FORMAT_BC5_TYPELESS);
		ecase(DXGI_FORMAT_BC5_UNORM);
		ecase(DXGI_FORMAT_BC5_SNORM);
		ecase(DXGI_FORMAT_BC6H_TYPELESS);
		ecase(DXGI_FORMAT_BC6H_SF16);
		ecase(DXGI_FORMAT_BC6H_UF16);
		ecase(DXGI_FORMAT_BC7_TYPELESS);
		ecase(DXGI_FORMAT_BC7_UNORM);
		ecase(DXGI_FORMAT_BC7_UNORM_SRGB);
		ecase(DXGI_FORMAT_B5G6R5_UNORM);
		ecase(DXGI_FORMAT_B5G5R5A1_UNORM);
		ecase(DXGI_FORMAT_B8G8R8A8_UNORM);
		ecase(DXGI_FORMAT_B8G8R8X8_UNORM);
	#if (CRY_RENDERER_OPENGL >= 430)
		ecase(DXGI_FORMAT_EAC_R11_TYPELESS);
		ecase(DXGI_FORMAT_EAC_R11_UNORM);
		ecase(DXGI_FORMAT_EAC_R11_SNORM);
		ecase(DXGI_FORMAT_EAC_RG11_TYPELESS);
		ecase(DXGI_FORMAT_EAC_RG11_UNORM);
		ecase(DXGI_FORMAT_EAC_RG11_SNORM);
		ecase(DXGI_FORMAT_ETC2_TYPELESS);
		ecase(DXGI_FORMAT_ETC2_UNORM);
		ecase(DXGI_FORMAT_ETC2_UNORM_SRGB);
		ecase(DXGI_FORMAT_ETC2A_TYPELESS);
		ecase(DXGI_FORMAT_ETC2A_UNORM);
		ecase(DXGI_FORMAT_ETC2A_UNORM_SRGB);
	#endif //CRY_RENDERER_OPENGL
		edefault(Value);
	}
}

static char* sD3DPRIM_TOPLOGY(D3D11_PRIMITIVE_TOPOLOGY Topology)
{
	switch (Topology)
	{
		ecase(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		ecase(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		ecase(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
		ecase(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ecase(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		ecase(D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ);
		ecase(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ);
		ecase(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ);
		ecase(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ);
		edefault(Topology);
	}
}

static char* sD3DBlendOP(DWORD Value)
{
	switch (Value)
	{
		ecase(D3D11_BLEND_OP_ADD);
		ecase(D3D11_BLEND_OP_SUBTRACT);
		ecase(D3D11_BLEND_OP_REV_SUBTRACT);
		ecase(D3D11_BLEND_OP_MIN);
		ecase(D3D11_BLEND_OP_MAX);
		edefault((uint32)Value);
	}
}

static char* sD3DBLEND(DWORD Value)
{
	switch (Value)
	{
		ecase(D3D11_BLEND_ZERO);
		ecase(D3D11_BLEND_ONE);
		ecase(D3D11_BLEND_SRC_COLOR);
		ecase(D3D11_BLEND_DEST_COLOR);
		ecase(D3D11_BLEND_SRC_ALPHA);
		ecase(D3D11_BLEND_SRC_ALPHA_SAT);
		ecase(D3D11_BLEND_DEST_ALPHA);
		ecase(D3D11_BLEND_INV_SRC_COLOR);
		ecase(D3D11_BLEND_INV_SRC_ALPHA);
		ecase(D3D11_BLEND_INV_DEST_ALPHA);
		ecase(D3D11_BLEND_INV_DEST_COLOR);
		ecase(D3D11_BLEND_BLEND_FACTOR);
		ecase(D3D11_BLEND_INV_BLEND_FACTOR);
		ecase(D3D11_BLEND_SRC1_COLOR);
		ecase(D3D11_BLEND_INV_SRC1_COLOR);
		ecase(D3D11_BLEND_SRC1_ALPHA);
		ecase(D3D11_BLEND_INV_SRC1_ALPHA);
		edefault((uint32)Value);
	}
}

static char* sD3DCompareFunc(DWORD Value)
{
	switch (Value)
	{
		ecase(D3D11_COMPARISON_NEVER);
		ecase(D3D11_COMPARISON_LESS);
		ecase(D3D11_COMPARISON_EQUAL);
		ecase(D3D11_COMPARISON_LESS_EQUAL);
		ecase(D3D11_COMPARISON_GREATER);
		ecase(D3D11_COMPARISON_NOT_EQUAL);
		ecase(D3D11_COMPARISON_GREATER_EQUAL);
		ecase(D3D11_COMPARISON_ALWAYS);
		edefault((uint32)Value);
	}
}

static char* sD3DStencilOp(DWORD Value)
{
	switch (Value)
	{
		ecase(D3D11_STENCIL_OP_KEEP);
		ecase(D3D11_STENCIL_OP_ZERO);
		ecase(D3D11_STENCIL_OP_REPLACE);
		ecase(D3D11_STENCIL_OP_INCR_SAT);
		ecase(D3D11_STENCIL_OP_DECR_SAT);
		ecase(D3D11_STENCIL_OP_INVERT);
		ecase(D3D11_STENCIL_OP_INCR);
		ecase(D3D11_STENCIL_OP_DECR);
		edefault((uint32)Value);
	}
}

static char* sD3DCull(DWORD Value)
{
	switch (Value)
	{
		ecase(D3D11_CULL_BACK);
		ecase(D3D11_CULL_FRONT);
		ecase(D3D11_CULL_NONE);
		edefault((uint32)Value);
	}
}

static char* sD3DTAddress(D3D11_TEXTURE_ADDRESS_MODE Value)
{
	switch (Value)
	{
		ecase(D3D11_TEXTURE_ADDRESS_CLAMP);
		ecase(D3D11_TEXTURE_ADDRESS_WRAP);
		ecase(D3D11_TEXTURE_ADDRESS_MIRROR);
		ecase(D3D11_TEXTURE_ADDRESS_BORDER);
		ecase(D3D11_TEXTURE_ADDRESS_MIRROR_ONCE);
		edefault(Value);
	}
}

static char* sD3DTFilter(D3D11_FILTER Value)
{
	switch (Value)
	{
		ecase(D3D11_FILTER_MIN_MAG_MIP_POINT);
		ecase(D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR);
		ecase(D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT);
		ecase(D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR);
		ecase(D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT);
		ecase(D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR);
		ecase(D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT);
		ecase(D3D11_FILTER_MIN_MAG_MIP_LINEAR);
		ecase(D3D11_FILTER_ANISOTROPIC);
		ecase(D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT);
		ecase(D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR);
		ecase(D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT);
		ecase(D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR);
		ecase(D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT);
		ecase(D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR);
		ecase(D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT);
		ecase(D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR);
		ecase(D3D11_FILTER_COMPARISON_ANISOTROPIC);
		edefault(Value);
	}
}

///////////////////////////////////////////////////////////////////////////////
struct CCryDeviceLoggingHook : public ICryDeviceWrapperHook
{
public:
	CCryDeviceLoggingHook() {}
	virtual const char* Name() const { return GetName(); }

	static const char*  GetName()    { return "CCryDeviceLoggingHook"; }

	/*** ID3DDevice methods ***/
	virtual void QueryInterface_Device_PreCallHook(REFIID riid, void** ppvObj);
	virtual void AddRef_Device_PreCallHook();
	virtual void Release_Device_PreCallHook();

	virtual void GetDeviceRemovedReason_PreCallHook();

	virtual void SetExceptionMode_PreCallHook(UINT RaiseFlags);
	virtual void GetExceptionMode_PreCallHook();

	virtual void GetPrivateData_Device_PreCallHook(REFGUID guid, UINT* pDataSize, void* pData);
	virtual void SetPrivateData_Device_PreCallHook(REFGUID guid, UINT DataSize, const void* pData);
	virtual void SetPrivateDataInterface_Device_PreCallHook(REFGUID guid, const IUnknown* pData);

	virtual void CreateBuffer_PreCallHook(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer);

	virtual void CreateTexture1D_PreCallHook(const D3D11_TEXTURE1D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D);
	virtual void CreateTexture2D_PreCallHook(const D3D11_TEXTURE2D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D);
	virtual void CreateTexture3D_PreCallHook(const D3D11_TEXTURE3D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D);

	virtual void CreateShaderResourceView_PreCallHook(ID3D11Resource* pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc, ID3D11ShaderResourceView** ppSRView);
	virtual void CreateRenderTargetView_PreCallHook(ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc, ID3D11RenderTargetView** ppRTView);
	virtual void CreateDepthStencilView_PreCallHook(ID3D11Resource* pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc, ID3D11DepthStencilView** ppDepthStencilView);
	virtual void CreateUnorderedAccessView_PreCallHook(ID3D11Resource* pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc, ID3D11UnorderedAccessView** ppUAView);

	virtual void CreateInputLayout_PreCallHook(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements, const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout** ppInputLayout);

	virtual void CreateVertexShader_PreCallHook(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader);
	virtual void CreateGeometryShader_PreCallHook(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader);
	virtual void CreateGeometryShaderWithStreamOutput_PreCallHook(const void* pShaderBytecode, SIZE_T BytecodeLength, const D3D11_SO_DECLARATION_ENTRY* pSODeclaration, UINT NumEntries, const UINT* pBufferStrides, UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader);
	virtual void CreatePixelShader_PreCallHook(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader);
	virtual void CreateHullShader_PreCallHook(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11HullShader** ppHullShader);
	virtual void CreateDomainShader_PreCallHook(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11DomainShader** ppDomainShader);
	virtual void CreateComputeShader_PreCallHook(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11ComputeShader** ppComputeShader);

	virtual void CreateBlendState_PreCallHook(const D3D11_BLEND_DESC* pBlendStateDesc, ID3D11BlendState** ppBlendState);
	virtual void CreateDepthStencilState_PreCallHook(const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc, ID3D11DepthStencilState** ppDepthStencilState);
	virtual void CreateRasterizerState_PreCallHook(const D3D11_RASTERIZER_DESC* pRasterizerDesc, ID3D11RasterizerState** ppRasterizerState);
	virtual void CreateSamplerState_PreCallHook(const D3D11_SAMPLER_DESC* pSamplerDesc, ID3D11SamplerState** ppSamplerState);

	virtual void CreateQuery_PreCallHook(const D3D11_QUERY_DESC* pQueryDesc, ID3D11Query** ppQuery);
	virtual void CreatePredicate_PreCallHook(const D3D11_QUERY_DESC* pPredicateDesc, ID3D11Predicate** ppPredicate);

	virtual void CreateClassLinkage_PreCallHook(ID3D11ClassLinkage** ppLinkage);

	virtual void GetCreationFlags_PreCallHook();
	virtual void GetImmediateContext_PreCallHook(ID3D11DeviceContext** ppImmediateContext);
	virtual void CreateDeferredContext_PreCallHook(UINT ContextFlags, ID3D11DeviceContext** ppDeferredContext);

	virtual void GetFeatureLevel_PreCallHook();
	virtual void CheckFeatureSupport_PreCallHook(D3D11_FEATURE Feature, void* pFeatureSupportData, UINT FeatureSupportDataSize);
	virtual void CheckFormatSupport_PreCallHook(DXGI_FORMAT Format, UINT* pFormatSupport);
	virtual void CheckMultisampleQualityLevels_PreCallHook(DXGI_FORMAT Format, UINT SampleCount, UINT* pNumQualityLevels);

	virtual void CheckCounterInfo_PreCallHook(D3D11_COUNTER_INFO* pCounterInfo);
	virtual void CreateCounter_PreCallHook(const D3D11_COUNTER_DESC* pCounterDesc, ID3D11Counter** ppCounter);
	virtual void CheckCounter_PreCallHook(const D3D11_COUNTER_DESC* pDesc, D3D11_COUNTER_TYPE* pType, UINT* pActiveCounters, LPSTR wszName, UINT* pNameLength, LPSTR wszUnits, UINT* pUnitsLength, LPSTR wszDescription, UINT* pDescriptionLength);

	virtual void OpenSharedResource_PreCallHook(HANDLE hResource, REFIID ReturnedInterface, void** ppResource);

	/*************************************/
	/*** ID3DDeviceContext methods ***/
	virtual void QueryInterface_Context_PreCallHook(REFIID riid, void** ppvObj);
	virtual void AddRef_Context_PreCallHook();
	virtual void Release_Context_PreCallHook();

	void         GetDevice_PreCallHook(ID3D11Device** ppDevice);

	virtual void GetPrivateData_Context_PreCallHook(REFGUID guid, UINT* pDataSize, void* pData);
	virtual void SetPrivateData_Context_PreCallHook(REFGUID guid, UINT DataSize, const void* pData);
	virtual void SetPrivateDataInterface_Context_PreCallHook(REFGUID guid, const IUnknown* pData);

	virtual void PSSetShader_PreCallHook(ID3D11PixelShader* pPixelShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	virtual void PSGetShader_PreCallHook(ID3D11PixelShader** ppPixelShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	virtual void PSSetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	virtual void PSGetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	virtual void PSSetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	virtual void PSGetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	virtual void PSSetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	virtual void PSGetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);

	virtual void VSSetShader_PreCallHook(ID3D11VertexShader* pVertexShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	virtual void VSGetShader_PreCallHook(ID3D11VertexShader** ppVertexShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	virtual void VSSetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	virtual void VSGetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	virtual void VSSetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	virtual void VSGetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	virtual void VSSetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	virtual void VSGetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);

	virtual void GSSetShader_PreCallHook(ID3D11GeometryShader* pShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	virtual void GSGetShader_PreCallHook(ID3D11GeometryShader** ppGeometryShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	virtual void GSSetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	virtual void GSGetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	virtual void GSSetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	virtual void GSGetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	virtual void GSSetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	virtual void GSGetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);

	virtual void HSSetShader_PreCallHook(ID3D11HullShader* pHullShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	virtual void HSGetShader_PreCallHook(ID3D11HullShader** ppHullShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	virtual void HSSetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	virtual void HSGetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	virtual void HSSetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	virtual void HSGetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	virtual void HSSetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	virtual void HSGetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);

	virtual void DSSetShader_PreCallHook(ID3D11DomainShader* pDomainShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	virtual void DSGetShader_PreCallHook(ID3D11DomainShader** ppDomainShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	virtual void DSSetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	virtual void DSGetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	virtual void DSSetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	virtual void DSGetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	virtual void DSSetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	virtual void DSGetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);

	virtual void CSSetShader_PreCallHook(ID3D11ComputeShader* pComputeShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	virtual void CSGetShader_PreCallHook(ID3D11ComputeShader** ppComputeShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	virtual void CSSetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	virtual void CSGetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	virtual void CSSetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	virtual void CSGetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	virtual void CSSetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	virtual void CSGetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);
	virtual void CSSetUnorderedAccessViews_PreCallHook(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts);
	virtual void CSGetUnorderedAccessViews_PreCallHook(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews);

	virtual void DrawAuto_PreCallHook();
	virtual void Draw_PreCallHook(UINT VertexCount, UINT StartVertexLocation);
	virtual void DrawInstanced_PreCallHook(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation);
	virtual void DrawInstancedIndirect_PreCallHook(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs);
	virtual void DrawIndexed_PreCallHook(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
	virtual void DrawIndexedInstanced_PreCallHook(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation);
	virtual void DrawIndexedInstancedIndirect_PreCallHook(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs);

	virtual void Map_PreCallHook(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource);
	virtual void Unmap_PreCallHook(ID3D11Resource* pResource, UINT Subresource);

	virtual void IASetInputLayout_PreCallHook(ID3D11InputLayout* pInputLayout);
	virtual void IAGetInputLayout_PreCallHook(ID3D11InputLayout** ppInputLayout);
	virtual void IASetVertexBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const UINT* pStrides, const UINT* pOffsets);
	virtual void IAGetVertexBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppVertexBuffers, UINT* pStrides, UINT* pOffsets);
	virtual void IASetIndexBuffer_PreCallHook(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, UINT Offset);
	virtual void IAGetIndexBuffer_PreCallHook(ID3D11Buffer** pIndexBuffer, DXGI_FORMAT* Format, UINT* Offset);
	virtual void IASetPrimitiveTopology_PreCallHook(D3D11_PRIMITIVE_TOPOLOGY Topology);
	virtual void IAGetPrimitiveTopology_PreCallHook(D3D11_PRIMITIVE_TOPOLOGY* pTopology);

	virtual void Begin_PreCallHook(ID3D11Asynchronous* pAsync);
	virtual void End_PreCallHook(ID3D11Asynchronous* pAsync);

	virtual void GetData_PreCallHook(ID3D11Asynchronous* pAsync, void* pData, UINT DataSize, UINT GetDataFlags);
	virtual void SetPredication_PreCallHook(ID3D11Predicate* pPredicate, BOOL PredicateValue);
	virtual void GetPredication_PreCallHook(ID3D11Predicate** ppPredicate, BOOL* pPredicateValue);

	virtual void OMSetRenderTargets_PreCallHook(UINT NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView);
	virtual void OMGetRenderTargets_PreCallHook(UINT NumViews, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView);
	virtual void OMSetRenderTargetsAndUnorderedAccessViews_PreCallHook(UINT NumRTVs, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts);
	virtual void OMGetRenderTargetsAndUnorderedAccessViews_PreCallHook(UINT NumRTVs, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews);
	virtual void OMSetBlendState_PreCallHook(ID3D11BlendState* pBlendState, const FLOAT BlendFactor[4], UINT SampleMask);
	virtual void OMGetBlendState_PreCallHook(ID3D11BlendState** ppBlendState, FLOAT BlendFactor[4], UINT* pSampleMask);
	virtual void OMSetDepthStencilState_PreCallHook(ID3D11DepthStencilState* pDepthStencilState, UINT StencilRef);
	virtual void OMGetDepthStencilState_PreCallHook(ID3D11DepthStencilState** ppDepthStencilState, UINT* pStencilRef);

	virtual void SOSetTargets_PreCallHook(UINT NumBuffers, ID3D11Buffer* const* ppSOTargets, const UINT* pOffsets);
	virtual void SOGetTargets_PreCallHook(UINT NumBuffers, ID3D11Buffer** ppSOTargets);

	virtual void Dispatch_PreCallHook(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ);
	virtual void DispatchIndirect_PreCallHook(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs);

	virtual void RSSetState_PreCallHook(ID3D11RasterizerState* pRasterizerState);
	virtual void RSGetState_PreCallHook(ID3D11RasterizerState** ppRasterizerState);
	virtual void RSSetViewports_PreCallHook(UINT NumViewports, const D3D11_VIEWPORT* pViewports);
	virtual void RSGetViewports_PreCallHook(/*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/ UINT* pNumViewports, D3D11_VIEWPORT* pViewports);
	virtual void RSSetScissorRects_PreCallHook(UINT NumRects, const D3D11_RECT* pRects);
	virtual void RSGetScissorRects_PreCallHook(/*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/ UINT* pNumRects, D3D11_RECT* pRects);

	virtual void CopyResource_PreCallHook(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource);
	virtual void CopySubresourceRegion_PreCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox);
	virtual void UpdateSubresource_PreCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);

	virtual void CopyStructureCount_PreCallHook(ID3D11Buffer* pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView* pSrcView);

	virtual void ClearRenderTargetView_PreCallHook(ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4]);
	virtual void ClearUnorderedAccessViewUint_PreCallHook(ID3D11UnorderedAccessView* pUnorderedAccessView, const UINT Values[4]);
	virtual void ClearUnorderedAccessViewFloat_PreCallHook(ID3D11UnorderedAccessView* pUnorderedAccessView, const FLOAT Values[4]);
	virtual void ClearDepthStencilView_PreCallHook(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil);

	virtual void GenerateMips_PreCallHook(ID3D11ShaderResourceView* pShaderResourceView);

	virtual void SetResourceMinLOD_PreCallHook(ID3D11Resource* pResource, FLOAT MinLOD);
	virtual void GetResourceMinLOD_PreCallHook(ID3D11Resource* pResource);

	virtual void ResolveSubresource_PreCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, ID3D11Resource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format);

	#if !CRY_PLATFORM_ORBIS
	virtual void ExecuteCommandList_PreCallHook(ID3D11CommandList* pCommandList, BOOL RestoreContextState);
	virtual void FinishCommandList_PreCallHook(BOOL RestoreDeferredContextState, ID3D11CommandList** ppCommandList);
	#endif

	virtual void ClearState_PreCallHook();
	virtual void Flush_PreCallHook();

	virtual void GetType_PreCallHook();
	virtual void GetContextFlags_PreCallHook();
};

void CCryDeviceLoggingHook::QueryInterface_Device_PreCallHook(REFIID riid, void** ppvObj)
{
	gRenDev->Logv("%s\n", "D3DDevice::QueryInterface");
}

void CCryDeviceLoggingHook::AddRef_Device_PreCallHook()
{
	gRenDev->Logv("%s\n", "D3DDevice::AddRef");
}
void CCryDeviceLoggingHook::Release_Device_PreCallHook()
{
	gRenDev->Logv("%s\n", "D3DDevice::Release");
}

void CCryDeviceLoggingHook::QueryInterface_Context_PreCallHook(REFIID riid, void** ppvObj)
{
	gRenDev->Logv("%s\n", "D3DDeviceContext::QueryInterface");
}
void CCryDeviceLoggingHook::AddRef_Context_PreCallHook()
{
	gRenDev->Logv("%s\n", "D3DDeviceContext::AddRef");
}
void CCryDeviceLoggingHook::Release_Context_PreCallHook()
{
	gRenDev->Logv("%s\n", "D3DDeviceContext::Release");
}

void CCryDeviceLoggingHook::VSSetConstantBuffers_PreCallHook(
  /* [in] */ UINT StartConstantSlot,
  /* [in] */ UINT NumBuffers,
  /* [size_is][in] */ ID3D11Buffer* const* ppConstantBuffers)
{
	if (ppConstantBuffers && *ppConstantBuffers)
	{
		int nBytes = 0;
		D3D11_BUFFER_DESC Desc;
		(*ppConstantBuffers)->GetDesc(&Desc);
		nBytes = Desc.ByteWidth;
		gRenDev->Logv("%s(%d, %d, 0x%x) (%d bytes)\n", "D3DDeviceContext::VSSetConstantBuffers", StartConstantSlot, NumBuffers, *ppConstantBuffers, nBytes);
	}
}

void CCryDeviceLoggingHook::PSSetShaderResources_PreCallHook(
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumViews,
  /* [size_is][in] */ ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::PSSetShaderResources", Offset, NumViews, *ppShaderResourceViews);
}

void CCryDeviceLoggingHook::PSSetShader_PreCallHook(
  /* [in] */ ID3D11PixelShader* pPixelShader,
  /* [in] */ ID3D11ClassInstance* const* ppClassInstances,
  /* [in] */ UINT NumClassInstances)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDeviceContext::PSSetShader", pPixelShader);
}

void CCryDeviceLoggingHook::PSSetSamplers_PreCallHook(
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumSamplers,
  /* [size_is][in] */ ID3D11SamplerState* const* ppSamplers
  )
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::PSSetSamplers", Offset, NumSamplers, *ppSamplers);
#if !CRY_RENDERER_VULKAN
	D3D11_SAMPLER_DESC Desc;
	if (ppSamplers && *ppSamplers)
	{
		(*ppSamplers)->GetDesc(&Desc);
		gRenDev->Logv("  AddressU: %s\n", sD3DTAddress(Desc.AddressU));
		gRenDev->Logv("  AddressV: %s\n", sD3DTAddress(Desc.AddressV));
		gRenDev->Logv("  AddressW: %s\n", sD3DTAddress(Desc.AddressW));
		gRenDev->Logv("  Filter: %s\n", sD3DTFilter(Desc.Filter));
		if (Desc.Filter == D3D11_FILTER_ANISOTROPIC || Desc.Filter == D3D11_FILTER_COMPARISON_ANISOTROPIC)
			gRenDev->Logv("  MaxAnisotropy: %d\n", Desc.MaxAnisotropy);
	}
#endif
}

void CCryDeviceLoggingHook::VSSetShader_PreCallHook(
  /* [in] */ ID3D11VertexShader* pVertexShader,
  /* [in] */ ID3D11ClassInstance* const* ppClassInstances,
  /* [in] */ UINT NumClassInstances)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDeviceContext::VSSetShader", pVertexShader);
}

void CCryDeviceLoggingHook::DrawIndexed_PreCallHook(
  /* [in] */ UINT IndexCount,
  /* [in] */ UINT StartIndexLocation,
  /* [in] */ INT BaseVertexLocation)
{
	gRenDev->Logv("%s(%d, %d, %d)\n", "D3DDeviceContext::DrawIndexed", IndexCount, StartIndexLocation, BaseVertexLocation);
}

void CCryDeviceLoggingHook::Draw_PreCallHook(
  /* [in] */ UINT VertexCount,
  /* [in] */ UINT StartVertexLocation)
{
	gRenDev->Logv("%s(%d, %d)\n", "D3DDeviceContext::Draw", VertexCount, StartVertexLocation);
}

void CCryDeviceLoggingHook::PSSetConstantBuffers_PreCallHook(
  /* [in] */ UINT StartConstantSlot,
  /* [in] */ UINT NumBuffers,
  /* [size_is][in] */ ID3D11Buffer* const* ppConstantBuffers)
{
	if (ppConstantBuffers && *ppConstantBuffers)
	{
		int nBytes = 0;
		D3D11_BUFFER_DESC Desc;
		(*ppConstantBuffers)->GetDesc(&Desc);
		nBytes = Desc.ByteWidth;
		gRenDev->Logv("%s(%d, %d, 0x%x) (%d bytes)\n", "D3DDeviceContext::PSSetConstantBuffers", StartConstantSlot, NumBuffers, *ppConstantBuffers, nBytes);
	}
}

void CCryDeviceLoggingHook::IASetInputLayout_PreCallHook(
  /* [in] */ ID3D11InputLayout* pInputLayout)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDeviceContext::IASetInputLayout", pInputLayout);
}

void CCryDeviceLoggingHook::IASetVertexBuffers_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumBuffers,
  /* [size_is][in] */ ID3D11Buffer* const* ppVertexBuffers,
  /* [size_is][in] */ const UINT* pStrides,
  /* [size_is][in] */ const UINT* pOffsets)
{
	gRenDev->Logv("%s(%d, %d, 0x%x, %d, %d)\n", "D3DDeviceContext::IASetVertexBuffers", StartSlot, NumBuffers, *ppVertexBuffers, *pStrides, *pOffsets);
}

void CCryDeviceLoggingHook::IASetIndexBuffer_PreCallHook(
  /* [in] */ ID3D11Buffer* pIndexBuffer,
  /* [in] */ DXGI_FORMAT Format,
  /* [in] */ UINT Offset)
{
	gRenDev->Logv("%s(0x%x, %s, %d)\n", "D3DDeviceContext::IASetIndexBuffer", pIndexBuffer, sD3DFMT(Format), Offset);
}

void CCryDeviceLoggingHook::DrawIndexedInstanced_PreCallHook(
  /* [in] */ UINT IndexCountPerInstance,
  /* [in] */ UINT InstanceCount,
  /* [in] */ UINT StartIndexLocation,
  /* [in] */ INT BaseVertexLocation,
  /* [in] */ UINT StartInstanceLocation)
{
	gRenDev->Logv("%s(%d, %d, %d, %d, %d)\n", "D3DDeviceContext::DrawIndexedInstanced", IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

void CCryDeviceLoggingHook::DrawInstanced_PreCallHook(
  /* [in] */ UINT VertexCountPerInstance,
  /* [in] */ UINT InstanceCount,
  /* [in] */ UINT StartVertexLocation,
  /* [in] */ UINT StartInstanceLocation)
{
	gRenDev->Logv("%s(%d, %d, %d, %d)\n", "D3DDeviceContext::DrawInstanced", VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

void CCryDeviceLoggingHook::GSSetConstantBuffers_PreCallHook(
  /* [in] */ UINT StartConstantSlot,
  /* [in] */ UINT NumBuffers,
  /* [size_is][in] */ ID3D11Buffer* const* ppConstantBuffers)
{
	if (ppConstantBuffers && *ppConstantBuffers)
	{
		int nBytes = 0;
		D3D11_BUFFER_DESC Desc;
		(*ppConstantBuffers)->GetDesc(&Desc);
		nBytes = Desc.ByteWidth;
		gRenDev->Logv("%s(%d, %d, 0x%x) (%d bytes)\n", "D3DDeviceContext::GSSetConstantBuffers", StartConstantSlot, NumBuffers, *ppConstantBuffers, nBytes);
	}
}

void CCryDeviceLoggingHook::GSSetShader_PreCallHook(
  /* [in] */ ID3D11GeometryShader* pShader,
  /* [in] */ ID3D11ClassInstance* const* ppClassInstances,
  /* [in] */ UINT NumClassInstances)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDeviceContext::GSSetShader", pShader);
}

void CCryDeviceLoggingHook::IASetPrimitiveTopology_PreCallHook(
  /* [in] */ D3D11_PRIMITIVE_TOPOLOGY Topology)
{
	gRenDev->Logv("%s(%s)\n", "D3DDeviceContext::IASetPrimitiveTopology", sD3DPRIM_TOPLOGY(Topology));
}

void CCryDeviceLoggingHook::VSSetShaderResources_PreCallHook(
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumViews,
  /* [size_is][in] */ ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::VSSetShaderResources", Offset, NumViews, *ppShaderResourceViews);
}

void CCryDeviceLoggingHook::VSSetSamplers_PreCallHook(
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumSamplers,
  /* [size_is][in] */ ID3D11SamplerState* const* ppSamplers)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::VSSetSamplers", Offset, NumSamplers, *ppSamplers);
}

void CCryDeviceLoggingHook::SetPredication_PreCallHook(
  /* [in] */ ID3D11Predicate* pPredicate,
  /* [in] */ BOOL PredicateValue)
{
	gRenDev->Logv("%s(0x%x, %d)\n", "D3DDeviceContext::SetPredication", pPredicate, PredicateValue);
}

void CCryDeviceLoggingHook::GSSetShaderResources_PreCallHook(
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumViews,
  /* [size_is][in] */ ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::GSSetShaderResources", Offset, NumViews, *ppShaderResourceViews);
}

void CCryDeviceLoggingHook::GSSetSamplers_PreCallHook(
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumSamplers,
  /* [size_is][in] */ ID3D11SamplerState* const* ppSamplers)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::GSSetSamplers", Offset, NumSamplers, *ppSamplers);
}

void CCryDeviceLoggingHook::OMSetRenderTargets_PreCallHook(
  /* [in] */ UINT NumViews,
  /* [size_is][in] */ ID3D11RenderTargetView* const* ppRenderTargetViews,
  /* [in] */ ID3D11DepthStencilView* pDepthStencilView)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::OMSetRenderTargets", NumViews, *ppRenderTargetViews, pDepthStencilView);
}

void CCryDeviceLoggingHook::OMSetBlendState_PreCallHook(
  /* [in] */ ID3D11BlendState* pBlendState,
  /* [in] */ const FLOAT BlendFactor[4],
  /* [in] */ UINT SampleMask)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDeviceContext::OMSetBlendState", pBlendState);
	D3D11_BLEND_DESC Desc;
	pBlendState->GetDesc(&Desc);

	if (!Desc.RenderTarget[0].BlendEnable)
		gRenDev->Logv("  Blending: Disabled\n");
	else
		gRenDev->Logv("  Blending: (%s, %s, BlendOp:%s, BlendOpAlpha:%s)\n", sD3DBLEND(Desc.RenderTarget[0].SrcBlend), sD3DBLEND(Desc.RenderTarget[0].DestBlend), sD3DBlendOP(Desc.RenderTarget[0].BlendOp), sD3DBlendOP(Desc.RenderTarget[0].BlendOpAlpha));

	for (int i = 1; i < 8; i++)
	{
		if (Desc.RenderTarget[i].BlendEnable)
			gRenDev->Logv("RT:  Blending %d: (%s, %s, BlendOp:%s, BlendOpAlpha:%s)\n", i, sD3DBLEND(Desc.RenderTarget[0].SrcBlend), sD3DBLEND(Desc.RenderTarget[0].DestBlend), sD3DBlendOP(Desc.RenderTarget[0].BlendOp), sD3DBlendOP(Desc.RenderTarget[0].BlendOpAlpha));
	}

	gRenDev->Logv("  WriteMask: %d\n", Desc.RenderTarget[0].RenderTargetWriteMask);
}

void CCryDeviceLoggingHook::OMSetDepthStencilState_PreCallHook(
  /* [in] */ ID3D11DepthStencilState* pDepthStencilState,
  /* [in] */ UINT StencilRef)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDeviceContext::OMSetDepthStencilState", pDepthStencilState);
	D3D11_DEPTH_STENCIL_DESC Desc;
	ZeroStruct(Desc);
	if (pDepthStencilState)
		pDepthStencilState->GetDesc(&Desc);
	gRenDev->Logv("  DepthTest: %s\n", Desc.DepthEnable ? "TRUE" : "FALSE");
	gRenDev->Logv("  DepthWrite: %s\n", Desc.DepthWriteMask ? "TRUE" : "FALSE");
	gRenDev->Logv("  DepthFunc: %s\n", sD3DCompareFunc(Desc.DepthFunc));

	gRenDev->Logv("  StencilEnable: %s\n", Desc.StencilEnable ? "TRUE" : "FALSE");

	if (Desc.StencilEnable)
	{
		gRenDev->Logv("  StencilRef: 0x%x\n", StencilRef);
		gRenDev->Logv("  StencilReadMask: 0x%x\n", Desc.StencilReadMask);
		gRenDev->Logv("  StencilWriteMask: 0x%x\n", Desc.StencilWriteMask);

		gRenDev->Logv("  FrontFace Stencil: \n");
		gRenDev->Logv("    StencilFailOp: %s\n", sD3DStencilOp(Desc.FrontFace.StencilFailOp));
		gRenDev->Logv("    StencilDepthFailOp: %s\n", sD3DStencilOp(Desc.FrontFace.StencilDepthFailOp));
		gRenDev->Logv("    StencilPassOp: %s\n", sD3DStencilOp(Desc.FrontFace.StencilPassOp));
		gRenDev->Logv("    StencilFunc: %s\n", sD3DCompareFunc(Desc.FrontFace.StencilFunc));

		gRenDev->Logv("  BackFace Stencil: \n");
		gRenDev->Logv("    StencilFailOp: %s\n", sD3DStencilOp(Desc.BackFace.StencilFailOp));
		gRenDev->Logv("    StencilDepthFailOp: %s\n", sD3DStencilOp(Desc.BackFace.StencilDepthFailOp));
		gRenDev->Logv("    StencilPassOp: %s\n", sD3DStencilOp(Desc.BackFace.StencilPassOp));
		gRenDev->Logv("    StencilFunc: %s\n", sD3DCompareFunc(Desc.BackFace.StencilFunc));
	}
}

void CCryDeviceLoggingHook::SOSetTargets_PreCallHook(
  /* [in] */ UINT NumBuffers,
  /* [size_is][in] */ ID3D11Buffer* const* ppSOTargets,
  /* [size_is][in] */ const UINT* pOffsets)
{
	gRenDev->Logv("%s(%d, 0x%x, %d)\n", "D3DDeviceContext::SOSetTargets", NumBuffers, *ppSOTargets, *pOffsets);
}

void CCryDeviceLoggingHook::DrawAuto_PreCallHook(void)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::DrawAuto");
}

void CCryDeviceLoggingHook::RSSetState_PreCallHook(
  /* [in] */ ID3D11RasterizerState* pRasterizerState)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDeviceContext::RSSetState", pRasterizerState);
	D3D11_RASTERIZER_DESC Desc;
	pRasterizerState->GetDesc(&Desc);
	gRenDev->Logv("   CullMode: %s\n", sD3DCull(Desc.CullMode));
	gRenDev->Logv("   Scissor: %s\n", Desc.ScissorEnable ? "TRUE" : "FALSE");
}

void CCryDeviceLoggingHook::RSSetViewports_PreCallHook(
  /* [in] */ UINT NumViewports,
  /* [size_is][in] */ const D3D11_VIEWPORT* pViewports)
{
	gRenDev->Logv("%s(%d, 0x%x) (%d, %d, %d, %d [%.2f, %.2f])\n", "D3DDeviceContext::RSSetViewports", NumViewports, pViewports, pViewports->TopLeftX, pViewports->TopLeftY, pViewports->Width, pViewports->Height, pViewports->MinDepth, pViewports->MaxDepth);
}

void CCryDeviceLoggingHook::RSSetScissorRects_PreCallHook(
  /* [in] */ UINT NumRects,
  /* [size_is][in] */ const D3D11_RECT* pRects)
{
	gRenDev->Logv("%s(%d, 0x%x)\n", "D3DDeviceContext::RSSetScissorRects", NumRects, pRects);
}

void CCryDeviceLoggingHook::ClearRenderTargetView_PreCallHook(
  /* [in] */ ID3D11RenderTargetView* pRenderTargetView,
  /* [in] */ const FLOAT ColorRGBA[4])
{
	gRenDev->Logv("%s(0x%x (%f, %f, %f, %f))\n", "D3DDeviceContext::ClearRenderTargetView", pRenderTargetView, ColorRGBA[0], ColorRGBA[1], ColorRGBA[2], ColorRGBA[3]);
}

void CCryDeviceLoggingHook::ClearDepthStencilView_PreCallHook(
  /* [in] */ ID3D11DepthStencilView* pDepthStencilView,
  /* [in] */ UINT Flags,
  /* [in] */ FLOAT Depth,
  /* [in] */ UINT8 Stencil)
{
	gRenDev->Logv("%s(0x%x, 0x%x, %f, %d)\n", "D3DDeviceContext::ClearDepthStencilView", pDepthStencilView, Flags, Depth, Stencil);
}

void CCryDeviceLoggingHook::GenerateMips_PreCallHook(
  /* [in] */ ID3D11ShaderResourceView* pShaderResourceView)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDeviceContext::GenerateMips", pShaderResourceView);
}

void CCryDeviceLoggingHook::VSGetConstantBuffers_PreCallHook(
  /* [in] */ UINT StartConstantSlot,
  /* [in] */ UINT NumBuffers,
  /* [size_is][out] */ ID3D11Buffer** ppConstantBuffers)
{
	gRenDev->Logv("%s(%d, %d)\n", "D3DDeviceContext::VSGetConstantBuffers", StartConstantSlot, NumBuffers);
}

void CCryDeviceLoggingHook::PSGetShaderResources_PreCallHook(
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumViews,
  /* [size_is][out] */ ID3D11ShaderResourceView** ppShaderResourceViews)
{
	gRenDev->Logv("%s(%d, %d)\n", "D3DDeviceContext::PSGetShaderResources", Offset, NumViews);
}

void CCryDeviceLoggingHook::PSGetShader_PreCallHook(
  /* [out][in] */ ID3D11PixelShader** ppPixelShader,
  /* [in] */ ID3D11ClassInstance** ppClassInstances,
  /* [in] */ UINT* pNumClassInstances)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::PSGetShader");
}

void CCryDeviceLoggingHook::PSGetSamplers_PreCallHook(
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumSamplers,
  /* [size_is][out] */ ID3D11SamplerState** ppSamplers)
{
	gRenDev->Logv("%s(%d, %d)\n", "D3DDeviceContext::PSGetSamplers", Offset, NumSamplers);
}

void CCryDeviceLoggingHook::VSGetShader_PreCallHook(
  /* [out][in] */ ID3D11VertexShader** ppVertexShader,
  /* [in] */ ID3D11ClassInstance** ppClassInstances,
  /* [in] */ UINT* pNumClassInstances)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::VSGetShader");
}

void CCryDeviceLoggingHook::PSGetConstantBuffers_PreCallHook(
  /* [in] */ UINT StartConstantSlot,
  /* [in] */ UINT NumBuffers,
  /* [size_is][out] */ ID3D11Buffer** ppConstantBuffers)
{
	gRenDev->Logv("%s(%d, %d)\n", "D3DDeviceContext::PSGetConstantBuffers", StartConstantSlot, NumBuffers);
}

void CCryDeviceLoggingHook::IAGetInputLayout_PreCallHook(
  /* [out][in] */ ID3D11InputLayout** ppInputLayout)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::IAGetInputLayout");
}

void CCryDeviceLoggingHook::IAGetVertexBuffers_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumBuffers,
  /* [size_is][out] */ ID3D11Buffer** ppVertexBuffers,
  /* [size_is][out] */ UINT* pStrides,
  /* [size_is][out] */ UINT* pOffsets)
{
	gRenDev->Logv("%s(%d, %d)\n", "D3DDeviceContext::IAGetVertexBuffers", StartSlot, NumBuffers);
}

void CCryDeviceLoggingHook::IAGetIndexBuffer_PreCallHook(
  /* [out] */ ID3D11Buffer** pIndexBuffer,
  /* [out] */ DXGI_FORMAT* Format,
  /* [out] */ UINT* Offset)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::IAGetIndexBuffer");
}

void CCryDeviceLoggingHook::GSGetConstantBuffers_PreCallHook(
  /* [in] */ UINT StartConstantSlot,
  /* [in] */ UINT NumBuffers,
  /* [size_is][out] */ ID3D11Buffer** ppConstantBuffers)
{
	gRenDev->Logv("%s(%d, %d)\n", "D3DDeviceContext::GSGetConstantBuffers", StartConstantSlot, NumBuffers);
}

void CCryDeviceLoggingHook::GSGetShader_PreCallHook(
  /* [out] */ ID3D11GeometryShader** ppGeometryShader,
  /* [in] */ ID3D11ClassInstance** ppClassInstances,
  /* [in] */ UINT* pNumClassInstances)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::GSGetShader");
}

void CCryDeviceLoggingHook::IAGetPrimitiveTopology_PreCallHook(
  /* [out] */ D3D11_PRIMITIVE_TOPOLOGY* pTopology)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::IAGetPrimitiveTopology");
}

void CCryDeviceLoggingHook::VSGetShaderResources_PreCallHook(
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumViews,
  /* [size_is][out] */ ID3D11ShaderResourceView** ppShaderResourceViews)
{
	gRenDev->Logv("%s(%d, %d)\n", "D3DDeviceContext::VSGetShaderResources", Offset, NumViews);
}

void CCryDeviceLoggingHook::VSGetSamplers_PreCallHook(
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumSamplers,
  /* [size_is][out] */ ID3D11SamplerState** ppSamplers)
{
	gRenDev->Logv("%s(%d, %d)\n", "D3DDeviceContext::VSGetSamplers", Offset, NumSamplers);
}

void CCryDeviceLoggingHook::GetPredication_PreCallHook(
  /* [out] */ ID3D11Predicate** ppPredicate,
  /* [out] */ BOOL* pPredicateValue)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::GetPredication");
}

void CCryDeviceLoggingHook::GSGetShaderResources_PreCallHook(
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumViews,
  /* [size_is][out] */ ID3D11ShaderResourceView** ppShaderResourceViews)
{
	gRenDev->Logv("%s(%d, %d)\n", "D3DDeviceContext::GSGetShaderResources", Offset, NumViews);
}

void CCryDeviceLoggingHook::GSGetSamplers_PreCallHook(
  /* [in] */ UINT Offset,
  /* [in] */ UINT NumSamplers,
  /* [size_is][out] */ ID3D11SamplerState** ppSamplers)
{
	gRenDev->Logv("%s(%d, %d)\n", "D3DDeviceContext::GSGetSamplers", Offset, NumSamplers);
}

void CCryDeviceLoggingHook::OMGetRenderTargets_PreCallHook(
  /* [in] */ UINT NumViews,
  /* [size_is][out] */ ID3D11RenderTargetView** ppRenderTargetViews,
  /* [out] */ ID3D11DepthStencilView** ppDepthStencilView)
{
	gRenDev->Logv("%s(%d)\n", "D3DDeviceContext::OMGetRenderTargets", NumViews);
}

void CCryDeviceLoggingHook::OMGetBlendState_PreCallHook(
  /* [out] */ ID3D11BlendState** ppBlendState,
  /* [out] */ FLOAT BlendFactor[4],
  /* [out] */ UINT* pSampleMask)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::OMGetBlendState");
}

void CCryDeviceLoggingHook::OMGetDepthStencilState_PreCallHook(
  /* [out] */ ID3D11DepthStencilState** ppDepthStencilState,
  /* [out] */ UINT* pStencilRef)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::OMGetDepthStencilState");
}

void CCryDeviceLoggingHook::SOGetTargets_PreCallHook(
  /* [in] */ UINT NumBuffers,
  /* [size_is][out] */ ID3D11Buffer** ppSOTargets)
{
	gRenDev->Logv("%s(%d)\n", "D3DDeviceContext::SOGetTargets", NumBuffers);
}

void CCryDeviceLoggingHook::RSGetState_PreCallHook(
  /* [out] */ ID3D11RasterizerState** ppRasterizerState)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::RSGetState");
}

void CCryDeviceLoggingHook::RSGetViewports_PreCallHook(
  /* [out][in] */ UINT* NumViewports,
  /* [size_is][out] */ D3D11_VIEWPORT* pViewports)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::RSGetViewports");
}

void CCryDeviceLoggingHook::RSGetScissorRects_PreCallHook(
  /* [out][in] */ UINT* NumRects,
  /* [size_is][out] */ D3D11_RECT* pRects)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::RSGetScissorRects");
}

void CCryDeviceLoggingHook::Flush_PreCallHook(void)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::Flush");
}

void CCryDeviceLoggingHook::GetDeviceRemovedReason_PreCallHook(void)
{
	gRenDev->Logv("%s()\n", "D3DDevice::GetDeviceRemovedReason");
}

void CCryDeviceLoggingHook::SetExceptionMode_PreCallHook(
  UINT RaiseFlags)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::SetExceptionMode", RaiseFlags);
}

void CCryDeviceLoggingHook::GetExceptionMode_PreCallHook(void)
{
	gRenDev->Logv("%s()\n", "D3DDevice::GetExceptionMode");
}

void CCryDeviceLoggingHook::GetPrivateData_Device_PreCallHook(
  /* [in] */ REFGUID guid,
  /* [out][in] */ UINT* pDataSize,
  /* [size_is][out] */ void* pData)
{
	gRenDev->Logv("%s()\n", "D3DDevice::GetPrivateData");
}

void CCryDeviceLoggingHook::SetPrivateData_Device_PreCallHook(
  /* [in] */ REFGUID guid,
  /* [in] */ UINT DataSize,
  /* [size_is][in] */ const void* pData)
{
	gRenDev->Logv("%s()\n", "D3DDevice::SetPrivateData");
}

void CCryDeviceLoggingHook::SetPrivateDataInterface_Device_PreCallHook(
  /* [in] */ REFGUID guid,
  /* [in] */ const IUnknown* pData)
{
	gRenDev->Logv("%s()\n", "D3DDevice::SetPrivateDataInterface");
}

void CCryDeviceLoggingHook::CreateBuffer_PreCallHook(
  /* [in] */ const D3D11_BUFFER_DESC* pDesc,
  /* [in] */ const D3D11_SUBRESOURCE_DATA* pInitialData,
  /* [out] */ ID3D11Buffer** ppBuffer)
{
	gRenDev->Logv("%s(0x%x, 0x%x)\n", "D3DDevice::CreateBuffer", pDesc, pInitialData);
}

void CCryDeviceLoggingHook::CreateTexture1D_PreCallHook(
  /* [in] */ const D3D11_TEXTURE1D_DESC* pDesc,
  /* [in] */ const D3D11_SUBRESOURCE_DATA* pInitialData,
  /* [out] */ ID3D11Texture1D** ppTexture1D)
{
	gRenDev->Logv("%s(0x%x, 0x%x)\n", "D3DDevice::CreateTexture1D", pDesc, pInitialData);
}

void CCryDeviceLoggingHook::CreateTexture2D_PreCallHook(
  /* [in] */ const D3D11_TEXTURE2D_DESC* pDesc,
  /* [in] */ const D3D11_SUBRESOURCE_DATA* pInitialData,
  /* [out] */ ID3D11Texture2D** ppTexture2D)
{
	gRenDev->Logv("%s(0x%x, 0x%x)\n", "D3DDevice::CreateTexture2D", pDesc, pInitialData);
}

void CCryDeviceLoggingHook::CreateTexture3D_PreCallHook(
  /* [in] */ const D3D11_TEXTURE3D_DESC* pDesc,
  /* [in] */ const D3D11_SUBRESOURCE_DATA* pInitialData,
  /* [out] */ ID3D11Texture3D** ppTexture3D)
{
	gRenDev->Logv("%s(0x%x, 0x%x)\n", "D3DDevice::CreateTexture3D", pDesc, pInitialData);
}

void CCryDeviceLoggingHook::CreateShaderResourceView_PreCallHook(
  /* [in] */ ID3D11Resource* pResource,
  /* [in] */ const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc,
  /* [out] */ ID3D11ShaderResourceView** ppSRView)
{
	gRenDev->Logv("%s(0x%x, 0x%x)\n", "D3DDevice::CreateShaderResourceView", pResource, pDesc);
}

void CCryDeviceLoggingHook::CreateRenderTargetView_PreCallHook(
  /* [in] */ ID3D11Resource* pResource,
  /* [in] */ const D3D11_RENDER_TARGET_VIEW_DESC* pDesc,
  /* [out] */ ID3D11RenderTargetView** ppRTView)
{
	gRenDev->Logv("%s(0x%x, 0x%x)\n", "D3DDevice::CreateRenderTargetView", pResource, pDesc);
}

void CCryDeviceLoggingHook::CreateDepthStencilView_PreCallHook(
  /* [in] */ ID3D11Resource* pResource,
  /* [in] */ const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc,
  /* [out] */ ID3D11DepthStencilView** ppDepthStencilView)
{
	gRenDev->Logv("%s(0x%x, 0x%x)\n", "D3DDevice::CreateDepthStencilView", pResource, pDesc);
}

void CCryDeviceLoggingHook::CreateInputLayout_PreCallHook(
  /* [size_is][in] */ const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs,
  /* [in] */ UINT NumElements,
  /* [in] */ const void* pShaderBytecodeWithInputSignature,
  /* [in] */ SIZE_T BytecodeLength,
  /* [out] */ ID3D11InputLayout** ppInputLayout)
{
	gRenDev->Logv("%s(0x%x, %d, 0x%x, %d)\n", "D3DDevice::CreateInputLayout", pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength);
}

void CCryDeviceLoggingHook::CreateVertexShader_PreCallHook(
  /* [in] */ const void* pShaderBytecode,
  /* [in] */ SIZE_T BytecodeLength,
  /* [in] */ ID3D11ClassLinkage* pClassLinkage,
  /* [out] */ ID3D11VertexShader** ppVertexShader)
{
	gRenDev->Logv("%s(0x%x, %d)\n", "D3DDevice::CreateVertexShader", pShaderBytecode, BytecodeLength);
}

void CCryDeviceLoggingHook::CreateGeometryShader_PreCallHook(
  /* [in] */ const void* pShaderBytecode,
  /* [in] */ SIZE_T BytecodeLength,
  /* [in] */ ID3D11ClassLinkage* pClassLinkage,
  /* [out] */ ID3D11GeometryShader** ppGeometryShader)
{
	gRenDev->Logv("%s(0x%x, %d)\n", "D3DDevice::CreateGeometryShader", pShaderBytecode, BytecodeLength);
}

void CCryDeviceLoggingHook::CreateGeometryShaderWithStreamOutput_PreCallHook(
  /* [in] */ const void* pShaderBytecode,
  /* [in] */ SIZE_T BytecodeLength,
  /* [size_is][in] */ const D3D11_SO_DECLARATION_ENTRY* pSODeclaration,
  /* [in] */ UINT NumEntries,
  /* [in] */ const UINT* pBufferStrides,
  /* [in] */ UINT NumStrides,
  /* [in] */ UINT RasterizedStream,
  /* [in] */ ID3D11ClassLinkage* pClassLinkage,
  /* [out] */ ID3D11GeometryShader** ppGeometryShader)
{
	gRenDev->Logv("%s(0x%x, %d, 0x%x, %d, %d)\n", "D3DDevice::CreateGeometryShader", pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, RasterizedStream);
}

void CCryDeviceLoggingHook::CreatePixelShader_PreCallHook(
  /* [in] */ const void* pShaderBytecode,
  /* [in] */ SIZE_T BytecodeLength,
  /* [in] */ ID3D11ClassLinkage* pClassLinkage,
  /* [out] */ ID3D11PixelShader** ppPixelShader)
{
	gRenDev->Logv("%s(0x%x, %d)\n", "D3DDevice::CreatePixelShader", pShaderBytecode, BytecodeLength);
}

void CCryDeviceLoggingHook::CreateBlendState_PreCallHook(
  /* [in] */ const D3D11_BLEND_DESC* pBlendStateDesc,
  /* [out] */ ID3D11BlendState** ppBlendState)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::CreateBlendState", pBlendStateDesc);
}

void CCryDeviceLoggingHook::CreateDepthStencilState_PreCallHook(
  /* [in] */ const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc,
  /* [out] */ ID3D11DepthStencilState** ppDepthStencilState)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::CreateDepthStencilState", pDepthStencilDesc);
}

void CCryDeviceLoggingHook::CreateRasterizerState_PreCallHook(
  /* [in] */ const D3D11_RASTERIZER_DESC* pRasterizerDesc,
  /* [out] */ ID3D11RasterizerState** ppRasterizerState)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::CreateRasterizerState", pRasterizerDesc);
}

void CCryDeviceLoggingHook::CreateSamplerState_PreCallHook(
  /* [in] */ const D3D11_SAMPLER_DESC* pSamplerDesc,
  /* [out] */ ID3D11SamplerState** ppSamplerState)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::CreateSamplerState", pSamplerDesc);
}

void CCryDeviceLoggingHook::CreateQuery_PreCallHook(
  /* [in] */ const D3D11_QUERY_DESC* pQueryDesc,
  /* [out] */ ID3D11Query** ppQuery)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::CreateQuery", pQueryDesc);
}
void CCryDeviceLoggingHook::CreatePredicate_PreCallHook(
  /* [in] */ const D3D11_QUERY_DESC* pPredicateDesc,
  /* [out] */ ID3D11Predicate** ppPredicate)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::CreatePredicate", pPredicateDesc);
}

void CCryDeviceLoggingHook::CreateUnorderedAccessView_PreCallHook(
  /* [in] */ ID3D11Resource* pResource,
  /* [in] */ const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
  /* [out] */ ID3D11UnorderedAccessView** ppUAView)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::CreateUnorderedAccessView", pDesc);
}

void CCryDeviceLoggingHook::CreateHullShader_PreCallHook(
  /* [in] */ const void* pShaderBytecode,
  /* [in] */ SIZE_T BytecodeLength,
  /* [in] */ ID3D11ClassLinkage* pClassLinkage,
  /* [out] */ ID3D11HullShader** ppHullShader
  )
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::CreateHullShader", ppHullShader);
}

void CCryDeviceLoggingHook::CreateDomainShader_PreCallHook(
  /* [in] */ const void* pShaderBytecode,
  /* [in] */ SIZE_T BytecodeLength,
  /* [in] */ ID3D11ClassLinkage* pClassLinkage,
  /* [out] */ ID3D11DomainShader** ppDomainShader
  )
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::CreateDomainShader", ppDomainShader);
}

void CCryDeviceLoggingHook::CreateComputeShader_PreCallHook(
  /* [in] */ const void* pShaderBytecode,
  /* [in] */ SIZE_T BytecodeLength,
  /* [in] */ ID3D11ClassLinkage* pClassLinkage,
  /* [out] */ ID3D11ComputeShader** ppComputeShader
  )
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::CreateComputeShader", ppComputeShader);
}

void CCryDeviceLoggingHook::CreateClassLinkage_PreCallHook(
  /* [out] */ ID3D11ClassLinkage** ppLinkage
  )
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::CreateClassLinkage", ppLinkage);
}

void CCryDeviceLoggingHook::CreateDeferredContext_PreCallHook(
  /* [in] */ UINT ContextFlags,
  /* [out] */ ID3D11DeviceContext** ppDeferredContext
  )
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::CreateDeferredContext", ppDeferredContext);
}

void CCryDeviceLoggingHook::CheckFeatureSupport_PreCallHook(
  /* [in] */ D3D11_FEATURE Feature,
  /* [in] */ void* pFeatureSupportData,
  /* [in] */ UINT FeatureSupportDataSize
  )
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::CheckFeatureSupport", Feature);
}

void CCryDeviceLoggingHook::GetFeatureLevel_PreCallHook()
{
	gRenDev->Logv("%s\n", "D3DDevice::GetFeatureLevel");
}

void CCryDeviceLoggingHook::GetImmediateContext_PreCallHook(
  /* [out] */ ID3D11DeviceContext** ppImmediateContext
  )
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::GetImmediateContext", ppImmediateContext);
}

void CCryDeviceLoggingHook::CreateCounter_PreCallHook(
  /* [in] */ const D3D11_COUNTER_DESC* pCounterDesc,
  /* [out] */ ID3D11Counter** ppCounter)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::CreateCounter", pCounterDesc);
}
void CCryDeviceLoggingHook::CheckFormatSupport_PreCallHook(
  /* [in] */ DXGI_FORMAT Format,
  /* [retval][out] */ UINT* pFormatSupport)
{
	gRenDev->Logv("%s(%s)\n", "D3DDevice::CheckFormatSupport", sD3DFMT(Format));
}

void CCryDeviceLoggingHook::CheckMultisampleQualityLevels_PreCallHook(
  DXGI_FORMAT Format,
  /* [in] */ UINT SampleCount,
  /* [retval][out] */ UINT* pNumQualityLevels)
{
	gRenDev->Logv("%s(%s, %d)\n", "D3DDevice::CheckMultisampleQualityLevels", sD3DFMT(Format), SampleCount);
}

void CCryDeviceLoggingHook::CheckCounterInfo_PreCallHook(
  /* [retval][out] */ D3D11_COUNTER_INFO* pCounterInfo)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::CheckCounterInfo", pCounterInfo);
}

void CCryDeviceLoggingHook::CheckCounter_PreCallHook(
  const D3D11_COUNTER_DESC* pDesc,
  D3D11_COUNTER_TYPE* pType,
  UINT* pActiveCounters,
  LPSTR wszName,
  UINT* pNameLength,
  LPSTR wszUnits,
  UINT* pUnitsLength,
  LPSTR wszDescription,
  UINT* pDescriptionLength)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDevice::CheckCounter", pDesc);
}

void CCryDeviceLoggingHook::CopySubresourceRegion_PreCallHook(
  ID3D11Resource* pDstResource,
  UINT DstSubresource,
  UINT DstX,
  UINT DstY,
  UINT DstZ,
  ID3D11Resource* pSrcResource,
  UINT SrcSubresource,
  const D3D11_BOX* pSrcBox
  )
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::CopySubresourceRegion");
}

void CCryDeviceLoggingHook::UpdateSubresource_PreCallHook(
  ID3D11Resource* pDstResource,
  UINT DstSubresource,
  const D3D11_BOX* pDstBox,
  const void* pSrcData,
  UINT SrcRowPitch,
  UINT SrcDepthPitch
  )
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::UpdateSubresource");
}

void CCryDeviceLoggingHook::ResolveSubresource_PreCallHook(
  ID3D11Resource* pDstResource,
  UINT DstSubresource,
  ID3D11Resource* pSrcResource,
  UINT SrcSubresource,
  DXGI_FORMAT Format
  )
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::ResolveSubresource");
}

void CCryDeviceLoggingHook::CopyResource_PreCallHook(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::CopyResource");
}

void CCryDeviceLoggingHook::ClearState_PreCallHook()
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::ClearState");
}

void CCryDeviceLoggingHook::OpenSharedResource_PreCallHook(
  HANDLE hResource,
  REFIID ReturnedInterface,
  void** ppResource
  )
{
	gRenDev->Logv("%s()\n", "D3DDevice::OpenSharedResource");
}

void CCryDeviceLoggingHook::GetCreationFlags_PreCallHook()
{
	gRenDev->Logv("%s()\n", "D3DDevice::GetCreationFlags");
}

void CCryDeviceLoggingHook::GetDevice_PreCallHook(
  /* [out] */ ID3D11Device** ppDevice)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDeviceContext::GetDevice", ppDevice);
}

void CCryDeviceLoggingHook::GetPrivateData_Context_PreCallHook(
  /* [in] */ REFGUID guid,
  /* [inout] */ UINT* pDataSize,
  /* [out_opt] */ void* pData)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDeviceContext::GetPrivateData", pData);
}

void CCryDeviceLoggingHook::SetPrivateData_Context_PreCallHook(
  /* [in] */ REFGUID guid,
  /* [in] */ UINT DataSize,
  /* [in_opt] */ const void* pData)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDeviceContext::SetPrivateData", pData);
}

void CCryDeviceLoggingHook::SetPrivateDataInterface_Context_PreCallHook(
  /* [in] */ REFGUID guid,
  /* [in_opt] */ const IUnknown* pData)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDeviceContext::SetPrivateDataInterface", pData);
}
void CCryDeviceLoggingHook::Map_PreCallHook(
  /* [in] */ ID3D11Resource* pResource,
  /* [in] */ UINT Subresource,
  /* [in] */ D3D11_MAP MapType,
  /* [in] */ UINT MapFlags,
  /* [out] */ D3D11_MAPPED_SUBRESOURCE* pMappedResource)
{
	gRenDev->Logv("%s(0x%x, %d, %d, %d, 0x%x)\n", "D3DDeviceContext::Map", pResource, Subresource, MapType, MapFlags, pMappedResource);
}

void CCryDeviceLoggingHook::Unmap_PreCallHook(
  /* [in] */ ID3D11Resource* pResource,
  /* [in] */ UINT Subresource)
{
	gRenDev->Logv("%s(0x%x, %d)\n", "D3DDeviceContext::Unmap", pResource, Subresource);
}

void CCryDeviceLoggingHook::Begin_PreCallHook(
  /* [in] */ ID3D11Asynchronous* pAsync)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDeviceContext::Begin", pAsync);
}

void CCryDeviceLoggingHook::End_PreCallHook(
  /* [in] */ ID3D11Asynchronous* pAsync)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDeviceContext::End", pAsync);
}

void CCryDeviceLoggingHook::GetData_PreCallHook(
  /* [in] */ ID3D11Asynchronous* pAsync,
  /* [out_opt] */ void* pData,
  /* [in] */ UINT DataSize,
  /* [in] */ UINT GetDataFlags)
{
	gRenDev->Logv("%s(0x%x, 0x%x, %d, %d)\n", "D3DDeviceContext::GetData", pAsync, pData, DataSize, GetDataFlags);
}

void CCryDeviceLoggingHook::OMSetRenderTargetsAndUnorderedAccessViews_PreCallHook(
  /* [in] */ UINT NumRTVs,
  /* [in_opt] */ ID3D11RenderTargetView* const* ppRenderTargetViews,
  /* [in_opt] */ ID3D11DepthStencilView* pDepthStencilView,
  /* [in] */ UINT UAVStartSlot,
  /* [in] */ UINT NumUAVs,
  /* [in_opt] */ ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
  /* [in_opt] */ const UINT* pUAVInitialCounts)
{
	gRenDev->Logv("%s(%d, 0x%x, 0x%x, %d, %d, 0x%x, 0x%x)\n", "D3DDeviceContext::OMSetRenderTargetsAndUnorderedAccessViews", NumRTVs, ppRenderTargetViews, pDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}

void CCryDeviceLoggingHook::DrawIndexedInstancedIndirect_PreCallHook(
  /* [in] */ ID3D11Buffer* pBufferForArgs,
  /* [in] */ UINT AlignedByteOffsetForArgs)
{
	gRenDev->Logv("%s(0x%x, %d)\n", "D3DDeviceContext::DrawIndexedInstancedIndirect", pBufferForArgs, AlignedByteOffsetForArgs);
}

void CCryDeviceLoggingHook::DrawInstancedIndirect_PreCallHook(
  /* [in] */ ID3D11Buffer* pBufferForArgs,
  /* [in] */ UINT AlignedByteOffsetForArgs)
{
	gRenDev->Logv("%s(0x%x, %d)\n", "D3DDeviceContext::DrawInstancedIndirect", pBufferForArgs, AlignedByteOffsetForArgs);
}

void CCryDeviceLoggingHook::Dispatch_PreCallHook(
  /* [in] */ UINT ThreadGroupCountX,
  /* [in] */ UINT ThreadGroupCountY,
  /* [in] */ UINT ThreadGroupCountZ)
{
	gRenDev->Logv("%s(%d, %d, %d)\n", "D3DDeviceContext::Dispatch", ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

void CCryDeviceLoggingHook::DispatchIndirect_PreCallHook(
  /* [in] */ ID3D11Buffer* pBufferForArgs,
  /* [in] */ UINT AlignedByteOffsetForArgs)
{
	gRenDev->Logv("%s(0x%x, %d)\n", "D3DDeviceContext::DispatchIndirect", pBufferForArgs, AlignedByteOffsetForArgs);
}

void CCryDeviceLoggingHook::CopyStructureCount_PreCallHook(
  /* [in] */ ID3D11Buffer* pDstBuffer,
  /* [in] */ UINT DstAlignedByteOffset,
  /* [in] */ ID3D11UnorderedAccessView* pSrcView)
{
	gRenDev->Logv("%s(0x%x, %d, 0x%x)\n", "D3DDeviceContext::CopyStructureCount", pDstBuffer, DstAlignedByteOffset, pSrcView);
}

void CCryDeviceLoggingHook::ClearUnorderedAccessViewUint_PreCallHook(
  /* [in] */ ID3D11UnorderedAccessView* pUnorderedAccessView,
  /* [in] */ const UINT Values[4])
{
	gRenDev->Logv("%s(0x%x, 0x%x)\n", "D3DDeviceContext::ClearUnorderedAccessViewUint", pUnorderedAccessView, Values);
}

void CCryDeviceLoggingHook::ClearUnorderedAccessViewFloat_PreCallHook(
  /* [in] */ ID3D11UnorderedAccessView* pUnorderedAccessView,
  /* [in] */ const FLOAT Values[4])
{
	gRenDev->Logv("%s(0x%x, 0x%x)\n", "D3DDeviceContext::ClearUnorderedAccessViewFloat", pUnorderedAccessView, Values);
}

void CCryDeviceLoggingHook::SetResourceMinLOD_PreCallHook(
  /* [in] */ ID3D11Resource* pResource,
  FLOAT MinLOD)
{
	gRenDev->Logv("%s(0x%x, %f)\n", "D3DDeviceContext::SetResourceMinLOD", pResource, MinLOD);
}

void CCryDeviceLoggingHook::GetResourceMinLOD_PreCallHook(
  /* [in] */ ID3D11Resource* pResource)
{
	gRenDev->Logv("%s(0x%x)\n", "D3DDeviceContext::GetResourceMinLOD", pResource);
}

	#if !CRY_PLATFORM_ORBIS
void CCryDeviceLoggingHook::ExecuteCommandList_PreCallHook(
  /* [in] */ ID3D11CommandList* pCommandList,
  /* [in] */ BOOL RestoreContextState)
{
	gRenDev->Logv("%s(0x%x, %d)\n", "D3DDeviceContext::ExecuteCommandList", pCommandList, RestoreContextState);
}
	#endif

void CCryDeviceLoggingHook::HSSetShaderResources_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumViews,
  /* [in] */ ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::HSSetShaderResources", StartSlot, NumViews, ppShaderResourceViews);
}

void CCryDeviceLoggingHook::HSSetShader_PreCallHook(
  /* [in_opt] */ ID3D11HullShader* pHullShader,
  /* [in] */ ID3D11ClassInstance* const* ppClassInstances,
  UINT NumClassInstances)
{
	gRenDev->Logv("%s(0x%x, 0x%x, %d)\n", "D3DDeviceContext::HSSetShader", pHullShader, ppClassInstances, NumClassInstances);
}

void CCryDeviceLoggingHook::HSSetSamplers_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumSamplers,
  /* [in] */ ID3D11SamplerState* const* ppSamplers)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::HSSetSamplers", StartSlot, NumSamplers, ppSamplers);
}

void CCryDeviceLoggingHook::HSSetConstantBuffers_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumBuffers,
  /* [in] */ ID3D11Buffer* const* ppConstantBuffers)
{
	if (ppConstantBuffers && *ppConstantBuffers)
	{
		int nBytes = 0;
		D3D11_BUFFER_DESC Desc;
		(*ppConstantBuffers)->GetDesc(&Desc);
		nBytes = Desc.ByteWidth;
		gRenDev->Logv("%s(%d, %d, 0x%x) (%d bytes)\n", "D3DDeviceContext::HSSetConstantBuffers", StartSlot, NumBuffers, *ppConstantBuffers, nBytes);
	}
}

void CCryDeviceLoggingHook::DSSetShaderResources_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumViews,
  /* [in] */ ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::DSSetShaderResources", StartSlot, NumViews, ppShaderResourceViews);
}

void CCryDeviceLoggingHook::DSSetShader_PreCallHook(
  /* [in_opt] */ ID3D11DomainShader* pDomainShader,
  /* [in] */ ID3D11ClassInstance* const* ppClassInstances,
  UINT NumClassInstances)
{
	gRenDev->Logv("%s(0x%x, 0x%x, %d)\n", "D3DDeviceContext::DSSetShader", pDomainShader, ppClassInstances, NumClassInstances);
}

void CCryDeviceLoggingHook::DSSetSamplers_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumSamplers,
  /* [in] */ ID3D11SamplerState* const* ppSamplers)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::DSSetSamplers", StartSlot, NumSamplers, ppSamplers);
#if !CRY_RENDERER_VULKAN
	D3D11_SAMPLER_DESC Desc;
	if (ppSamplers && *ppSamplers)
	{
		(*ppSamplers)->GetDesc(&Desc);
		gRenDev->Logv("  AddressU: %s\n", sD3DTAddress(Desc.AddressU));
		gRenDev->Logv("  AddressV: %s\n", sD3DTAddress(Desc.AddressV));
		gRenDev->Logv("  AddressW: %s\n", sD3DTAddress(Desc.AddressW));
		gRenDev->Logv("  Filter: %s\n", sD3DTFilter(Desc.Filter));
		if (Desc.Filter == D3D11_FILTER_ANISOTROPIC || Desc.Filter == D3D11_FILTER_COMPARISON_ANISOTROPIC)
			gRenDev->Logv("  MaxAnisotropy: %d\n", Desc.MaxAnisotropy);
	}
#endif
}

void CCryDeviceLoggingHook::DSSetConstantBuffers_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumBuffers,
  /* [in] */ ID3D11Buffer* const* ppConstantBuffers)
{
	if (ppConstantBuffers && *ppConstantBuffers)
	{
		int nBytes = 0;
		D3D11_BUFFER_DESC Desc;
		(*ppConstantBuffers)->GetDesc(&Desc);
		nBytes = Desc.ByteWidth;
		gRenDev->Logv("%s(%d, %d, 0x%x) (%d bytes)\n", "D3DDeviceContext::DSSetConstantBuffers", StartSlot, NumBuffers, *ppConstantBuffers, nBytes);
	}
}

void CCryDeviceLoggingHook::CSSetShaderResources_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumViews,
  /* [in] */ ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::CSSetShaderResources", StartSlot, NumViews, ppShaderResourceViews);
}

void CCryDeviceLoggingHook::CSSetUnorderedAccessViews_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumUAVs,
  /* [in] */ ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
  /* [in] */ const UINT* pUAVInitialCounts)
{
	gRenDev->Logv("%s(%d, %d, 0x%x, 0x%x)\n", "D3DDeviceContext::CSSetUnorderedAccessViews", StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}

void CCryDeviceLoggingHook::CSSetShader_PreCallHook(
  /* [in_opt] */ ID3D11ComputeShader* pComputeShader,
  /* [in] */ ID3D11ClassInstance* const* ppClassInstances,
  /* [in] */ UINT NumClassInstances)
{
	gRenDev->Logv("%s(0x%x, 0x%x, %d)\n", "D3DDeviceContext::CSSetShader", pComputeShader, ppClassInstances, NumClassInstances);
}

void CCryDeviceLoggingHook::CSSetSamplers_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumSamplers,
  /* [in] */ ID3D11SamplerState* const* ppSamplers)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::CSSetSamplers", StartSlot, NumSamplers, ppSamplers);
}

void CCryDeviceLoggingHook::CSSetConstantBuffers_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumBuffers,
  /* [in] */ ID3D11Buffer* const* ppConstantBuffers)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::CSSetConstantBuffers", StartSlot, NumBuffers, ppConstantBuffers);
}

void CCryDeviceLoggingHook::OMGetRenderTargetsAndUnorderedAccessViews_PreCallHook(
  /* [in] */ UINT NumRTVs,
  /* [out] */ ID3D11RenderTargetView** ppRenderTargetViews,
  /* [out] */ ID3D11DepthStencilView** ppDepthStencilView,
  /* [in] */ UINT UAVStartSlot,
  /* [in] */ UINT NumUAVs,
  /* [out] */ ID3D11UnorderedAccessView** ppUnorderedAccessViews)
{
	gRenDev->Logv("%s(%d, 0x%x, 0x%x, %d, %d, 0x%x)\n", "D3DDeviceContext::OMGetRenderTargetsAndUnorderedAccessViews", NumRTVs, ppRenderTargetViews, ppDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews);
}

void CCryDeviceLoggingHook::HSGetShaderResources_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumViews,
  /* [out] */ ID3D11ShaderResourceView** ppShaderResourceViews)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::HSGetShaderResources", StartSlot, NumViews, ppShaderResourceViews);
}

void CCryDeviceLoggingHook::HSGetShader_PreCallHook(
  /* [out] */ ID3D11HullShader** ppHullShader,
  /* [out] */ ID3D11ClassInstance** ppClassInstances,
  /* [inout] */ UINT* pNumClassInstances)
{
	gRenDev->Logv("%s(0x%x, 0x%x 0x%x)\n", "D3DDeviceContext::HSGetShader", ppHullShader, ppClassInstances, pNumClassInstances);
}

void CCryDeviceLoggingHook::HSGetSamplers_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumSamplers,
  /* [in] */ ID3D11SamplerState** ppSamplers)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::HSGetSamplers", StartSlot, NumSamplers, ppSamplers);
}

void CCryDeviceLoggingHook::HSGetConstantBuffers_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumBuffers,
  /* [out] */ ID3D11Buffer** ppConstantBuffers)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::HSGetConstantBuffers", StartSlot, NumBuffers, ppConstantBuffers);
}

void CCryDeviceLoggingHook::DSGetShaderResources_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumViews,
  /* [out] */ ID3D11ShaderResourceView** ppShaderResourceViews)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::DSGetShaderResources", StartSlot, NumViews, ppShaderResourceViews);
}

void CCryDeviceLoggingHook::DSGetShader_PreCallHook(
  /* [out] */ ID3D11DomainShader** ppDomainShader,
  /* [out] */ ID3D11ClassInstance** ppClassInstances,
  /* [inout] */ UINT* pNumClassInstances)
{
	gRenDev->Logv("%s(0x%x, 0x%x, 0x%x)\n", "D3DDeviceContext::DSGetShader", ppDomainShader, ppClassInstances, pNumClassInstances);
}

void CCryDeviceLoggingHook::DSGetSamplers_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumSamplers,
  /* [out] */ ID3D11SamplerState** ppSamplers)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::DSGetSamplers", StartSlot, NumSamplers, ppSamplers);
}

void CCryDeviceLoggingHook::DSGetConstantBuffers_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumBuffers,
  /* [out] */ ID3D11Buffer** ppConstantBuffers)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::DSGetConstantBuffers", StartSlot, NumBuffers, ppConstantBuffers);
}

void CCryDeviceLoggingHook::CSGetShaderResources_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumViews,
  /* [out] */ ID3D11ShaderResourceView** ppShaderResourceViews)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::CSGetShaderResources", StartSlot, NumViews, ppShaderResourceViews);
}

void CCryDeviceLoggingHook::CSGetUnorderedAccessViews_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumUAVs,
  /* [out] */ ID3D11UnorderedAccessView** ppUnorderedAccessViews)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::CSGetUnorderedAccessViews", StartSlot, NumUAVs, ppUnorderedAccessViews);
}

void CCryDeviceLoggingHook::CSGetShader_PreCallHook(
  /* [out] */ ID3D11ComputeShader** ppComputeShader,
  /* [out] */ ID3D11ClassInstance** ppClassInstances,
  /* [inout] */ UINT* pNumClassInstances)
{
	gRenDev->Logv("%s(0x%x, 0x%x, 0x%x)\n", "D3DDeviceContext::CSGetUnorderedAccessViews", ppComputeShader, ppClassInstances, pNumClassInstances);
}

void CCryDeviceLoggingHook::CSGetSamplers_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumSamplers,
  /* [out] */ ID3D11SamplerState** ppSamplers)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::CSGetSamplers", StartSlot, NumSamplers, ppSamplers);
}

void CCryDeviceLoggingHook::CSGetConstantBuffers_PreCallHook(
  /* [in] */ UINT StartSlot,
  /* [in] */ UINT NumBuffers,
  /* [out] */ ID3D11Buffer** ppConstantBuffers)
{
	gRenDev->Logv("%s(%d, %d, 0x%x)\n", "D3DDeviceContext::CSGetConstantBuffers", StartSlot, NumBuffers, ppConstantBuffers);
}

void CCryDeviceLoggingHook::GetType_PreCallHook(void)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::GetType");
}

void CCryDeviceLoggingHook::GetContextFlags_PreCallHook(void)
{
	gRenDev->Logv("%s()\n", "D3DDeviceContext::GetContextFlags");
}

	#if !CRY_PLATFORM_ORBIS
void CCryDeviceLoggingHook::FinishCommandList_PreCallHook(
  BOOL RestoreDeferredContextState,
  /* [out] */ ID3D11CommandList** ppCommandList)
{
	gRenDev->Logv("%s(%d, 0x%x)\n", "D3DDeviceContext::FinishCommandList", RestoreDeferredContextState, ppCommandList);
}
	#endif

void CD3D9Renderer::SetLogFuncs(bool bSet)
{
	static bool sSet = 0;

	if (bSet == sSet)
		return;

	sSet = bSet;

	if (bSet)
	{
		RegisterDeviceWrapperHook(new CCryDeviceLoggingHook());
	}
	else
	{
		UnregisterDeviceWrapperHook(CCryDeviceLoggingHook::GetName());
	}
}

#endif //DO_RENDERLOG
