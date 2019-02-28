// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/CCryDX12Object.hpp"
#include "DX12/API/DX12Device.hpp"

class CCryDX12DeviceContext;

class CCryDX12Device : public CCryDX12Object<ID3D11Device1ToImplement>
{
public:
	DX12_OBJECT(CCryDX12Device, CCryDX12Object<ID3D11Device1ToImplement> );

	static CCryDX12Device* Create(CCryDX12GIAdapter* pAdapter, D3D_FEATURE_LEVEL* pFeatureLevel);

	CCryDX12Device(NCryDX12::CDevice* device);

	NCryDX12::CDevice*     GetDX12Device() const                 { return m_pDevice; }

	ID3D12Device*          GetD3D12Device() const                { return m_pDevice->GetD3D12Device(); }

	CCryDX12DeviceContext* GetDeviceContext() const              { return m_pMainContext; }
	CCryDX12DeviceContext* GetDeviceContext(UINT nodeMask) const { return m_pNodeContexts[nodeMask]; }

	ILINE UINT             GetNodeCount() const
	{
		return m_numNodes;
	}
	ILINE UINT GetCreationMask(bool bCPUHeap) const
	{
		return (bCPUHeap ? blsi(m_crtMask) : m_crtMask);
	}
	ILINE UINT GetVisibilityMask(bool bCPUHeap) const
	{
		return (bCPUHeap ? m_crtMask : m_visMask);
	}
	ILINE UINT GetAllMask() const
	{
		return m_allMask;
	}

	void SetVisibilityMask(UINT visibilityMask)
	{
		m_visMask = m_crtMask & visibilityMask;
	}

