// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   DXGL_ID3D11Device.h
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Contains a definition of the ID3D11Device interface
//               matching the one in the DirectX SDK
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __DXGL_ID3D11Device_h__
#define __DXGL_ID3D11Device_h__

#if !DXGL_FULL_EMULATION
	#include "../Interfaces/CCryDXGLBase.hpp"
#endif //!DXGL_FULL_EMULATION

struct ID3D11DeviceContext;

struct ID3D11Device
#if DXGL_FULL_EMULATION
	: IUnknown
#else
	: CCryDXGLBase
#endif
{
	// ID3D11Device interface (adapted from DXSDK/include/D3D11.h)
	virtual HRESULT STDMETHODCALLTYPE           CreateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateTexture1D(const D3D11_TEXTURE1D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateTexture2D(const D3D11_TEXTURE2D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateTexture3D(const D3D11_TEXTURE3D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateShaderResourceView(ID3D11Resource* pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc, ID3D11ShaderResourceView** ppSRView) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateUnorderedAccessView(ID3D11Resource* pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc, ID3D11UnorderedAccessView** ppUAView) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateRenderTargetView(ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc, ID3D11RenderTargetView** ppRTView) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateDepthStencilView(ID3D11Resource* pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc, ID3D11DepthStencilView** ppDepthStencilView) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements, const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout** ppInputLayout) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateVertexShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateGeometryShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateGeometryShaderWithStreamOutput(const void* pShaderBytecode, SIZE_T BytecodeLength, const D3D11_SO_DECLARATION_ENTRY* pSODeclaration, UINT NumEntries, const UINT* pBufferStrides, UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreatePixelShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateHullShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11HullShader** ppHullShader) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateDomainShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11DomainShader** ppDomainShader) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateComputeShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11ComputeShader** ppComputeShader) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateClassLinkage(ID3D11ClassLinkage** ppLinkage) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateBlendState(const D3D11_BLEND_DESC* pBlendStateDesc, ID3D11BlendState** ppBlendState) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc, ID3D11DepthStencilState** ppDepthStencilState) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateRasterizerState(const D3D11_RASTERIZER_DESC* pRasterizerDesc, ID3D11RasterizerState** ppRasterizerState) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateSamplerState(const D3D11_SAMPLER_DESC* pSamplerDesc, ID3D11SamplerState** ppSamplerState) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateQuery(const D3D11_QUERY_DESC* pQueryDesc, ID3D11Query** ppQuery) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreatePredicate(const D3D11_QUERY_DESC* pPredicateDesc, ID3D11Predicate** ppPredicate) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateCounter(const D3D11_COUNTER_DESC* pCounterDesc, ID3D11Counter** ppCounter) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CreateDeferredContext(UINT ContextFlags, ID3D11DeviceContext** ppDeferredContext) = 0;
	virtual HRESULT STDMETHODCALLTYPE           OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface, void** ppResource) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CheckFormatSupport(DXGI_FORMAT Format, UINT* pFormatSupport) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount, UINT* pNumQualityLevels) = 0;
	virtual void STDMETHODCALLTYPE              CheckCounterInfo(D3D11_COUNTER_INFO* pCounterInfo) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CheckCounter(const D3D11_COUNTER_DESC* pDesc, D3D11_COUNTER_TYPE* pType, UINT* pActiveCounters, LPSTR szName, UINT* pNameLength, LPSTR szUnits, UINT* pUnitsLength, LPSTR szDescription, UINT* pDescriptionLength) = 0;
	virtual HRESULT STDMETHODCALLTYPE           CheckFeatureSupport(D3D11_FEATURE Feature, void* pFeatureSupportData, UINT FeatureSupportDataSize) = 0;
	virtual HRESULT STDMETHODCALLTYPE           GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData) = 0;
	virtual HRESULT STDMETHODCALLTYPE           SetPrivateData(REFGUID guid, UINT DataSize, const void* pData) = 0;
	virtual HRESULT STDMETHODCALLTYPE           SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) = 0;
	virtual D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel(void) = 0;
	virtual UINT STDMETHODCALLTYPE              GetCreationFlags(void) = 0;
	virtual HRESULT STDMETHODCALLTYPE           GetDeviceRemovedReason(void) = 0;
	virtual void STDMETHODCALLTYPE              GetImmediateContext(ID3D11DeviceContext** ppImmediateContext) = 0;
	virtual HRESULT STDMETHODCALLTYPE           SetExceptionMode(UINT RaiseFlags) = 0;
	virtual UINT STDMETHODCALLTYPE              GetExceptionMode(void) = 0;

	// IDXGIDevice interface (adapted from DXSDK/include/dxgi.h)
	virtual HRESULT STDMETHODCALLTYPE GetAdapter(IDXGIAdapter** pAdapter)                                                                                                                      { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE CreateSurface(const DXGI_SURFACE_DESC* pDesc, UINT NumSurfaces, DXGI_USAGE Usage, const DXGI_SHARED_RESOURCE* pSharedResource, IDXGISurface** ppSurface) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE QueryResourceResidency(IUnknown* const* ppResources, DXGI_RESIDENCY* pResidencyStatus, UINT NumResources)                                                { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(INT Priority)                                                                                                                       { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(INT* pPriority)                                                                                                                     { return E_NOTIMPL; }

	// IDXGIDevice1 interface (adapted from DXSDK/include/dxgi.h)
	virtual HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency)   { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT* pMaxLatency) { return E_NOTIMPL; }
};

#if !DXGL_FULL_EMULATION
typedef ID3D11Device IDXGIDevice;
typedef ID3D11Device IDXGIDevice1;
#endif // !DXGL_FULL_EMULATION

#endif // __DXGL_ID3D11Device_h__