	#pragma region /* ID3D11Device implementation */

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateBuffer(
	  _In_ const D3D11_BUFFER_DESC* pDesc,
	  _In_opt_ const D3D11_SUBRESOURCE_DATA* pInitialData,
	  _Out_opt_ ID3D11Buffer** ppBuffer) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateTexture1D(
	  _In_  const D3D11_TEXTURE1D_DESC * pDesc,
	  _In_reads_opt_(_Inexpressible_(pDesc->MipLevels * pDesc->ArraySize))  const D3D11_SUBRESOURCE_DATA * pInitialData,
	  _Out_opt_ ID3D11Texture1D * *ppTexture1D) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateTexture2D(
	  _In_  const D3D11_TEXTURE2D_DESC * pDesc,
	  _In_reads_opt_(_Inexpressible_(pDesc->MipLevels * pDesc->ArraySize))  const D3D11_SUBRESOURCE_DATA * pInitialData,
	  _Out_opt_ ID3D11Texture2D * *ppTexture2D) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateTexture3D(
	  _In_  const D3D11_TEXTURE3D_DESC * pDesc,
	  _In_reads_opt_(_Inexpressible_(pDesc->MipLevels))  const D3D11_SUBRESOURCE_DATA * pInitialData,
	  _Out_opt_ ID3D11Texture3D * *ppTexture3D) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateShaderResourceView(
	  _In_ ID3D11Resource* pResource,
	  _In_opt_ const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc,
	  _Out_opt_ ID3D11ShaderResourceView** ppSRView) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(
	  _In_ ID3D11Resource* pResource,
	  _In_opt_ const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
	  _Out_opt_ ID3D11UnorderedAccessView** ppUAView) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateRenderTargetView(
	  _In_ ID3D11Resource* pResource,
	  _In_opt_ const D3D11_RENDER_TARGET_VIEW_DESC* pDesc,
	  _Out_opt_ ID3D11RenderTargetView** ppRTView) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateDepthStencilView(
	  _In_ ID3D11Resource* pResource,
	  _In_opt_ const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc,
	  _Out_opt_ ID3D11DepthStencilView** ppDepthStencilView) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateInputLayout(
	  _In_reads_(NumElements)  const D3D11_INPUT_ELEMENT_DESC * pInputElementDescs,
	  _In_range_(0, D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT)  UINT NumElements,
	  _In_  const void* pShaderBytecodeWithInputSignature,
	  _In_ SIZE_T BytecodeLength,
	  _Out_opt_ ID3D11InputLayout * *ppInputLayout) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateVertexShader(
	  _In_ const void* pShaderBytecode,
	  _In_ SIZE_T BytecodeLength,
	  _In_opt_ ID3D11ClassLinkage* pClassLinkage,
	  _Out_opt_ ID3D11VertexShader** ppVertexShader) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateGeometryShader(
	  _In_ const void* pShaderBytecode,
	  _In_ SIZE_T BytecodeLength,
	  _In_opt_ ID3D11ClassLinkage* pClassLinkage,
	  _Out_opt_ ID3D11GeometryShader** ppGeometryShader) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(
	  _In_  const void* pShaderBytecode,
	  _In_ SIZE_T BytecodeLength,
	  _In_reads_opt_(NumEntries)  const D3D11_SO_DECLARATION_ENTRY * pSODeclaration,
	  _In_range_(0, D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT)  UINT NumEntries,
	  _In_reads_opt_(NumStrides)  const UINT * pBufferStrides,
	  _In_range_(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumStrides,
	  _In_ UINT RasterizedStream,
	  _In_opt_ ID3D11ClassLinkage * pClassLinkage,
	  _Out_opt_ ID3D11GeometryShader * *ppGeometryShader) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreatePixelShader(
	  _In_ const void* pShaderBytecode,
	  _In_ SIZE_T BytecodeLength,
	  _In_opt_ ID3D11ClassLinkage* pClassLinkage,
	  _Out_opt_ ID3D11PixelShader** ppPixelShader) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateHullShader(
	  _In_ const void* pShaderBytecode,
	  _In_ SIZE_T BytecodeLength,
	  _In_opt_ ID3D11ClassLinkage* pClassLinkage,
	  _Out_opt_ ID3D11HullShader** ppHullShader) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateDomainShader(
	  _In_ const void* pShaderBytecode,
	  _In_ SIZE_T BytecodeLength,
	  _In_opt_ ID3D11ClassLinkage* pClassLinkage,
	  _Out_opt_ ID3D11DomainShader** ppDomainShader) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateComputeShader(
	  _In_ const void* pShaderBytecode,
	  _In_ SIZE_T BytecodeLength,
	  _In_opt_ ID3D11ClassLinkage* pClassLinkage,
	  _Out_opt_ ID3D11ComputeShader** ppComputeShader) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateClassLinkage(
	  _Out_ ID3D11ClassLinkage** ppLinkage) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateBlendState(
	  _In_ const D3D11_BLEND_DESC* pBlendStateDesc,
	  _Out_opt_ ID3D11BlendState** ppBlendState) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateDepthStencilState(
	  _In_ const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc,
	  _Out_opt_ ID3D11DepthStencilState** ppDepthStencilState) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateRasterizerState(
	  _In_ const D3D11_RASTERIZER_DESC* pRasterizerDesc,
	  _Out_opt_ ID3D11RasterizerState** ppRasterizerState) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateSamplerState(
	  _In_ const D3D11_SAMPLER_DESC* pSamplerDesc,
	  _Out_opt_ ID3D11SamplerState** ppSamplerState) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateQuery(
	  _In_ const D3D11_QUERY_DESC* pQueryDesc,
	  _Out_opt_ ID3D11Query** ppQuery) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreatePredicate(
	  _In_ const D3D11_QUERY_DESC* pPredicateDesc,
	  _Out_opt_ ID3D11Predicate** ppPredicate) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateCounter(
	  _In_ const D3D11_COUNTER_DESC* pCounterDesc,
	  _Out_opt_ ID3D11Counter** ppCounter) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateDeferredContext(
	  UINT ContextFlags,
	  _Out_opt_ ID3D11DeviceContext** ppDeferredContext) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE OpenSharedResource(
	  _In_ HANDLE hResource,
	  _In_ REFIID ReturnedInterface,
	  _Out_opt_ void** ppResource) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CheckFormatSupport(
	  _In_ DXGI_FORMAT Format,
	  _Out_ UINT* pFormatSupport) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(
	  _In_ DXGI_FORMAT Format,
	  _In_ UINT SampleCount,
	  _Out_ UINT* pNumQualityLevels) FINALGFX;

	VIRTUALGFX void STDMETHODCALLTYPE CheckCounterInfo(
	  _Out_ D3D11_COUNTER_INFO* pCounterInfo) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CheckCounter(
	  _In_ const D3D11_COUNTER_DESC* pDesc,
	  _Out_ D3D11_COUNTER_TYPE* pType,
	  _Out_ UINT* pActiveCounters,
	  _Out_writes_opt_(*pNameLength)  LPSTR szName,
	  _Inout_opt_ UINT* pNameLength,
	  _Out_writes_opt_(*pUnitsLength)  LPSTR szUnits,
	  _Inout_opt_ UINT* pUnitsLength,
	  _Out_writes_opt_(*pDescriptionLength)  LPSTR szDescription,
	  _Inout_opt_ UINT* pDescriptionLength) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CheckFeatureSupport(
	  D3D11_FEATURE Feature,
	  _Out_writes_bytes_(FeatureSupportDataSize)  void* pFeatureSupportData,
	  UINT FeatureSupportDataSize) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetPrivateData(
	  _In_ REFGUID guid,
	  _Inout_ UINT* pDataSize,
	  _Out_writes_bytes_opt_(*pDataSize)  void* pData) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetPrivateData(
	  _In_ REFGUID guid,
	  _In_ UINT DataSize,
	  _In_reads_bytes_opt_(DataSize)  const void* pData) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
	  _In_ REFGUID guid,
	  _In_opt_ const IUnknown* pData) FINALGFX;

	VIRTUALGFX D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel() FINALGFX;

	VIRTUALGFX UINT STDMETHODCALLTYPE              GetCreationFlags() FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE           GetDeviceRemovedReason() FINALGFX;

	VIRTUALGFX void STDMETHODCALLTYPE              GetImmediateContext(
	  _Out_ ID3D11DeviceContext** ppImmediateContext) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetExceptionMode(
	  UINT RaiseFlags) FINALGFX;

	VIRTUALGFX UINT STDMETHODCALLTYPE GetExceptionMode() FINALGFX;

	#pragma endregion

	#pragma region /* ID3D11Device1 implementation */

	VIRTUALGFX void STDMETHODCALLTYPE GetImmediateContext1(
	  /* [annotation] */
	  _Outptr_ ID3D11DeviceContext1** ppImmediateContext) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateDeferredContext1(
	  UINT ContextFlags,
	  /* [annotation] */
	  _COM_Outptr_opt_ ID3D11DeviceContext1** ppDeferredContext) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateBlendState1(
	  /* [annotation] */
	  _In_ const D3D11_BLEND_DESC1* pBlendStateDesc,
	  /* [annotation] */
	  _COM_Outptr_opt_ ID3D11BlendState1** ppBlendState) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateRasterizerState1(
	  /* [annotation] */
	  _In_ const D3D11_RASTERIZER_DESC1* pRasterizerDesc,
	  /* [annotation] */
	  _COM_Outptr_opt_ ID3D11RasterizerState1** ppRasterizerState) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateDeviceContextState(
	  UINT Flags,
	  /* [annotation] */
	  _In_reads_(FeatureLevels)  const D3D_FEATURE_LEVEL * pFeatureLevels,
	  UINT FeatureLevels,
	  UINT SDKVersion,
	  REFIID EmulatedInterface,
	  /* [annotation] */
	  _Out_opt_ D3D_FEATURE_LEVEL * pChosenFeatureLevel,
	  /* [annotation] */
	  _Out_opt_ ID3DDeviceContextState * *ppContextState) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE OpenSharedResource1(
	  /* [annotation] */
	  _In_ HANDLE hResource,
	  /* [annotation] */
	  _In_ REFIID returnedInterface,
	  /* [annotation] */
	  _COM_Outptr_ void** ppResource) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE OpenSharedResourceByName(
	  /* [annotation] */
	  _In_ LPCWSTR lpName,
	  /* [annotation] */
	  _In_ DWORD dwDesiredAccess,
	  /* [annotation] */
	  _In_ REFIID returnedInterface,
	  /* [annotation] */
	  _COM_Outptr_ void** ppResource) FINALGFX;

	#pragma endregion

	HRESULT STDMETHODCALLTYPE CreateTarget1D(
	  _In_  const D3D11_TEXTURE1D_DESC * pDesc,
	  _In_  const FLOAT cClearValue[4],
	  _In_reads_opt_(_Inexpressible_(pDesc->MipLevels * pDesc->ArraySize))  const D3D11_SUBRESOURCE_DATA * pInitialData,
	  _Out_opt_ ID3D11Texture1D * *ppTexture1D);

	HRESULT STDMETHODCALLTYPE CreateTarget2D(
	  _In_  const D3D11_TEXTURE2D_DESC * pDesc,
	  _In_  const FLOAT cClearValue[4],
	  _In_reads_opt_(_Inexpressible_(pDesc->MipLevels * pDesc->ArraySize))  const D3D11_SUBRESOURCE_DATA * pInitialData,
	  _Out_opt_ ID3D11Texture2D * *ppTexture2D);

	HRESULT STDMETHODCALLTYPE CreateTarget3D(
	  _In_  const D3D11_TEXTURE3D_DESC * pDesc,
	  _In_  const FLOAT cClearValue[4],
	  _In_reads_opt_(_Inexpressible_(pDesc->MipLevels))  const D3D11_SUBRESOURCE_DATA * pInitialData,
	  _Out_opt_ ID3D11Texture3D * *ppTexture3D);

	HRESULT STDMETHODCALLTYPE CreateNullResource(
	  _In_ D3D11_RESOURCE_DIMENSION eType,
	  _Out_opt_ ID3D11Resource** ppNullResource);

	HRESULT STDMETHODCALLTYPE ReleaseNullResource(
	  _In_ ID3D11Resource* pNullResource);

	HRESULT STDMETHODCALLTYPE CreateStagingResource(
	  _In_ ID3D11Resource* pInputResource,
	  _Out_opt_ ID3D11Resource** ppStagingResource,
	  _In_ BOOL Upload);

	HRESULT STDMETHODCALLTYPE ReleaseStagingResource(
	  _In_ ID3D11Resource* pStagingResource);

	ILINE void FlushAndWaitForGPU() { m_pDevice->FlushAndWaitForGPU(); }

private:
	DX12_PTR(NCryDX12::CDevice) m_pDevice;

	UINT m_numNodes;
	UINT m_crtMask;
	UINT m_shrMask;
	UINT m_visMask;
	UINT m_allMask;

	DX12_PTR(CCryDX12DeviceContext) m_pMainContext;
	std::vector<DX12_PTR(CCryDX12DeviceContext)> m_pNodeContexts;
};
