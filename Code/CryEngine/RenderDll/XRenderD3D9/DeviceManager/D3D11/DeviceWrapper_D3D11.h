// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#if !defined(CRY_DEVICE_WRAPPER_H_)
#define CRY_DEVICE_WRAPPER_H_

#if defined(USE_NV_API) && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	#include NV_API_HEADER
#endif

///////////////////////////////////////////////////////////////////////////////
// Forward Declarations
#if !CRY_RENDERER_OPENGL && !CRY_RENDERER_OPENGLES && !CRY_RENDERER_VULKAN && !CRY_PLATFORM_ORBIS && DX11_COM_INTERFACES
	struct ID3D11DeviceContext1;
	struct ID3D11CommandList;
	struct ID3D11View;
	struct ID3DDeviceContextState;
#endif

///////////////////////////////////////////////////////////////////////////////
// only enable the device hook checks if a system requiering those is compiled in
#if ((DO_RENDERLOG) || (CAPTURE_REPLAY_LOG))
	#define DX11_ENABLEDHOOKS DX11_WRAPPABLE_INTERFACE
#endif

///////////////////////////////////////////////////////////////////////////////
// convience macro for all device wrapper function. currently disabled as it causes too much overhead for general profiling
#define CRY_DEVICE_WRAPPER_PROFILE()

struct SRenderStatePassD3D;
struct CRenderObjectD3D;
class CDeviceObjectFactory;

// Represent state of the device pipeline, Dx12 PSO, or Vulkan VkPipeline
class CDevicePipelineState
{
public:
	virtual bool Execute(SRenderStatePassD3D* pPassState) = 0;
	virtual int  Release() = 0;
	virtual int  AddRef() = 0;
};

///////////////////////////////////////////////////////////////////////////////
// Base Class for DeviceWrapper Hooks
// Contains all supported device/context function
// For each function, a PreCallHock and a PostCallHock is provided
// Implementation classes are free to implemented all requiered function
// all other functions are using default 'nop' implementaitions
// Those can be registered by the renderere with RegisterDeviceWrapperHook
// to unregister, the name of the hook must be passed into UnregisterDeviceWrapperHook
struct ICryDeviceWrapperHook : public _reference_target_t
{
private:
	_smart_ptr<ICryDeviceWrapperHook> m_pNext;

public:

	ICryDeviceWrapperHook() : m_pNext(NULL) {}
	virtual ~ICryDeviceWrapperHook() {}

	_smart_ptr<ICryDeviceWrapperHook> GetNext()                             { return m_pNext; }
	void                              SetNext(ICryDeviceWrapperHook* pNext) { m_pNext = pNext; }

	virtual const char*               Name() const = 0;

	///////////////////////////////////////////////////////////////////////////////
	// pre function call hooks, those are called before the actual device function is invoked
	///////////////////////////////////////////////////////////////////////////////

	/**************************/
	/*** ID3DDevice methods ***/
	virtual void QueryInterface_Device_PreCallHook(REFIID riid, void** ppvObj)                                                                                                                                                                                                                                                           {};
	virtual void AddRef_Device_PreCallHook()                                                                                                                                                                                                                                                                                             {};
	virtual void Release_Device_PreCallHook()                                                                                                                                                                                                                                                                                            {};

	virtual void GetDeviceRemovedReason_PreCallHook()                                                                                                                                                                                                                                                                                    {};

	virtual void SetExceptionMode_PreCallHook(UINT RaiseFlags)                                                                                                                                                                                                                                                                           {};
	virtual void GetExceptionMode_PreCallHook()                                                                                                                                                                                                                                                                                          {};

	virtual void GetPrivateData_Device_PreCallHook(REFGUID guid, UINT* pDataSize, void* pData)                                                                                                                                                                                                                                           {};
	virtual void SetPrivateData_Device_PreCallHook(REFGUID guid, UINT DataSize, const void* pData)                                                                                                                                                                                                                                       {};
	virtual void SetPrivateDataInterface_Device_PreCallHook(REFGUID guid, const IUnknown* pData)                                                                                                                                                                                                                                         {};

	virtual void CreateBuffer_PreCallHook(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer)                                                                                                                                                                                           {};

	virtual void CreateTexture1D_PreCallHook(const D3D11_TEXTURE1D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D)                                                                                                                                                                               {};
	virtual void CreateTexture2D_PreCallHook(const D3D11_TEXTURE2D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D)                                                                                                                                                                               {};
	virtual void CreateTexture3D_PreCallHook(const D3D11_TEXTURE3D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D)                                                                                                                                                                               {};

	virtual void CreateShaderResourceView_PreCallHook(ID3D11Resource* pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc, ID3D11ShaderResourceView** ppSRView)                                                                                                                                                                      {};
	virtual void CreateRenderTargetView_PreCallHook(ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc, ID3D11RenderTargetView** ppRTView)                                                                                                                                                                            {};
	virtual void CreateDepthStencilView_PreCallHook(ID3D11Resource* pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc, ID3D11DepthStencilView** ppDepthStencilView)                                                                                                                                                                  {};
	virtual void CreateUnorderedAccessView_PreCallHook(ID3D11Resource* pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc, ID3D11UnorderedAccessView** ppUAView)                                                                                                                                                                   {};

	virtual void CreateInputLayout_PreCallHook(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements, const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout** ppInputLayout)                                                                                                            {};

	virtual void CreateVertexShader_PreCallHook(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader)                                                                                                                                                              {};
	virtual void CreateGeometryShader_PreCallHook(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader)                                                                                                                                                        {};
	virtual void CreateGeometryShaderWithStreamOutput_PreCallHook(const void* pShaderBytecode, SIZE_T BytecodeLength, const D3D11_SO_DECLARATION_ENTRY* pSODeclaration, UINT NumEntries, const UINT* pBufferStrides, UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader) {};
	virtual void CreatePixelShader_PreCallHook(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader)                                                                                                                                                                 {};
	virtual void CreateHullShader_PreCallHook(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11HullShader** ppHullShader)                                                                                                                                                                    {};
	virtual void CreateDomainShader_PreCallHook(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11DomainShader** ppDomainShader)                                                                                                                                                              {};
	virtual void CreateComputeShader_PreCallHook(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11ComputeShader** ppComputeShader)                                                                                                                                                           {};

	virtual void CreateBlendState_PreCallHook(const D3D11_BLEND_DESC* pBlendStateDesc, ID3D11BlendState** ppBlendState)                                                                                                                                                                                                                  {};
	virtual void CreateDepthStencilState_PreCallHook(const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc, ID3D11DepthStencilState** ppDepthStencilState)                                                                                                                                                                                   {};
	virtual void CreateRasterizerState_PreCallHook(const D3D11_RASTERIZER_DESC* pRasterizerDesc, ID3D11RasterizerState** ppRasterizerState)                                                                                                                                                                                              {};
	virtual void CreateSamplerState_PreCallHook(const D3D11_SAMPLER_DESC* pSamplerDesc, ID3D11SamplerState** ppSamplerState)                                                                                                                                                                                                             {};

	virtual void CreateQuery_PreCallHook(const D3D11_QUERY_DESC* pQueryDesc, ID3D11Query** ppQuery)                                                                                                                                                                                                                                      {};
	virtual void CreatePredicate_PreCallHook(const D3D11_QUERY_DESC* pPredicateDesc, ID3D11Predicate** ppPredicate)                                                                                                                                                                                                                      {};

	virtual void CreateClassLinkage_PreCallHook(ID3D11ClassLinkage** ppLinkage)                                                                                                                                                                                                                                                          {};

	virtual void GetCreationFlags_PreCallHook()                                                                                                                                                                                                                                                                                          {};
	virtual void GetImmediateContext_PreCallHook(ID3D11DeviceContext** ppImmediateContext)                                                                                                                                                                                                                                               {};
	virtual void CreateDeferredContext_PreCallHook(UINT ContextFlags, ID3D11DeviceContext** ppDeferredContext)                                                                                                                                                                                                                           {};

	virtual void GetFeatureLevel_PreCallHook()                                                                                                                                                                                                                                                                                           {};
	virtual void GetNodeCount_PreCallHook()                                                                                                                                                                                                                                                                                              {};
	virtual void CheckFeatureSupport_PreCallHook(D3D11_FEATURE Feature, void* pFeatureSupportData, UINT FeatureSupportDataSize)                                                                                                                                                                                                          {};
	virtual void CheckFormatSupport_PreCallHook(DXGI_FORMAT Format, UINT* pFormatSupport)                                                                                                                                                                                                                                                {};
	virtual void CheckMultisampleQualityLevels_PreCallHook(DXGI_FORMAT Format, UINT SampleCount, UINT* pNumQualityLevels)                                                                                                                                                                                                                {};

	virtual void CheckCounterInfo_PreCallHook(D3D11_COUNTER_INFO* pCounterInfo)                                                                                                                                                                                                                                                          {};
	virtual void CreateCounter_PreCallHook(const D3D11_COUNTER_DESC* pCounterDesc, ID3D11Counter** ppCounter)                                                                                                                                                                                                                            {};
	virtual void CheckCounter_PreCallHook(const D3D11_COUNTER_DESC* pDesc, D3D11_COUNTER_TYPE* pType, UINT* pActiveCounters, LPSTR wszName, UINT* pNameLength, LPSTR wszUnits, UINT* pUnitsLength, LPSTR wszDescription, UINT* pDescriptionLength)                                                                                       {};

	virtual void OpenSharedResource_PreCallHook(HANDLE hResource, REFIID ReturnedInterface, void** ppResource)                                                                                                                                                                                                                           {};

	#if (CRY_RENDERER_DIRECT3D >= 111)
	virtual void GetImmediateContext1_PreCallHook(ID3D11DeviceContext1** ppImmediateContext) {}
	#endif

	// DX12-specific with DX11-fallback
	virtual void CreateTarget1D_PreCallHook(const D3D11_TEXTURE1D_DESC* pDesc, const FLOAT ClearValue[4], const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D) {};
	virtual void CreateTarget2D_PreCallHook(const D3D11_TEXTURE2D_DESC* pDesc, const FLOAT ClearValue[4], const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D) {};
	virtual void CreateTarget3D_PreCallHook(const D3D11_TEXTURE3D_DESC* pDesc, const FLOAT ClearValue[4], const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D) {};

	// DX12-only
	virtual void CreateNullResource_PreCallHook(D3D11_RESOURCE_DIMENSION eType, ID3D11Resource** ppNullResource)                    {}
	virtual void ReleaseNullResource_PreCallHook(ID3D11Resource* pNullResource)                                                     {}

	virtual void CreateStagingResource_PreCallHook(ID3D11Resource* pInputResource, ID3D11Resource** ppStagingResource, BOOL Upload) {}
	virtual void ReleaseStagingResource_PreCallHook(ID3D11Resource* pStagingResource)                                               {}

	/*********************************/
	/*** ID3DDeviceContext methods ***/
	virtual void QueryInterface_Context_PreCallHook(REFIID riid, void** ppvObj)                                                                                                                                                                                                                               {};
	virtual void AddRef_Context_PreCallHook()                                                                                                                                                                                                                                                                 {};
	virtual void Release_Context_PreCallHook()                                                                                                                                                                                                                                                                {};

	void         GetDevice_PreCallHook(ID3D11Device** ppDevice)                                                                                                                                                                                                                                               {}

	virtual void GetPrivateData_Context_PreCallHook(REFGUID guid, UINT* pDataSize, void* pData)                                                                                                                                                                                                               {}
	virtual void SetPrivateData_Context_PreCallHook(REFGUID guid, UINT DataSize, const void* pData)                                                                                                                                                                                                           {}
	virtual void SetPrivateDataInterface_Context_PreCallHook(REFGUID guid, const IUnknown* pData)                                                                                                                                                                                                             {}

	virtual void PSSetShader_PreCallHook(ID3D11PixelShader* pPixelShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                               {}
	virtual void PSGetShader_PreCallHook(ID3D11PixelShader** ppPixelShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                                 {}
	virtual void PSSetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           {}
	virtual void PSGetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 {}
	virtual void PSSetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    {}
	virtual void PSGetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          {}
	virtual void PSSetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      {}
	virtual void PSGetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            {}

	virtual void VSSetShader_PreCallHook(ID3D11VertexShader* pVertexShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                             {}
	virtual void VSGetShader_PreCallHook(ID3D11VertexShader** ppVertexShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                               {}
	virtual void VSSetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           {}
	virtual void VSGetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 {}
	virtual void VSSetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    {}
	virtual void VSGetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          {}
	virtual void VSSetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      {}
	virtual void VSGetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            {}

	virtual void GSSetShader_PreCallHook(ID3D11GeometryShader* pShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                                 {}
	virtual void GSGetShader_PreCallHook(ID3D11GeometryShader** ppGeometryShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                           {}
	virtual void GSSetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           {}
	virtual void GSGetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 {}
	virtual void GSSetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    {}
	virtual void GSGetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          {}
	virtual void GSSetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      {}
	virtual void GSGetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            {}

	virtual void HSSetShader_PreCallHook(ID3D11HullShader* pHullShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                                 {}
	virtual void HSGetShader_PreCallHook(ID3D11HullShader** ppHullShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                                   {}
	virtual void HSSetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           {}
	virtual void HSGetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 {}
	virtual void HSSetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    {}
	virtual void HSGetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          {}
	virtual void HSSetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      {}
	virtual void HSGetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            {}

	virtual void DSSetShader_PreCallHook(ID3D11DomainShader* pDomainShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                             {}
	virtual void DSGetShader_PreCallHook(ID3D11DomainShader** ppDomainShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                               {}
	virtual void DSSetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           {}
	virtual void DSGetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 {}
	virtual void DSSetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    {}
	virtual void DSGetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          {}
	virtual void DSSetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      {}
	virtual void DSGetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            {}

	virtual void CSSetShader_PreCallHook(ID3D11ComputeShader* pComputeShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                           {}
	virtual void CSGetShader_PreCallHook(ID3D11ComputeShader** ppComputeShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                             {}
	virtual void CSSetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           {}
	virtual void CSGetSamplers_PreCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 {}
	virtual void CSSetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    {}
	virtual void CSGetConstantBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          {}
	virtual void CSSetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      {}
	virtual void CSGetShaderResources_PreCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            {}
	virtual void CSSetUnorderedAccessViews_PreCallHook(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts)                                                                                                                                 {}
	virtual void CSGetUnorderedAccessViews_PreCallHook(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews)                                                                                                                                                                      {}

	virtual void DrawAuto_PreCallHook()                                                                                                                                                                                                                                                                       {}
	virtual void Draw_PreCallHook(UINT VertexCount, UINT StartVertexLocation)                                                                                                                                                                                                                                 {}
	virtual void DrawInstanced_PreCallHook(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)                                                                                                                                                             {}
	virtual void DrawInstancedIndirect_PreCallHook(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)                                                                                                                                                                                               {}
	virtual void DrawIndexed_PreCallHook(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)                                                                                                                                                                                                    {}
	virtual void DrawIndexedInstanced_PreCallHook(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)                                                                                                                                {}
	virtual void DrawIndexedInstancedIndirect_PreCallHook(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)                                                                                                                                                                                        {}

	virtual void Map_PreCallHook(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource)                                                                                                                                                    {}
	virtual void Unmap_PreCallHook(ID3D11Resource* pResource, UINT Subresource)                                                                                                                                                                                                                               {}

	virtual void IASetInputLayout_PreCallHook(ID3D11InputLayout* pInputLayout)                                                                                                                                                                                                                                {}
	virtual void IAGetInputLayout_PreCallHook(ID3D11InputLayout** ppInputLayout)                                                                                                                                                                                                                              {}
	virtual void IASetVertexBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const UINT* pStrides, const UINT* pOffsets)                                                                                                                                            {}
	virtual void IAGetVertexBuffers_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppVertexBuffers, UINT* pStrides, UINT* pOffsets)                                                                                                                                                              {}
	virtual void IASetIndexBuffer_PreCallHook(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, UINT Offset)                                                                                                                                                                                                    {}
	virtual void IAGetIndexBuffer_PreCallHook(ID3D11Buffer** pIndexBuffer, DXGI_FORMAT* Format, UINT* Offset)                                                                                                                                                                                                 {}
	virtual void IASetPrimitiveTopology_PreCallHook(D3D11_PRIMITIVE_TOPOLOGY Topology)                                                                                                                                                                                                                        {}
	virtual void IAGetPrimitiveTopology_PreCallHook(D3D11_PRIMITIVE_TOPOLOGY* pTopology)                                                                                                                                                                                                                      {}

	virtual void Begin_PreCallHook(ID3D11Asynchronous* pAsync)                                                                                                                                                                                                                                                {}
	virtual void End_PreCallHook(ID3D11Asynchronous* pAsync)                                                                                                                                                                                                                                                  {}

	virtual void GetData_PreCallHook(ID3D11Asynchronous* pAsync, void* pData, UINT DataSize, UINT GetDataFlags)                                                                                                                                                                                               {}
	virtual void SetPredication_PreCallHook(ID3D11Predicate* pPredicate, BOOL PredicateValue)                                                                                                                                                                                                                 {}
	virtual void GetPredication_PreCallHook(ID3D11Predicate** ppPredicate, BOOL* pPredicateValue)                                                                                                                                                                                                             {}

	virtual void OMSetRenderTargets_PreCallHook(UINT NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView)                                                                                                                                                 {}
	virtual void OMGetRenderTargets_PreCallHook(UINT NumViews, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView)                                                                                                                                                     {}
	virtual void OMSetRenderTargetsAndUnorderedAccessViews_PreCallHook(UINT NumRTVs, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts) {}
	virtual void OMGetRenderTargetsAndUnorderedAccessViews_PreCallHook(UINT NumRTVs, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews)                                          {}
	virtual void OMSetBlendState_PreCallHook(ID3D11BlendState* pBlendState, const FLOAT BlendFactor[4], UINT SampleMask)                                                                                                                                                                                      {}
	virtual void OMGetBlendState_PreCallHook(ID3D11BlendState** ppBlendState, FLOAT BlendFactor[4], UINT* pSampleMask)                                                                                                                                                                                        {}
	virtual void OMSetDepthStencilState_PreCallHook(ID3D11DepthStencilState* pDepthStencilState, UINT StencilRef)                                                                                                                                                                                             {}
	virtual void OMGetDepthStencilState_PreCallHook(ID3D11DepthStencilState** ppDepthStencilState, UINT* pStencilRef)                                                                                                                                                                                         {}

	virtual void SOSetTargets_PreCallHook(UINT NumBuffers, ID3D11Buffer* const* ppSOTargets, const UINT* pOffsets)                                                                                                                                                                                            {}
	virtual void SOGetTargets_PreCallHook(UINT NumBuffers, ID3D11Buffer** ppSOTargets)                                                                                                                                                                                                                        {}

	virtual void Dispatch_PreCallHook(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)                                                                                                                                                                                                 {}
	virtual void DispatchIndirect_PreCallHook(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)                                                                                                                                                                                                    {}

	virtual void RSSetState_PreCallHook(ID3D11RasterizerState* pRasterizerState)                                                                                                                                                                                                                              {}
	virtual void RSGetState_PreCallHook(ID3D11RasterizerState** ppRasterizerState)                                                                                                                                                                                                                            {}
	virtual void RSSetViewports_PreCallHook(UINT NumViewports, const D3D11_VIEWPORT* pViewports)                                                                                                                                                                                                              {}
	virtual void RSGetViewports_PreCallHook(UINT* pNumViewports, D3D11_VIEWPORT* pViewports)                                                                                                                                                                                                                  {}
	virtual void RSSetScissorRects_PreCallHook(UINT NumRects, const D3D11_RECT* pRects)                                                                                                                                                                                                                       {}
	virtual void RSGetScissorRects_PreCallHook(UINT* pNumRects, D3D11_RECT* pRects)                                                                                                                                                                                                                           {}

	virtual void CopyResource_PreCallHook(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource)                                                                                                                                                                                                         {}
	virtual void CopySubresourceRegion_PreCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox) {}
	virtual void CopySubresourcesRegion_PreCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT NumSubresources) {}
	virtual void UpdateSubresource_PreCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)                                                                                                                       {}

	virtual void CopyStructureCount_PreCallHook(ID3D11Buffer* pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView* pSrcView)                                                                                                                                                                     {}

	virtual void ClearRenderTargetView_PreCallHook(ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4])                                                                                                                                                                                       {}
	virtual void ClearUnorderedAccessViewUint_PreCallHook(ID3D11UnorderedAccessView* pUnorderedAccessView, const UINT Values[4])                                                                                                                                                                              {}
	virtual void ClearUnorderedAccessViewFloat_PreCallHook(ID3D11UnorderedAccessView* pUnorderedAccessView, const FLOAT Values[4])                                                                                                                                                                            {}
	virtual void ClearDepthStencilView_PreCallHook(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)                                                                                                                                                                    {}

	virtual void GenerateMips_PreCallHook(ID3D11ShaderResourceView* pShaderResourceView)                                                                                                                                                                                                                      {}

	virtual void SetResourceMinLOD_PreCallHook(ID3D11Resource* pResource, FLOAT MinLOD)                                                                                                                                                                                                                       {}
	virtual void GetResourceMinLOD_PreCallHook(ID3D11Resource* pResource)                                                                                                                                                                                                                                     {}

	virtual void ResolveSubresource_PreCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, ID3D11Resource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)                                                                                                                                     {}

	#if !CRY_PLATFORM_ORBIS
	virtual void ExecuteCommandList_PreCallHook(ID3D11CommandList* pCommandList, BOOL RestoreContextState)          {}
	virtual void FinishCommandList_PreCallHook(BOOL RestoreDeferredContextState, ID3D11CommandList** ppCommandList) {}
	#endif

	virtual void ClearState_PreCallHook()      {}
	virtual void Flush_PreCallHook()           {}

	virtual void GetType_PreCallHook()         {}
	virtual void GetContextFlags_PreCallHook() {}

	#if (CRY_RENDERER_DIRECT3D >= 111)
	virtual void CopySubresourceRegion1_PreCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT CopyFlags) {}
	virtual void CopySubresourcesRegion1_PreCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT CopyFlags, UINT NumSubresources) {}
	virtual void UpdateSubresource1_PreCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags) {}
	virtual void DiscardResource_PreCallHook(ID3D11Resource* pResource) {}
	virtual void DiscardView_PreCallHook(ID3D11View* pResourceView) {}
	#endif

	#if (CRY_RENDERER_DIRECT3D >= 111)
	virtual void VSSetConstantBuffers1_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants) {}
	virtual void HSSetConstantBuffers1_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants) {}
	virtual void DSSetConstantBuffers1_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants) {}
	virtual void GSSetConstantBuffers1_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants) {}
	virtual void PSSetConstantBuffers1_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants) {}
	virtual void CSSetConstantBuffers1_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants) {}
	#endif

	#if (CRY_RENDERER_DIRECT3D >= 111)
	virtual void VSGetConstantBuffers1_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants) {}
	virtual void HSGetConstantBuffers1_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants) {}
	virtual void DSGetConstantBuffers1_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants) {}
	virtual void GSGetConstantBuffers1_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants) {}
	virtual void PSGetConstantBuffers1_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants) {}
	virtual void CSGetConstantBuffers1_PreCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants) {}

	virtual void SwapDeviceContextState_PreCallHook(ID3DDeviceContextState* pState, ID3DDeviceContextState** ppPreviousState)                                    {}
	virtual void ClearView_PreCallHook(ID3D11View* pView, const FLOAT Color[4], const D3D11_RECT* pRect, UINT NumRects)                                          {}
	virtual void DiscardView1_PreCallHook(ID3D11View* pResourceView, const D3D11_RECT* pRects, UINT NumRects)                                                    {}
	#endif

	virtual void ClearRectsRenderTargetView_PreCallHook(ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D11_RECT* pRects)                                                                                                {}
	virtual void ClearRectsUnorderedAccessViewUint_PreCallHook(ID3D11UnorderedAccessView* pUnorderedAccessView, const UINT Values[4], UINT NumRects, const D3D11_RECT* pRects)                                                                                       {}
	virtual void ClearRectsUnorderedAccessViewFloat_PreCallHook(ID3D11UnorderedAccessView* pUnorderedAccessView, const FLOAT Values[4], UINT NumRects, const D3D11_RECT* pRects)                                                                                     {}
	virtual void ClearRectsDepthStencilView_PreCallHook(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D11_RECT* pRects)                                                                             {}

	virtual void CopyResourceOvercross_PreCallHook(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource) {}
	virtual void JoinSubresourceRegion_PreCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResourceL, ID3D11Resource* pSrcResourceR, UINT SrcSubresource, const D3D11_BOX* pSrcBox)                  {}
	#if (CRY_RENDERER_DIRECT3D >= 111)
	virtual void CopyResourceOvercross1_PreCallHook(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource, UINT CopyFlags) {}
	virtual void JoinSubresourceRegion1_PreCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResourceL, ID3D11Resource* pSrcResourceR, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT CopyFlags) {}
	#endif

	virtual void CopyStagingResource_PreCallHook(ID3D11Resource* pStagingResource, ID3D11Resource* pSourceResource, UINT SubResource, BOOL Upload) {}
	virtual void TestStagingResource_PreCallHook(ID3D11Resource* pStagingResource)                                                                 {}
	virtual void WaitStagingResource_PreCallHook(ID3D11Resource* pStagingResource)                                                                 {}
	virtual void MapStagingResource_PreCallHook(ID3D11Resource* pStagingResource, BOOL Upload, void** ppStagingMemory)                             {}
	virtual void UnmapStagingResource_PreCallHook(ID3D11Resource* pStagingResource, BOOL Upload)                                                   {}

	virtual void MappedWriteToSubresource_PreCallHook(ID3D11Resource* pResource, UINT Subresource, SIZE_T Offset, SIZE_T Size, D3D11_MAP MapType, const void* pData, UINT numDataBlocks) {}
	virtual void MappedReadFromSubresource_PreCallHook(ID3D11Resource* pResource, UINT Subresource, SIZE_T Offset, SIZE_T Size, D3D11_MAP MapType, void* pData, UINT numDataBlocks) {}

	#if defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE)
	/*************************************/
	/*** ID3DPerformanceDevice methods ***/
	virtual void QueryInterface_PerformanceDevice_PreCallHook(REFIID riid, void** ppvObj)                                                                                         {};
	virtual void AddRef_PerformanceDevice_PreCallHook()                                                                                                                           {};
	virtual void Release_PerformanceDevice_PreCallHook()                                                                                                                          {};

	virtual void CreateCounterSet_PreCallHook(const D3D11X_COUNTER_SET_DESC* pCounterSetDesc, ID3DXboxCounterSet** ppCounterSet)                                                  {}
	virtual void CreateCounterSample_PreCallHook(ID3DXboxCounterSample** ppCounterSample)                                                                                         {}

	virtual void SetDriverHint_PreCallHook(UINT Feature, UINT Value)                                                                                                              {}
	virtual void CreateDmaEngineContext_PreCallHook(const D3D11_DMA_ENGINE_CONTEXT_DESC* pDmaEngineContextDesc, ID3D11DmaEngineContextX** ppDmaDeviceContext)                     {}

	virtual void IsFencePending_PreCallHook(UINT64 Fence)                                                                                                                         {}
	virtual void IsResourcePending_PreCallHook(ID3D11Resource* pResource)                                                                                                         {}

	virtual void CreatePlacementBuffer_PreCallHook(const D3D11_BUFFER_DESC* pDesc, void* pCpuVirtualAddress, ID3D11Buffer** ppBuffer)                                             {}
	virtual void CreatePlacementTexture1D_PreCallHook(const D3D11_TEXTURE1D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture1D** ppTexture1D) {}
	virtual void CreatePlacementTexture2D_PreCallHook(const D3D11_TEXTURE2D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture2D** ppTexture2D) {}
	virtual void CreatePlacementTexture3D_PreCallHook(const D3D11_TEXTURE3D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture3D** ppTexture3D) {}

	/********************************************/
	/*** ID3DPerformanceDeviceContext methods ***/
	virtual void QueryInterface_PerformanceContext_PreCallHook(REFIID riid, void** ppvObj)                                              {};
	virtual void AddRef_PerformanceContext_PreCallHook()                                                                                {};
	virtual void Release_PerformanceContext_PreCallHook()                                                                               {};

	virtual void FlushGpuCaches_PreCallHook(ID3D11Resource* pResource)                                                                  {}
	virtual void FlushGpuCacheRange_PreCallHook(UINT Flags, void* pBaseAddress, SIZE_T SizeInBytes)                                     {}

#ifndef CRY_PLATFORM_DURANGO
	virtual void InsertFence_PreCallHook()                                                                                              {}
#endif
	virtual void InsertWaitUntilIdle_PreCallHook(UINT Flags)                                                                            {}

	virtual void RemapConstantBufferInheritance_PreCallHook(D3D11_STAGE Stage, UINT Slot, D3D11_STAGE InheritStage, UINT InheritSlot)   {}
	virtual void RemapShaderResourceInheritance_PreCallHook(D3D11_STAGE Stage, UINT Slot, D3D11_STAGE InheritStage, UINT InheritSlot)   {}
	virtual void RemapSamplerInheritance_PreCallHook(D3D11_STAGE Stage, UINT Slot, D3D11_STAGE InheritStage, UINT InheritSlot)          {}
	virtual void RemapVertexBufferInheritance_PreCallHook(UINT Slot, UINT InheritSlot)                                                  {}

	virtual void PSSetFastConstantBuffer_PreCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer)                                          {}
	virtual void PSSetFastShaderResource_PreCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)                          {}
	virtual void PSSetFastSampler_PreCallHook(UINT Slot, ID3D11SamplerState* pSampler)                                                  {}

	virtual void VSSetFastConstantBuffer_PreCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer)                                          {}
	virtual void VSSetFastShaderResource_PreCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)                          {}
	virtual void VSSetFastSampler_PreCallHook(UINT Slot, ID3D11SamplerState* pSampler)                                                  {}

	virtual void GSSetFastConstantBuffer_PreCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer)                                          {}
	virtual void GSSetFastShaderResource_PreCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)                          {}
	virtual void GSSetFastSampler_PreCallHook(UINT Slot, ID3D11SamplerState* pSampler)                                                  {}

	virtual void CSSetFastConstantBuffer_PreCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer)                                          {}
	virtual void CSSetFastShaderResource_PreCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)                          {}
	virtual void CSSetFastSampler_PreCallHook(UINT Slot, ID3D11SamplerState* pSampler)                                                  {}

	virtual void HSSetFastConstantBuffer_PreCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer)                                          {}
	virtual void HSSetFastShaderResource_PreCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)                          {}
	virtual void HSSetFastSampler_PreCallHook(UINT Slot, ID3D11SamplerState* pSampler)                                                  {}

	virtual void DSSetFastConstantBuffer_PreCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer)                                          {}
	virtual void DSSetFastShaderResource_PreCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)                          {}
	virtual void DSSetFastSampler_PreCallHook(UINT Slot, ID3D11SamplerState* pSampler)                                                  {}

	virtual void IASetFastVertexBuffer_PreCallHook(UINT Slot, ID3D11Buffer* pVertexBuffer, UINT Stride)                                 {}
	virtual void IASetFastIndexBuffer_PreCallHook(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format)                                       {}

	virtual void PSSetPlacementConstantBuffer_PreCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)                 {}
	virtual void PSSetPlacementShaderResource_PreCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress) {}

	virtual void VSSetPlacementConstantBuffer_PreCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)                 {}
	virtual void VSSetPlacementShaderResource_PreCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress) {}

	virtual void GSSetPlacementConstantBuffer_PreCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)                 {}
	virtual void GSSetPlacementShaderResource_PreCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress) {}

	virtual void CSSetPlacementConstantBuffer_PreCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)                 {}
	virtual void CSSetPlacementShaderResource_PreCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress) {}

	virtual void HSSetPlacementConstantBuffer_PreCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)                 {}
	virtual void HSSetPlacementShaderResource_PreCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress) {}

	virtual void DSSetPlacementConstantBuffer_PreCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)                 {}
	virtual void DSSetPlacementShaderResource_PreCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress) {}

	virtual void IASetPlacementVertexBuffer_PreCallHook(UINT Slot, ID3D11Buffer* pVertexBuffer, void* pBaseAddress, UINT Stride)        {}
	virtual void IASetPlacementIndexBuffer_PreCallHook(ID3D11Buffer* pIndexBuffer, void* pBaseAddress, DXGI_FORMAT Format)              {}

	virtual void PIXGpuCaptureNextFrame_PreCallHook(UINT Flags, LPCWSTR lpOutputFileName)                                               {}
	virtual void PIXGpuBeginCapture_PreCallHook(UINT Flags, LPCWSTR lpOutputFileName)                                                   {}
	virtual void PIXGpuEndCapture_PreCallHook()                                                                                         {}

	virtual void InsertFence_PreCallHook(UINT Flags)                                                                                    {}
	virtual void InsertWaitOnFence_PreCallHook(UINT Flags, UINT64 Fence)                                                                {}

	virtual void PIXBeginEvent_PreCallHook(LPCWSTR Name)                                                                                {}
	virtual void PIXEndEvent_PreCallHook()                                                                                              {}
	virtual void PIXSetMarker_PreCallHook(LPCWSTR Name)                                                                                 {}
	virtual void PIXGetStatus_PreCallHook()                                                                                             {}

	virtual void StartCounters_PreCallHook(ID3D11CounterSetX* pCounterSet)                                                              {}
	virtual void SampleCounters_PreCallHook(ID3D11CounterSampleX* pCounterSample)                                                       {}
	virtual void StopCounters_PreCallHook()                                                                                             {}
	virtual void GetCounterData_PreCallHook(ID3D11CounterSampleX* pCounterSample, D3D11X_COUNTER_DATA* pData, UINT GetCounterDataFlags) {}

	virtual void HSSetTessellationParameters_PreCallHook(const D3D11X_TESSELLATION_PARAMETERS* pTessellationParameters)                 {}
	virtual void HSGetLastUsedTessellationParameters_PreCallHook(D3D11X_TESSELLATION_PARAMETERS* pTessellationParameters)               {}
	#endif   // DEVICE_SUPPORTS_PERFORMANCE_DEVICE

	///////////////////////////////////////////////////////////////////////////////
	// post function call hooks, those are called after the actual device function is invoked
	// the first parameter is the return value from the device call
	///////////////////////////////////////////////////////////////////////////////

	/**************************/
	/*** ID3DDevice methods ***/
	virtual void QueryInterface_Device_PostCallHook(HRESULT hr, REFIID riid, void** ppvObj)                                                                                                                                                                                                                                                           {};
	virtual void AddRef_Device_PostCallHook(ULONG nResult)                                                                                                                                                                                                                                                                                            {};
	virtual void Release_Device_PostCallHook(ULONG nResult)                                                                                                                                                                                                                                                                                           {};

	virtual void GetDeviceRemovedReason_PostCallHook(HRESULT hr)                                                                                                                                                                                                                                                                                      {};

	virtual void SetExceptionMode_PostCallHook(HRESULT hr, UINT RaiseFlags)                                                                                                                                                                                                                                                                           {};
	virtual void GetExceptionMode_PostCallHook(UINT nResult)                                                                                                                                                                                                                                                                                          {};

	virtual void GetPrivateData_Device_PostCallHook(HRESULT hr, REFGUID guid, UINT* pDataSize, void* pData)                                                                                                                                                                                                                                           {};
	virtual void SetPrivateData_Device_PostCallHook(HRESULT hr, REFGUID guid, UINT DataSize, const void* pData)                                                                                                                                                                                                                                       {};
	virtual void SetPrivateDataInterface_Device_PostCallHook(HRESULT hr, REFGUID guid, const IUnknown* pData)                                                                                                                                                                                                                                         {};

	virtual void CreateBuffer_PostCallHook(HRESULT hr, const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer)                                                                                                                                                                                           {};

	virtual void CreateTexture1D_PostCallHook(HRESULT hr, const D3D11_TEXTURE1D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D)                                                                                                                                                                               {};
	virtual void CreateTexture2D_PostCallHook(HRESULT hr, const D3D11_TEXTURE2D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D)                                                                                                                                                                               {};
	virtual void CreateTexture3D_PostCallHook(HRESULT hr, const D3D11_TEXTURE3D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D)                                                                                                                                                                               {};

	virtual void CreateShaderResourceView_PostCallHook(HRESULT hr, ID3D11Resource* pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc, ID3D11ShaderResourceView** ppSRView)                                                                                                                                                                      {};
	virtual void CreateRenderTargetView_PostCallHook(HRESULT hr, ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc, ID3D11RenderTargetView** ppRTView)                                                                                                                                                                            {};
	virtual void CreateDepthStencilView_PostCallHook(HRESULT hr, ID3D11Resource* pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc, ID3D11DepthStencilView** ppDepthStencilView)                                                                                                                                                                  {};
	virtual void CreateUnorderedAccessView_PostCallHook(HRESULT hr, ID3D11Resource* pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc, ID3D11UnorderedAccessView** ppUAView)                                                                                                                                                                   {};

	virtual void CreateInputLayout_PostCallHook(HRESULT hr, const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements, const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout** ppInputLayout)                                                                                                            {};

	virtual void CreateVertexShader_PostCallHook(HRESULT hr, const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader)                                                                                                                                                              {};
	virtual void CreateGeometryShader_PostCallHook(HRESULT hr, const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader)                                                                                                                                                        {};
	virtual void CreateGeometryShaderWithStreamOutput_PostCallHook(HRESULT hr, const void* pShaderBytecode, SIZE_T BytecodeLength, const D3D11_SO_DECLARATION_ENTRY* pSODeclaration, UINT NumEntries, const UINT* pBufferStrides, UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader) {};
	virtual void CreatePixelShader_PostCallHook(HRESULT hr, const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader)                                                                                                                                                                 {};
	virtual void CreateHullShader_PostCallHook(HRESULT hr, const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11HullShader** ppHullShader)                                                                                                                                                                    {};
	virtual void CreateDomainShader_PostCallHook(HRESULT hr, const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11DomainShader** ppDomainShader)                                                                                                                                                              {};
	virtual void CreateComputeShader_PostCallHook(HRESULT hr, const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11ComputeShader** ppComputeShader)                                                                                                                                                           {};

	virtual void CreateBlendState_PostCallHook(HRESULT hr, const D3D11_BLEND_DESC* pBlendStateDesc, ID3D11BlendState** ppBlendState)                                                                                                                                                                                                                  {};
	virtual void CreateDepthStencilState_PostCallHook(HRESULT hr, const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc, ID3D11DepthStencilState** ppDepthStencilState)                                                                                                                                                                                   {};
	virtual void CreateRasterizerState_PostCallHook(HRESULT hr, const D3D11_RASTERIZER_DESC* pRasterizerDesc, ID3D11RasterizerState** ppRasterizerState)                                                                                                                                                                                              {};
	virtual void CreateSamplerState_PostCallHook(HRESULT hr, const D3D11_SAMPLER_DESC* pSamplerDesc, ID3D11SamplerState** ppSamplerState)                                                                                                                                                                                                             {};

	virtual void CreateQuery_PostCallHook(HRESULT hr, const D3D11_QUERY_DESC* pQueryDesc, ID3D11Query** ppQuery)                                                                                                                                                                                                                                      {};
	virtual void CreatePredicate_PostCallHook(HRESULT hr, const D3D11_QUERY_DESC* pPredicateDesc, ID3D11Predicate** ppPredicate)                                                                                                                                                                                                                      {};

	virtual void CreateClassLinkage_PostCallHook(HRESULT hr, ID3D11ClassLinkage** ppLinkage)                                                                                                                                                                                                                                                          {};

	virtual void GetCreationFlags_PostCallHook(UINT nResult)                                                                                                                                                                                                                                                                                          {};
	virtual void GetImmediateContext_PostCallHook(ID3D11DeviceContext** ppImmediateContext)                                                                                                                                                                                                                                                           {};
	virtual void CreateDeferredContext_PostCallHook(HRESULT hr, UINT ContextFlags, ID3D11DeviceContext** ppDeferredContext)                                                                                                                                                                                                                           {};

	virtual void GetFeatureLevel_PostCallHook(D3D_FEATURE_LEVEL nResult)                                                                                                                                                                                                                                                                              {};
	virtual void GetNodeCount_PostCallHook(UINT nResult)                                                                                                                                                                                                                                                                                              {};
	virtual void CheckFeatureSupport_PostCallHook(HRESULT hr, D3D11_FEATURE Feature, void* pFeatureSupportData, UINT FeatureSupportDataSize)                                                                                                                                                                                                          {};
	virtual void CheckFormatSupport_PostCallHook(HRESULT hr, DXGI_FORMAT Format, UINT* pFormatSupport)                                                                                                                                                                                                                                                {};
	virtual void CheckMultisampleQualityLevels_PostCallHook(HRESULT hr, DXGI_FORMAT Format, UINT SampleCount, UINT* pNumQualityLevels)                                                                                                                                                                                                                {};

	virtual void CheckCounterInfo_PostCallHook(D3D11_COUNTER_INFO* pCounterInfo)                                                                                                                                                                                                                                                                      {};
	virtual void CreateCounter_PostCallHook(HRESULT hr, const D3D11_COUNTER_DESC* pCounterDesc, ID3D11Counter** ppCounter)                                                                                                                                                                                                                            {};
	virtual void CheckCounter_PostCallHook(HRESULT hr, const D3D11_COUNTER_DESC* pDesc, D3D11_COUNTER_TYPE* pType, UINT* pActiveCounters, LPSTR wszName, UINT* pNameLength, LPSTR wszUnits, UINT* pUnitsLength, LPSTR wszDescription, UINT* pDescriptionLength)                                                                                       {};

	virtual void OpenSharedResource_PostCallHook(HRESULT hr, HANDLE hResource, REFIID ReturnedInterface, void** ppResource)                                                                                                                                                                                                                           {};

	#if (CRY_RENDERER_DIRECT3D >= 111)
	virtual void GetImmediateContext1_PostCallHook(ID3D11DeviceContext1** ppImmediateContext) {}
	#endif

	// DX12-specific with DX11-fallback
	virtual void CreateTarget1D_PostCallHook(HRESULT hr, const D3D11_TEXTURE1D_DESC* pDesc, const FLOAT ClearValue[4], const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D) {};
	virtual void CreateTarget2D_PostCallHook(HRESULT hr, const D3D11_TEXTURE2D_DESC* pDesc, const FLOAT ClearValue[4], const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D) {};
	virtual void CreateTarget3D_PostCallHook(HRESULT hr, const D3D11_TEXTURE3D_DESC* pDesc, const FLOAT ClearValue[4], const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D) {};

	// DX12-only
	virtual void CreateNullResource_PostCallHook(HRESULT hr, D3D11_RESOURCE_DIMENSION eType, ID3D11Resource** ppNullResource)                    {}
	virtual void ReleaseNullResource_PostCallHook(HRESULT hr, ID3D11Resource* pNullResource)                                                     {}

	virtual void CreateStagingResource_PostCallHook(HRESULT hr, ID3D11Resource* pInputResource, ID3D11Resource** ppStagingResource, BOOL Upload) {}
	virtual void ReleaseStagingResource_PostCallHook(HRESULT hr, ID3D11Resource* pStagingResource)                                               {}

	/*********************************/
	/*** ID3DDeviceContext methods ***/
	virtual void QueryInterface_Context_PostCallHook(HRESULT hr, REFIID riid, void** ppvObj)                                                                                                                                                                                                                   {};
	virtual void AddRef_Context_PostCallHook(ULONG nResult)                                                                                                                                                                                                                                                    {};
	virtual void Release_Context_PostCallHook(ULONG nResult)                                                                                                                                                                                                                                                   {};

	virtual void GetDevice_PostCallHook(ID3D11Device** ppDevice)                                                                                                                                                                                                                                               {}

	virtual void GetPrivateData_Context_PostCallHook(HRESULT hr, REFGUID guid, UINT* pDataSize, void* pData)                                                                                                                                                                                                   {}
	virtual void SetPrivateData_Context_PostCallHook(HRESULT hr, REFGUID guid, UINT DataSize, const void* pData)                                                                                                                                                                                               {}
	virtual void SetPrivateDataInterface_Context_PostCallHook(HRESULT hr, REFGUID guid, const IUnknown* pData)                                                                                                                                                                                                 {}

	virtual void PSSetShader_PostCallHook(ID3D11PixelShader* pPixelShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                               {}
	virtual void PSGetShader_PostCallHook(ID3D11PixelShader** ppPixelShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                                 {}
	virtual void PSSetSamplers_PostCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           {}
	virtual void PSGetSamplers_PostCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 {}
	virtual void PSSetConstantBuffers_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    {}
	virtual void PSGetConstantBuffers_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          {}
	virtual void PSSetShaderResources_PostCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      {}
	virtual void PSGetShaderResources_PostCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            {}

	virtual void VSSetShader_PostCallHook(ID3D11VertexShader* pVertexShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                             {}
	virtual void VSGetShader_PostCallHook(ID3D11VertexShader** ppVertexShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                               {}
	virtual void VSSetSamplers_PostCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           {}
	virtual void VSGetSamplers_PostCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 {}
	virtual void VSSetConstantBuffers_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    {}
	virtual void VSGetConstantBuffers_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          {}
	virtual void VSSetShaderResources_PostCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      {}
	virtual void VSGetShaderResources_PostCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            {}

	virtual void GSSetShader_PostCallHook(ID3D11GeometryShader* pShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                                 {}
	virtual void GSGetShader_PostCallHook(ID3D11GeometryShader** ppGeometryShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                           {}
	virtual void GSSetSamplers_PostCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           {}
	virtual void GSGetSamplers_PostCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 {}
	virtual void GSSetConstantBuffers_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    {}
	virtual void GSGetConstantBuffers_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          {}
	virtual void GSSetShaderResources_PostCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      {}
	virtual void GSGetShaderResources_PostCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            {}

	virtual void HSSetShader_PostCallHook(ID3D11HullShader* pHullShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                                 {}
	virtual void HSGetShader_PostCallHook(ID3D11HullShader** ppHullShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                                   {}
	virtual void HSSetSamplers_PostCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           {}
	virtual void HSGetSamplers_PostCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 {}
	virtual void HSSetConstantBuffers_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    {}
	virtual void HSGetConstantBuffers_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          {}
	virtual void HSSetShaderResources_PostCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      {}
	virtual void HSGetShaderResources_PostCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            {}

	virtual void DSSetShader_PostCallHook(ID3D11DomainShader* pDomainShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                             {}
	virtual void DSGetShader_PostCallHook(ID3D11DomainShader** ppDomainShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                               {}
	virtual void DSSetSamplers_PostCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           {}
	virtual void DSGetSamplers_PostCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 {}
	virtual void DSSetConstantBuffers_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    {}
	virtual void DSGetConstantBuffers_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          {}
	virtual void DSSetShaderResources_PostCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      {}
	virtual void DSGetShaderResources_PostCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            {}

	virtual void CSSetShader_PostCallHook(ID3D11ComputeShader* pComputeShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                                                                                                                                           {}
	virtual void CSGetShader_PostCallHook(ID3D11ComputeShader** ppComputeShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)                                                                                                                                                             {}
	virtual void CSSetSamplers_PostCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           {}
	virtual void CSGetSamplers_PostCallHook(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)                                                                                                                                                                                                 {}
	virtual void CSSetConstantBuffers_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                                                                                                                                                    {}
	virtual void CSGetConstantBuffers_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)                                                                                                                                                                                          {}
	virtual void CSSetShaderResources_PostCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      {}
	virtual void CSGetShaderResources_PostCallHook(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)                                                                                                                                                                            {}
	virtual void CSSetUnorderedAccessViews_PostCallHook(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts)                                                                                                                                 {}
	virtual void CSGetUnorderedAccessViews_PostCallHook(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews)                                                                                                                                                                      {}

	virtual void DrawAuto_PostCallHook()                                                                                                                                                                                                                                                                       {}
	virtual void Draw_PostCallHook(UINT VertexCount, UINT StartVertexLocation)                                                                                                                                                                                                                                 {}
	virtual void DrawInstanced_PostCallHook(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)                                                                                                                                                             {}
	virtual void DrawInstancedIndirect_PostCallHook(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)                                                                                                                                                                                               {}
	virtual void DrawIndexed_PostCallHook(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)                                                                                                                                                                                                    {}
	virtual void DrawIndexedInstanced_PostCallHook(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)                                                                                                                                {}
	virtual void DrawIndexedInstancedIndirect_PostCallHook(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)                                                                                                                                                                                        {}

	virtual void Map_PostCallHook(HRESULT hr, ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource)                                                                                                                                        {}
	virtual void Unmap_PostCallHook(ID3D11Resource* pResource, UINT Subresource)                                                                                                                                                                                                                               {}

	virtual void IASetInputLayout_PostCallHook(ID3D11InputLayout* pInputLayout)                                                                                                                                                                                                                                {}
	virtual void IAGetInputLayout_PostCallHook(ID3D11InputLayout** ppInputLayout)                                                                                                                                                                                                                              {}
	virtual void IASetVertexBuffers_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const UINT* pStrides, const UINT* pOffsets)                                                                                                                                            {}
	virtual void IAGetVertexBuffers_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppVertexBuffers, UINT* pStrides, UINT* pOffsets)                                                                                                                                                              {}
	virtual void IASetIndexBuffer_PostCallHook(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, UINT Offset)                                                                                                                                                                                                    {}
	virtual void IAGetIndexBuffer_PostCallHook(ID3D11Buffer** pIndexBuffer, DXGI_FORMAT* Format, UINT* Offset)                                                                                                                                                                                                 {}
	virtual void IASetPrimitiveTopology_PostCallHook(D3D11_PRIMITIVE_TOPOLOGY Topology)                                                                                                                                                                                                                        {}
	virtual void IAGetPrimitiveTopology_PostCallHook(D3D11_PRIMITIVE_TOPOLOGY* pTopology)                                                                                                                                                                                                                      {}

	virtual void Begin_PostCallHook(ID3D11Asynchronous* pAsync)                                                                                                                                                                                                                                                {}
	virtual void End_PostCallHook(ID3D11Asynchronous* pAsync)                                                                                                                                                                                                                                                  {}

	virtual void GetData_PostCallHook(HRESULT hr, ID3D11Asynchronous* pAsync, void* pData, UINT DataSize, UINT GetDataFlags)                                                                                                                                                                                   {}
	virtual void SetPredication_PostCallHook(ID3D11Predicate* pPredicate, BOOL PredicateValue)                                                                                                                                                                                                                 {}
	virtual void GetPredication_PostCallHook(ID3D11Predicate** ppPredicate, BOOL* pPredicateValue)                                                                                                                                                                                                             {}

	virtual void OMSetRenderTargets_PostCallHook(UINT NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView)                                                                                                                                                 {}
	virtual void OMGetRenderTargets_PostCallHook(UINT NumViews, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView)                                                                                                                                                     {}
	virtual void OMSetRenderTargetsAndUnorderedAccessViews_PostCallHook(UINT NumRTVs, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts) {}
	virtual void OMGetRenderTargetsAndUnorderedAccessViews_PostCallHook(UINT NumRTVs, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews)                                          {}
	virtual void OMSetBlendState_PostCallHook(ID3D11BlendState* pBlendState, const FLOAT BlendFactor[4], UINT SampleMask)                                                                                                                                                                                      {}
	virtual void OMGetBlendState_PostCallHook(ID3D11BlendState** ppBlendState, FLOAT BlendFactor[4], UINT* pSampleMask)                                                                                                                                                                                        {}
	virtual void OMSetDepthStencilState_PostCallHook(ID3D11DepthStencilState* pDepthStencilState, UINT StencilRef)                                                                                                                                                                                             {}
	virtual void OMGetDepthStencilState_PostCallHook(ID3D11DepthStencilState** ppDepthStencilState, UINT* pStencilRef)                                                                                                                                                                                         {}

	virtual void SOSetTargets_PostCallHook(UINT NumBuffers, ID3D11Buffer* const* ppSOTargets, const UINT* pOffsets)                                                                                                                                                                                            {}
	virtual void SOGetTargets_PostCallHook(UINT NumBuffers, ID3D11Buffer** ppSOTargets)                                                                                                                                                                                                                        {}

	virtual void Dispatch_PostCallHook(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)                                                                                                                                                                                                 {}
	virtual void DispatchIndirect_PostCallHook(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)                                                                                                                                                                                                    {}

	virtual void RSSetState_PostCallHook(ID3D11RasterizerState* pRasterizerState)                                                                                                                                                                                                                              {}
	virtual void RSGetState_PostCallHook(ID3D11RasterizerState** ppRasterizerState)                                                                                                                                                                                                                            {}
	virtual void RSSetViewports_PostCallHook(UINT NumViewports, const D3D11_VIEWPORT* pViewports)                                                                                                                                                                                                              {}
	virtual void RSGetViewports_PostCallHook(UINT* pNumViewports, D3D11_VIEWPORT* pViewports)                                                                                                                                                                                                                  {}
	virtual void RSSetScissorRects_PostCallHook(UINT NumRects, const D3D11_RECT* pRects)                                                                                                                                                                                                                       {}
	virtual void RSGetScissorRects_PostCallHook(UINT* pNumRects, D3D11_RECT* pRects)                                                                                                                                                                                                                           {}

	virtual void CopyResource_PostCallHook(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource)                                                                                                                                                                                                         {}
	virtual void CopySubresourceRegion_PostCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox) {}
	virtual void CopySubresourcesRegion_PostCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT NumSubresources) {}
	virtual void UpdateSubresource_PostCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)                                                                                                                       {}

	virtual void CopyStructureCount_PostCallHook(ID3D11Buffer* pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView* pSrcView)                                                                                                                                                                     {}

	virtual void ClearRenderTargetView_PostCallHook(ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4])                                                                                                                                                                                       {}
	virtual void ClearUnorderedAccessViewUint_PostCallHook(ID3D11UnorderedAccessView* pUnorderedAccessView, const UINT Values[4])                                                                                                                                                                              {}
	virtual void ClearUnorderedAccessViewFloat_PostCallHook(ID3D11UnorderedAccessView* pUnorderedAccessView, const FLOAT Values[4])                                                                                                                                                                            {}
	virtual void ClearDepthStencilView_PostCallHook(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)                                                                                                                                                                    {}

	virtual void GenerateMips_PostCallHook(ID3D11ShaderResourceView* pShaderResourceView)                                                                                                                                                                                                                      {}

	virtual void SetResourceMinLOD_PostCallHook(ID3D11Resource* pResource, FLOAT MinLOD)                                                                                                                                                                                                                       {}
	virtual void GetResourceMinLOD_PostCallHook(ID3D11Resource* pResource)                                                                                                                                                                                                                                     {}

	virtual void ResolveSubresource_PostCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, ID3D11Resource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)                                                                                                                                     {}

	#if !CRY_PLATFORM_ORBIS
	virtual void ExecuteCommandList_PostCallHook(ID3D11CommandList* pCommandList, BOOL RestoreContextState)          {}
	virtual void FinishCommandList_PostCallHook(BOOL RestoreDeferredContextState, ID3D11CommandList** ppCommandList) {}
	#endif

	virtual void ClearState_PostCallHook()                               {}
	virtual void Flush_PostCallHook()                                    {}

	virtual void GetType_PostCallHook(D3D11_DEVICE_CONTEXT_TYPE nResult) {}
	virtual void GetContextFlags_PostCallHook(UINT nResult)              {}

	#if (CRY_RENDERER_DIRECT3D >= 111)
	virtual void CopySubresourceRegion1_PostCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT CopyFlags) {}
	virtual void CopySubresourcesRegion1_PostCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT CopyFlags, UINT NumSubresources) {}
	virtual void UpdateSubresource1_PostCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags) {}
	virtual void DiscardResource_PostCallHook(ID3D11Resource* pResource) {}
	virtual void DiscardView_PostCallHook(ID3D11View* pResourceView) {}
	#endif

	#if (CRY_RENDERER_DIRECT3D >= 111)
	virtual void VSSetConstantBuffers1_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants) {}
	virtual void HSSetConstantBuffers1_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants) {}
	virtual void DSSetConstantBuffers1_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants) {}
	virtual void GSSetConstantBuffers1_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants) {}
	virtual void PSSetConstantBuffers1_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants) {}
	virtual void CSSetConstantBuffers1_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants) {}
	#endif

	#if (CRY_RENDERER_DIRECT3D >= 111)
	virtual void VSGetConstantBuffers1_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants) {}
	virtual void HSGetConstantBuffers1_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants) {}
	virtual void DSGetConstantBuffers1_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants) {}
	virtual void GSGetConstantBuffers1_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants) {}
	virtual void PSGetConstantBuffers1_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants) {}
	virtual void CSGetConstantBuffers1_PostCallHook(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants) {}

	virtual void SwapDeviceContextState_PostCallHook(ID3DDeviceContextState* pState, ID3DDeviceContextState** ppPreviousState)                                    {}
	virtual void ClearView_PostCallHook(ID3D11View* pView, const FLOAT Color[4], const D3D11_RECT* pRect, UINT NumRects)                                          {}
	virtual void DiscardView1_PostCallHook(ID3D11View* pResourceView, const D3D11_RECT* pRects, UINT NumRects)                                                    {}
	#endif

	virtual void ClearRectsRenderTargetView_PostCallHook(ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D11_RECT* pRects)                                                                                                {}
	virtual void ClearRectsUnorderedAccessViewUint_PostCallHook(ID3D11UnorderedAccessView* pUnorderedAccessView, const UINT Values[4], UINT NumRects, const D3D11_RECT* pRects)                                                                                       {}
	virtual void ClearRectsUnorderedAccessViewFloat_PostCallHook(ID3D11UnorderedAccessView* pUnorderedAccessView, const FLOAT Values[4], UINT NumRects, const D3D11_RECT* pRects)                                                                                     {}
	virtual void ClearRectsDepthStencilView_PostCallHook(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D11_RECT* pRects)                                                                             {}

	virtual void CopyResourceOvercross_PostCallHook(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource) {}
	virtual void JoinSubresourceRegion_PostCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResourceL, ID3D11Resource* pSrcResourceR, UINT SrcSubresource, const D3D11_BOX* pSrcBox)                  {}
	#if (CRY_RENDERER_DIRECT3D >= 111)
	virtual void CopyResourceOvercross1_PostCallHook(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource, UINT CopyFlags) {}
	virtual void JoinSubresourceRegion1_PostCallHook(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResourceL, ID3D11Resource* pSrcResourceR, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT CopyFlags) {}
	#endif

	virtual void CopyStagingResource_PostCallHook(HRESULT hr, ID3D11Resource* pStagingResource, ID3D11Resource* pSourceResource, UINT SubResource, BOOL Upload) {}
	virtual void TestStagingResource_PostCallHook(HRESULT hr, ID3D11Resource* pStagingResource)                                                                 {}
	virtual void WaitStagingResource_PostCallHook(HRESULT hr, ID3D11Resource* pStagingResource)                                                                 {}
	virtual void MapStagingResource_PostCallHook(HRESULT hr, ID3D11Resource* pStagingResource, BOOL Upload, void** ppStagingMemory)                             {}
	virtual void UnmapStagingResource_PostCallHook(ID3D11Resource* pStagingResource, BOOL Upload)                                                               {}

	virtual void MappedWriteToSubresource_PostCallHook(HRESULT hr, ID3D11Resource* pResource, UINT Subresource, SIZE_T Offset, SIZE_T Size, D3D11_MAP MapType, const void* pData, UINT numDataBlocks) {}
	virtual void MappedReadFromSubresource_PostCallHook(HRESULT hr, ID3D11Resource* pResource, UINT Subresource, SIZE_T Offset, SIZE_T Size, D3D11_MAP MapType, void* pData, UINT numDataBlocks) {}

	#if defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE)
	/*************************************/
	/*** ID3DPerformanceDevice methods ***/
	virtual void QueryInterface_PerformanceDevice_PostCallHook(HRESULT hr, REFIID riid, void** ppvObj)                                                                                         {};
	virtual void AddRef_PerformanceDevice_PostCallHook(ULONG nResult)                                                                                                                          {};
	virtual void Release_PerformanceDevice_PostCallHook(ULONG nResult)                                                                                                                         {};

	virtual void CreateCounterSet_PostCallHook(HRESULT hr, const D3D11X_COUNTER_SET_DESC* pCounterSetDesc, ID3DXboxCounterSet** ppCounterSet)                                                  {}
	virtual void CreateCounterSample_PostCallHook(HRESULT hr, ID3DXboxCounterSample** ppCounterSample)                                                                                         {}

	virtual void SetDriverHint_PostCallHook(HRESULT hr, UINT Feature, UINT Value)                                                                                                              {}
	virtual void CreateDmaEngineContext_PostCallHook(HRESULT hr, const D3D11_DMA_ENGINE_CONTEXT_DESC* pDmaEngineContextDesc, ID3D11DmaEngineContextX** ppDmaDeviceContext)                     {}

	virtual void IsFencePending_PostCallHook(BOOL bResult, UINT64 Fence)                                                                                                                       {}
	virtual void IsResourcePending_PostCallHook(BOOL bResult, ID3D11Resource* pResource)                                                                                                       {}

	virtual void CreatePlacementBuffer_PostCallHook(HRESULT hr, const D3D11_BUFFER_DESC* pDesc, void* pCpuVirtualAddress, ID3D11Buffer** ppBuffer)                                             {}
	virtual void CreatePlacementTexture1D_PostCallHook(HRESULT hr, const D3D11_TEXTURE1D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture1D** ppTexture1D) {}
	virtual void CreatePlacementTexture2D_PostCallHook(HRESULT hr, const D3D11_TEXTURE2D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture2D** ppTexture2D) {}
	virtual void CreatePlacementTexture3D_PostCallHook(HRESULT hr, const D3D11_TEXTURE3D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture3D** ppTexture3D) {}

	/********************************************/
	/*** ID3DPerformanceDeviceContext methods ***/
	virtual void QueryInterface_PerformanceContext_PostCallHook(HRESULT hr, REFIID riid, void** ppvObj)                                  {};
	virtual void AddRef_PerformanceContext_PostCallHook(ULONG nResult)                                                                   {};
	virtual void Release_PerformanceContext_PostCallHook(ULONG nResult)                                                                  {};

	virtual void FlushGpuCaches_PostCallHook(ID3D11Resource* pResource)                                                                  {}
	virtual void FlushGpuCacheRange_PostCallHook(UINT Flags, void* pBaseAddress, SIZE_T SizeInBytes)                                     {}

	virtual void InsertWaitUntilIdle_PostCallHook(UINT Flags)                                                                            {}
	virtual void InsertFence_PostCallHook(UINT64 nResult)                                                                                {}

	virtual void RemapConstantBufferInheritance_PostCallHook(D3D11_STAGE Stage, UINT Slot, D3D11_STAGE InheritStage, UINT InheritSlot)   {}
	virtual void RemapShaderResourceInheritance_PostCallHook(D3D11_STAGE Stage, UINT Slot, D3D11_STAGE InheritStage, UINT InheritSlot)   {}
	virtual void RemapSamplerInheritance_PostCallHook(D3D11_STAGE Stage, UINT Slot, D3D11_STAGE InheritStage, UINT InheritSlot)          {}
	virtual void RemapVertexBufferInheritance_PostCallHook(UINT Slot, UINT InheritSlot)                                                  {}

	virtual void PSSetFastConstantBuffer_PostCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer)                                          {}
	virtual void PSSetFastShaderResource_PostCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)                          {}
	virtual void PSSetFastSampler_PostCallHook(UINT Slot, ID3D11SamplerState* pSampler)                                                  {}

	virtual void VSSetFastConstantBuffer_PostCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer)                                          {}
	virtual void VSSetFastShaderResource_PostCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)                          {}
	virtual void VSSetFastSampler_PostCallHook(UINT Slot, ID3D11SamplerState* pSampler)                                                  {}

	virtual void GSSetFastConstantBuffer_PostCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer)                                          {}
	virtual void GSSetFastShaderResource_PostCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)                          {}
	virtual void GSSetFastSampler_PostCallHook(UINT Slot, ID3D11SamplerState* pSampler)                                                  {}

	virtual void CSSetFastConstantBuffer_PostCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer)                                          {}
	virtual void CSSetFastShaderResource_PostCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)                          {}
	virtual void CSSetFastSampler_PostCallHook(UINT Slot, ID3D11SamplerState* pSampler)                                                  {}

	virtual void HSSetFastConstantBuffer_PostCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer)                                          {}
	virtual void HSSetFastShaderResource_PostCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)                          {}
	virtual void HSSetFastSampler_PostCallHook(UINT Slot, ID3D11SamplerState* pSampler)                                                  {}

	virtual void DSSetFastConstantBuffer_PostCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer)                                          {}
	virtual void DSSetFastShaderResource_PostCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)                          {}
	virtual void DSSetFastSampler_PostCallHook(UINT Slot, ID3D11SamplerState* pSampler)                                                  {}

	virtual void IASetFastVertexBuffer_PostCallHook(UINT Slot, ID3D11Buffer* pVertexBuffer, UINT Stride)                                 {}
	virtual void IASetFastIndexBuffer_PostCallHook(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format)                                       {}

	virtual void PSSetPlacementConstantBuffer_PostCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)                 {}
	virtual void PSSetPlacementShaderResource_PostCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress) {}

	virtual void VSSetPlacementConstantBuffer_PostCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)                 {}
	virtual void VSSetPlacementShaderResource_PostCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress) {}

	virtual void GSSetPlacementConstantBuffer_PostCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)                 {}
	virtual void GSSetPlacementShaderResource_PostCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress) {}

	virtual void CSSetPlacementConstantBuffer_PostCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)                 {}
	virtual void CSSetPlacementShaderResource_PostCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress) {}

	virtual void HSSetPlacementConstantBuffer_PostCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)                 {}
	virtual void HSSetPlacementShaderResource_PostCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress) {}

	virtual void DSSetPlacementConstantBuffer_PostCallHook(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)                 {}
	virtual void DSSetPlacementShaderResource_PostCallHook(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress) {}

	virtual void IASetPlacementVertexBuffer_PostCallHook(UINT Slot, ID3D11Buffer* pVertexBuffer, void* pBaseAddress, UINT Stride)        {}
	virtual void IASetPlacementIndexBuffer_PostCallHook(ID3D11Buffer* pIndexBuffer, void* pBaseAddress, DXGI_FORMAT Format)              {}

	virtual void PIXGpuCaptureNextFrame_PostCallHook(HRESULT hr, UINT Flags, LPCWSTR lpOutputFileName)                                   {}
	virtual void PIXGpuBeginCapture_PostCallHook(HRESULT hr, UINT Flags, LPCWSTR lpOutputFileName)                                       {}
	virtual void PIXGpuEndCapture_PostCallHook(HRESULT hr)                                                                               {}

	virtual void InsertFence_PostCallHook(UINT64 nResult, UINT Flags)                                                                                {}
	virtual void InsertWaitOnFence_PostCallHook(UINT Flags, UINT64 Fence)                                                                            {}

	virtual void PIXBeginEvent_PostCallHook(INT nResult, LPCWSTR Name)                                                                               {}
	virtual void PIXEndEvent_PostCallHook(INT nResult)                                                                                               {}
	virtual void PIXSetMarker_PostCallHook(LPCWSTR Name)                                                                                             {}
	virtual void PIXGetStatus_PostCallHook(BOOL nResult)                                                                                             {}

	virtual void StartCounters_PostCallHook(ID3D11CounterSetX* pCounterSet)                                                                          {}
	virtual void SampleCounters_PostCallHook(ID3D11CounterSampleX* pCounterSample)                                                                   {}
	virtual void StopCounters_PostCallHook()                                                                                                         {}
	virtual void GetCounterData_PostCallHook(HRESULT hr, ID3D11CounterSampleX* pCounterSample, D3D11X_COUNTER_DATA* pData, UINT GetCounterDataFlags) {}

	virtual void HSSetTessellationParameters_PostCallHook(const D3D11X_TESSELLATION_PARAMETERS* pTessellationParameters)                             {}
	virtual void HSGetLastUsedTessellationParameters_PostCallHook(D3D11X_TESSELLATION_PARAMETERS* pTessellationParameters)                           {}
	#endif   // DEVICE_SUPPORTS_PERFORMANCE_DEVICE
};

///////////////////////////////////////////////////////////////////////////////
// include concrete device/context implementations
#if (CRY_RENDERER_DIRECT3D >= 120)
	#include "XRenderD3D9/DX12/CryDX12.hpp"
#endif

///////////////////////////////////////////////////////////////////////////////
// Wrapper class for a D3D11 Device
// inline functions are calling the wrapped class
// each function is annotated with calls to the IDeviceWrapperHook list
class CCryDeviceWrapper
{
public:
	CCryDeviceWrapper();

	bool       IsValid() const;
	void       AssignDevice(D3DDevice* pDevice);
	void       SwitchNodeVisibility(UINT visibilityMask);
	void       ReleaseDevice();
	D3DDevice* GetRealDevice() const;

	void       RegisterHook(ICryDeviceWrapperHook* pDeviceWrapperHook);
	void       UnregisterHook(const char* pDeviceHookName);

	/************************/
	/*** IUnknown methods ***/
	HRESULT QueryInterface(REFIID riid, void** ppvObj);
	ULONG   AddRef();
	ULONG   Release();

	/**************************/
	/*** ID3DDevice methods ***/
	HRESULT           GetDeviceRemovedReason(void);

	HRESULT           SetExceptionMode(UINT RaiseFlags);
	UINT              GetExceptionMode();

	HRESULT           GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData);
	HRESULT           SetPrivateData(REFGUID guid, UINT DataSize, const void* pData);
	HRESULT           SetPrivateDataInterface(REFGUID guid, const IUnknown* pData);

	HRESULT           CreateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer);

	HRESULT           CreateTexture1D(const D3D11_TEXTURE1D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D);
	HRESULT           CreateTexture2D(const D3D11_TEXTURE2D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D);
	HRESULT           CreateTexture3D(const D3D11_TEXTURE3D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D);

	HRESULT           CreateShaderResourceView(ID3D11Resource* pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc, ID3D11ShaderResourceView** ppSRView);
	HRESULT           CreateRenderTargetView(ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc, ID3D11RenderTargetView** ppRTView);
	HRESULT           CreateDepthStencilView(ID3D11Resource* pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc, ID3D11DepthStencilView** ppDepthStencilView);
	HRESULT           CreateUnorderedAccessView(ID3D11Resource* pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc, ID3D11UnorderedAccessView** ppUAView);

	HRESULT           CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements, const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout** ppInputLayout);

	HRESULT           CreateVertexShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader);
	HRESULT           CreateGeometryShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader);
	HRESULT           CreateGeometryShaderWithStreamOutput(const void* pShaderBytecode, SIZE_T BytecodeLength, const D3D11_SO_DECLARATION_ENTRY* pSODeclaration, UINT NumEntries, const UINT* pBufferStrides, UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader);
	HRESULT           CreatePixelShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader);
	HRESULT           CreateHullShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11HullShader** ppHullShader);
	HRESULT           CreateDomainShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11DomainShader** ppDomainShader);
	HRESULT           CreateComputeShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11ComputeShader** ppComputeShader);

	HRESULT           CreateBlendState(const D3D11_BLEND_DESC* pBlendStateDesc, ID3D11BlendState** ppBlendState);
	HRESULT           CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc, ID3D11DepthStencilState** ppDepthStencilState);
	HRESULT           CreateRasterizerState(const D3D11_RASTERIZER_DESC* pRasterizerDesc, ID3D11RasterizerState** ppRasterizerState);
	HRESULT           CreateSamplerState(const D3D11_SAMPLER_DESC* pSamplerDesc, ID3D11SamplerState** ppSamplerState);

	HRESULT           CreateQuery(const D3D11_QUERY_DESC* pQueryDesc, ID3D11Query** ppQuery);
	HRESULT           CreatePredicate(const D3D11_QUERY_DESC* pPredicateDesc, ID3D11Predicate** ppPredicate);

	HRESULT           CreateClassLinkage(ID3D11ClassLinkage** ppLinkage);

	UINT              GetCreationFlags();
	void              GetImmediateContext(ID3D11DeviceContext** ppImmediateContext);
	HRESULT           CreateDeferredContext(UINT ContextFlags, ID3D11DeviceContext** ppDeferredContext);

	D3D_FEATURE_LEVEL GetFeatureLevel();
	UINT              GetNodeCount();
	HRESULT           CheckFeatureSupport(D3D11_FEATURE Feature, void* pFeatureSupportData, UINT FeatureSupportDataSize);
	HRESULT           CheckFormatSupport(DXGI_FORMAT Format, UINT* pFormatSupport);
	HRESULT           CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount, UINT* pNumQualityLevels);

	void              CheckCounterInfo(D3D11_COUNTER_INFO* pCounterInfo);
	HRESULT           CreateCounter(const D3D11_COUNTER_DESC* pCounterDesc, ID3D11Counter** ppCounter);
	HRESULT           CheckCounter(const D3D11_COUNTER_DESC* pDesc, D3D11_COUNTER_TYPE* pType, UINT* pActiveCounters, LPSTR wszName, UINT* pNameLength, LPSTR wszUnits, UINT* pUnitsLength, LPSTR wszDescription, UINT* pDescriptionLength);

	HRESULT           OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface, void** ppResource);

	#if (CRY_RENDERER_DIRECT3D >= 111)
	void GetImmediateContext1(ID3D11DeviceContext1** ppImmediateContext);
	#endif

	// DX12-specific with DX11-fallback
	HRESULT CreateTarget1D(const D3D11_TEXTURE1D_DESC* pDesc, const FLOAT ClearValue[4], const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D);
	HRESULT CreateTarget2D(const D3D11_TEXTURE2D_DESC* pDesc, const FLOAT ClearValue[4], const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D);
	HRESULT CreateTarget3D(const D3D11_TEXTURE3D_DESC* pDesc, const FLOAT ClearValue[4], const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D);

	// DX12-only
	HRESULT CreateNullResource(D3D11_RESOURCE_DIMENSION eType, ID3D11Resource** ppNullResource);
	HRESULT ReleaseNullResource(ID3D11Resource* pNullResource);

	HRESULT CreateStagingResource(ID3D11Resource* pInputResource, ID3D11Resource** ppStagingResource, BOOL Upload);
	HRESULT ReleaseStagingResource(ID3D11Resource* pStagingResource);

private:
	#if (CRY_RENDERER_DIRECT3D >= 120)
	CCryDX12Device * m_pDevice;      // the wrapped device (concrete implementation)
	#else
	D3DDevice * m_pDevice;       // the wrapped device (abstract interface)
	#endif

	_smart_ptr<ICryDeviceWrapperHook> m_pDeviceHooks;   // linked lists of device hooks to execute
};

///////////////////////////////////////////////////////////////////////////////
// Wrapper class for a D3D11 DeviceContext
// inline functions are calling the wrapped class
// each function is annotated with calls to the IDeviceWrapperHook list
class CCryDeviceContextWrapper
{
public:
	CCryDeviceContextWrapper();

	bool              IsValid() const;
	void              AssignDeviceContext(D3DDeviceContext* pDeviceContext);
	void              SwitchToDeviceNode(UINT nodeMask);
	void              ReleaseDeviceContext();
	D3DDeviceContext* GetRealDeviceContext() const;

	void              RegisterHook(ICryDeviceWrapperHook* pDeviceWrapperHook);
	void              UnregisterHook(const char* pDeviceHookName);

	/************************/
	/*** IUnknown methods ***/
	HRESULT QueryInterface(REFIID riid, void** ppvObj);
	ULONG   AddRef();
	ULONG   Release();

	/*********************************/
	/*** ID3DDeviceContext methods ***/
	void    GetDevice(ID3D11Device** ppDevice);

	HRESULT GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData);
	HRESULT SetPrivateData(REFGUID guid, UINT DataSize, const void* pData);
	HRESULT SetPrivateDataInterface(REFGUID guid, const IUnknown* pData);

	void    PSSetShader(ID3D11PixelShader* pPixelShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	void    PSGetShader(ID3D11PixelShader** ppPixelShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	void    PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	void    PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	void    PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	void    PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	void    PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	void    PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);

	void    VSSetShader(ID3D11VertexShader* pVertexShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	void    VSGetShader(ID3D11VertexShader** ppVertexShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	void    VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	void    VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	void    VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	void    VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	void    VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	void    VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);

	void    GSSetShader(ID3D11GeometryShader* pShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	void    GSGetShader(ID3D11GeometryShader** ppGeometryShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	void    GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	void    GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	void    GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	void    GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	void    GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	void    GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);

	void    HSSetShader(ID3D11HullShader* pHullShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	void    HSGetShader(ID3D11HullShader** ppHullShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	void    HSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	void    HSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	void    HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	void    HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	void    HSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	void    HSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);

	void    DSSetShader(ID3D11DomainShader* pDomainShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	void    DSGetShader(ID3D11DomainShader** ppDomainShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	void    DSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	void    DSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	void    DSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	void    DSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	void    DSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	void    DSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);

	void    CSSetShader(ID3D11ComputeShader* pComputeShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	void    CSGetShader(ID3D11ComputeShader** ppComputeShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	void    CSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	void    CSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	void    CSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	void    CSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	void    CSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	void    CSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);

	void    CSSetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts);
	void    CSGetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews);

	void    DrawAuto();
	void    Draw(UINT VertexCount, UINT StartVertexLocation);
	void    DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation);
	void    DrawInstancedIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs);
	void    DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
	void    DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation);
	void    DrawIndexedInstancedIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs);

	HRESULT Map(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource);
	HRESULT Map(ID3D11Resource* pResource, UINT Subresource, SIZE_T* BeginEnd, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource);
	void    Unmap(ID3D11Resource* pResource, UINT Subresource);
	void    Unmap(ID3D11Resource* pResource, UINT Subresource, SIZE_T* BeginEnd);

	void    IASetInputLayout(ID3D11InputLayout* pInputLayout);
	void    IAGetInputLayout(ID3D11InputLayout** ppInputLayout);
	void    IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const UINT* pStrides, const UINT* pOffsets);
	void    IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppVertexBuffers, UINT* pStrides, UINT* pOffsets);
	void    IASetIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, UINT Offset);
	void    IAGetIndexBuffer(ID3D11Buffer** pIndexBuffer, DXGI_FORMAT* Format, UINT* Offset);
	void    IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology);
	void    IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* pTopology);

	void    Begin(ID3D11Asynchronous* pAsync);
	void    End(ID3D11Asynchronous* pAsync);

	HRESULT GetData(ID3D11Asynchronous* pAsync, void* pData, UINT DataSize, UINT GetDataFlags);
	void    SetPredication(ID3D11Predicate* pPredicate, BOOL PredicateValue);
	void    GetPredication(ID3D11Predicate** ppPredicate, BOOL* pPredicateValue);

	void    OMSetRenderTargets(UINT NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView);
	void    OMGetRenderTargets(UINT NumViews, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView);
	void    OMSetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts);
	void    OMGetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews);
	void    OMSetBlendState(ID3D11BlendState* pBlendState, const FLOAT BlendFactor[4], UINT SampleMask);
	void    OMGetBlendState(ID3D11BlendState * *ppBlendState, FLOAT BlendFactor[4], UINT * pSampleMask);
	void    OMSetDepthStencilState(ID3D11DepthStencilState* pDepthStencilState, UINT StencilRef);
	void    OMGetDepthStencilState(ID3D11DepthStencilState** ppDepthStencilState, UINT* pStencilRef);

	void    SOSetTargets(UINT NumBuffers, ID3D11Buffer* const* ppSOTargets, const UINT* pOffsets);
	void    SOGetTargets(UINT NumBuffers, ID3D11Buffer** ppSOTargets);

	void    Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ);
	void    DispatchIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs);

	void    RSSetState(ID3D11RasterizerState* pRasterizerState);
	void    RSGetState(ID3D11RasterizerState** ppRasterizerState);
	void    RSSetViewports(UINT NumViewports, const D3D11_VIEWPORT* pViewports);
	void    RSGetViewports(UINT* pNumViewports, D3D11_VIEWPORT* pViewports);
	void    RSSetScissorRects(UINT NumRects, const D3D11_RECT* pRects);
	void    RSGetScissorRects(UINT* pNumRects, D3D11_RECT* pRects);

	void    CopyResource(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource);
	void    CopySubresourceRegion(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox);
	void    CopySubresourcesRegion(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT NumSubresources);
	void    UpdateSubresource(ID3D11Resource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);

	void    CopyStructureCount(ID3D11Buffer* pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView* pSrcView);

	void    ClearRenderTargetView(ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4]);
	void    ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView* pUnorderedAccessView, const UINT Values[4]);
	void    ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView* pUnorderedAccessView, const FLOAT Values[4]);
	void    ClearDepthStencilView(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil);

	void    GenerateMips(ID3D11ShaderResourceView* pShaderResourceView);

	void    SetResourceMinLOD(ID3D11Resource* pResource, FLOAT MinLOD);
	FLOAT   GetResourceMinLOD(ID3D11Resource* pResource);

	void    ResolveSubresource(ID3D11Resource* pDstResource, UINT DstSubresource, ID3D11Resource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format);

	#if !CRY_PLATFORM_ORBIS
	void    ExecuteCommandList(ID3D11CommandList* pCommandList, BOOL RestoreContextState);
	HRESULT FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList** ppCommandList);
	#endif

	// Wrapper for NvAPI_D3D_SetModifiedWMode
	void	SetModifiedWMode(bool enabled, uint32_t numViewports, const float* pA, const float* pB);
	void	QueryNvidiaProjectionFeatureSupport(bool& bMultiResSupported, bool& bLensMatchedSupported);

	void                      ClearState();
	void                      Flush();

	D3D11_DEVICE_CONTEXT_TYPE GetType();
	UINT                      GetContextFlags();

	#if (CRY_RENDERER_DIRECT3D >= 111)
	void CopySubresourceRegion1(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT CopyFlags);
	void CopySubresourcesRegion1(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT CopyFlags, UINT NumSubresources);
	void UpdateSubresource1(ID3D11Resource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags);
	void DiscardResource(ID3D11Resource* pResource);
	void DiscardView(ID3D11View* pResourceView);

	void VSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
	void HSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
	void DSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
	void GSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
	void PSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
	void CSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);

	void VSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants);
	void HSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants);
	void DSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants);
	void GSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants);
	void PSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants);
	void CSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants);

	void SwapDeviceContextState(ID3DDeviceContextState* pState, ID3DDeviceContextState** ppPreviousState);
	void ClearView(ID3D11View* pView, const FLOAT Color[4], UINT numRects, const D3D11_RECT* pRect); // Note: Argument order swapped intentionally!
	void DiscardView1(ID3D11View* pResourceView, UINT NumRects, const D3D11_RECT* pRects);           // Note: Argument order swapped intentionally!
	#endif                                                                                          

#if (CRY_RENDERER_DIRECT3D >= 111)
	void    TSSetConstantBuffers1(int type, UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
	void    TSGetConstantBuffers1(int type, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants);
#endif

	// DX12-specific with DX11-fallback
	void ClearRectsRenderTargetView(ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D11_RECT* pRects);
	void ClearRectsUnorderedAccessViewUint(ID3D11UnorderedAccessView* pUnorderedAccessView, const UINT Values[4], UINT NumRects, const D3D11_RECT* pRects);
	void ClearRectsUnorderedAccessViewFloat(ID3D11UnorderedAccessView* pUnorderedAccessView, const FLOAT Values[4], UINT NumRects, const D3D11_RECT* pRects);
	void ClearRectsDepthStencilView(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D11_RECT* pRects);

	// DX12-specific with DX11-fallback (for Multi-GPU)
	void CopyResourceOvercross(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource);
	void JoinSubresourceRegion(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResourceL, ID3D11Resource* pSrcResourceR, UINT SrcSubresource, const D3D11_BOX* pSrcBox);
	#if (CRY_RENDERER_DIRECT3D >= 111)
	void CopyResourceOvercross1(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource, UINT CopyFlags);
	void JoinSubresourceRegion1(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResourceL, ID3D11Resource* pSrcResourceR, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT CopyFlags);
	#endif

	// DX12-only
	HRESULT CopyStagingResource(ID3D11Resource* pStagingResource, ID3D11Resource* pSourceResource, UINT SubResource, BOOL Upload);
	HRESULT TestStagingResource(ID3D11Resource* pStagingResource); // S_OK or S_FAIL for ready or running
	HRESULT WaitStagingResource(ID3D11Resource* pStagingResource);
	// Map() and Unmap() are thread-safe under DX12
	HRESULT MapStagingResource(ID3D11Resource* pStagingResource, BOOL Upload, void** ppStagingMemory);
	void    UnmapStagingResource(ID3D11Resource* pStagingResource, BOOL Upload);

	HRESULT MappedWriteToSubresource(ID3D11Resource* pResource, UINT Subresource, SIZE_T Offset, SIZE_T Size, D3D11_MAP MapType, const void* pData, UINT numDataBlocks);
	HRESULT MappedReadFromSubresource(ID3D11Resource* pResource, UINT Subresource, SIZE_T Offset, SIZE_T Size, D3D11_MAP MapType, void* pData, UINT numDataBlocks);

	void    ResetCachedState(bool bGraphics = true, bool bCompute = false);

private:
	#if (CRY_RENDERER_DIRECT3D >= 120)
	CCryDX12DeviceContext * m_pDeviceContext;  // the wrapped device context (concrete implementation)
	#else
	D3DDeviceContext * m_pDeviceContext;     // the wrapped device context (abstract interface)
	#endif

	_smart_ptr<ICryDeviceWrapperHook> m_pDeviceHooks;     // linked lists of device hooks to execute

public:
	/***********************************/
	/*** ID3DDeviceContext delegates ***/

	// DX11-specific without type-branches
	void    TSSetShader(int type, ID3D11DeviceChild* pShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	void    TSGetShader(int type, ID3D11DeviceChild** ppShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	void    TSSetSamplers(int type, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	void    TSGetSamplers(int type, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	void    TSSetConstantBuffers(int type, UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	void    TSGetConstantBuffers(int type, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	void    TSSetShaderResources(int type, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	void    TSGetShaderResources(int type, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);

private:
	// DX11-specific without type-branches
	typedef void    (STDMETHODCALLTYPE ID3D11DeviceContext::*typeSetShader)(ID3D11DeviceChild* pShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances);
	typedef void    (STDMETHODCALLTYPE ID3D11DeviceContext::*typeGetShader)(ID3D11DeviceChild** ppShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances);
	typedef void    (STDMETHODCALLTYPE ID3D11DeviceContext::*typeSetSamplers)(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers);
	typedef void    (STDMETHODCALLTYPE ID3D11DeviceContext::*typeGetSamplers)(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
	typedef void    (STDMETHODCALLTYPE ID3D11DeviceContext::*typeSetConstantBuffers)(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers);
	typedef void    (STDMETHODCALLTYPE ID3D11DeviceContext::*typeGetConstantBuffers)(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
	typedef void    (STDMETHODCALLTYPE ID3D11DeviceContext::*typeSetShaderResources)(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews);
	typedef void    (STDMETHODCALLTYPE ID3D11DeviceContext::*typeGetShaderResources)(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);

#if (CRY_RENDERER_DIRECT3D >= 111)
	typedef void    (STDMETHODCALLTYPE ID3D11DeviceContext1::*typeSetConstantBuffers1)(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
	typedef void    (STDMETHODCALLTYPE ID3D11DeviceContext1::*typeGetConstantBuffers1)(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants);
#endif

	typeSetShader           mapSetShader[6];
	typeGetShader           mapGetShader[6];
	typeSetSamplers         mapSetSamplers[6];
	typeGetSamplers         mapGetSamplers[6];
	typeSetConstantBuffers  mapSetConstantBuffers[6];
	typeGetConstantBuffers  mapGetConstantBuffers[6];
	typeSetShaderResources  mapSetShaderResources[6];
	typeGetShaderResources  mapGetShaderResources[6];

#if (CRY_RENDERER_DIRECT3D >= 111)
	typeSetConstantBuffers1 mapSetConstantBuffers1[6];
	typeGetConstantBuffers1 mapGetConstantBuffers1[6];
#endif
};

	#if defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE)
///////////////////////////////////////////////////////////////////////////////
// Wrapper class for a D3D11 Performance Device
// inline functions are calling the wrapped class
// each function is annotated with calls to the IDeviceWrapperHook list
class CCryPerformanceDeviceWrapper
{
public:
	CCryPerformanceDeviceWrapper();

	bool                       IsValid() const;
	void                       AssignPerformanceDevice(ID3DXboxPerformanceDevice* pPerformanceDevice);
	void                       ReleasePerformanceDevice();
	ID3DXboxPerformanceDevice* GetRealPerformanceDevice() const;

	void                       RegisterHook(ICryDeviceWrapperHook* pDeviceWrapperHook);
	void                       UnregisterHook(const char* pDeviceHookName);

	/************************/
	/*** IUnknown methods ***/
	HRESULT QueryInterface(REFIID riid, void** ppvObj);
	ULONG   AddRef();
	ULONG   Release();

	/*************************************/
	/*** ID3DPerformanceDevice methods ***/
	HRESULT CreateCounterSet(const D3D11X_COUNTER_SET_DESC* pCounterSetDesc, ID3DXboxCounterSet** ppCounterSet);
	HRESULT CreateCounterSample(ID3DXboxCounterSample** ppCounterSample);

	HRESULT SetDriverHint(UINT Feature, UINT Value);
	HRESULT CreateDmaEngineContext(const D3D11_DMA_ENGINE_CONTEXT_DESC* pDmaEngineContextDesc, ID3D11DmaEngineContextX** ppDmaDeviceContext);

	BOOL    IsFencePending(UINT64 Fence);
	BOOL    IsResourcePending(ID3D11Resource* pResource);

	HRESULT CreatePlacementBuffer(const D3D11_BUFFER_DESC* pDesc, void* pCpuVirtualAddress, ID3D11Buffer** ppBuffer);
	HRESULT CreatePlacementTexture1D(const D3D11_TEXTURE1D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture1D** ppTexture1D);
	HRESULT CreatePlacementTexture2D(const D3D11_TEXTURE2D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture2D** ppTexture2D);
	HRESULT CreatePlacementTexture3D(const D3D11_TEXTURE3D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture3D** ppTexture3D);

private:
	ID3DXboxPerformanceDevice*        m_pPerformanceDevice;   // the wrapped performance device
	_smart_ptr<ICryDeviceWrapperHook> m_pDeviceHooks;         // linked lists of device hooks to execute
};

///////////////////////////////////////////////////////////////////////////////
// Wrapper class for a D3D11 Performance Device Context
// inline functions are calling the wrapped class
// each function is annotated with calls to the IDeviceWrapperHook list
class CCryPerformanceDeviceContextWrapper
{
public:
	CCryPerformanceDeviceContextWrapper();

	bool                        IsValid() const;
	void                        AssignPerformanceDeviceContext(ID3DXboxPerformanceContext* pPerformanceDeviceContext);
	void                        ReleasePerformanceDeviceContext();
	ID3DXboxPerformanceContext* GetRealPerformanceDeviceContext() const;

	void                        RegisterHook(ICryDeviceWrapperHook* pDeviceWrapperHook);
	void                        UnregisterHook(const char* pDeviceHookName);

	/********************************************/
	/*** ID3DPerformanceDeviceContext methods ***/
	void    FlushGpuCaches(ID3D11Resource* pResource);
	void    FlushGpuCacheRange(UINT Flags, void* pBaseAddress, SIZE_T SizeInBytes);

	UINT64  InsertFence();
	void    InsertWaitUntilIdle(UINT Flags);

	void    RemapConstantBufferInheritance(D3D11_STAGE Stage, UINT Slot, D3D11_STAGE InheritStage, UINT InheritSlot);
	void    RemapShaderResourceInheritance(D3D11_STAGE Stage, UINT Slot, D3D11_STAGE InheritStage, UINT InheritSlot);
	void    RemapSamplerInheritance(D3D11_STAGE Stage, UINT Slot, D3D11_STAGE InheritStage, UINT InheritSlot);
	void    RemapVertexBufferInheritance(UINT Slot, UINT InheritSlot);

	void    PSSetFastConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer);
	void    PSSetFastShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView);
	void    PSSetFastSampler(UINT Slot, ID3D11SamplerState* pSampler);

	void    VSSetFastConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer);
	void    VSSetFastShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView);
	void    VSSetFastSampler(UINT Slot, ID3D11SamplerState* pSampler);

	void    GSSetFastConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer);
	void    GSSetFastShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView);
	void    GSSetFastSampler(UINT Slot, ID3D11SamplerState* pSampler);

	void    CSSetFastConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer);
	void    CSSetFastShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView);
	void    CSSetFastSampler(UINT Slot, ID3D11SamplerState* pSampler);

	void    HSSetFastConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer);
	void    HSSetFastShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView);
	void    HSSetFastSampler(UINT Slot, ID3D11SamplerState* pSampler);

	void    DSSetFastConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer);
	void    DSSetFastShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView);
	void    DSSetFastSampler(UINT Slot, ID3D11SamplerState* pSampler);

	void    IASetFastVertexBuffer(UINT Slot, ID3D11Buffer* pVertexBuffer, UINT Stride);
	void    IASetFastIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format);

	void    PSSetPlacementConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress);
	void    PSSetPlacementShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress);

	void    VSSetPlacementConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress);
	void    VSSetPlacementShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress);

	void    GSSetPlacementConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress);
	void    GSSetPlacementShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress);

	void    CSSetPlacementConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress);
	void    CSSetPlacementShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress);

	void    HSSetPlacementConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress);
	void    HSSetPlacementShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress);

	void    DSSetPlacementConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress);
	void    DSSetPlacementShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress);

	void    IASetPlacementVertexBuffer(UINT Slot, ID3D11Buffer* pVertexBuffer, void* pBaseAddress, UINT Stride);
	void    IASetPlacementIndexBuffer(ID3D11Buffer* pIndexBuffer, void* pBaseAddress, DXGI_FORMAT Format);

	HRESULT PIXGpuCaptureNextFrame(UINT Flags, LPCWSTR lpOutputFileName);
	HRESULT PIXGpuBeginCapture(UINT Flags, LPCWSTR lpOutputFileName);
	HRESULT PIXGpuEndCapture();

	UINT64  InsertFence(UINT Flags);
	void    InsertWaitOnFence(UINT Flags, UINT64 Fence);

	INT     PIXBeginEvent(LPCWSTR Name);
	INT     PIXEndEvent();
	void    PIXSetMarker(LPCWSTR Name);
	BOOL    PIXGetStatus();

	void    StartCounters(ID3D11CounterSetX* pCounterSet);
	void    SampleCounters(ID3D11CounterSampleX* pCounterSample);
	void    StopCounters();
	HRESULT GetCounterData(ID3D11CounterSampleX* pCounterSample, D3D11X_COUNTER_DATA* pData, UINT GetCounterDataFlags);

	void    HSSetTessellationParameters(const D3D11X_TESSELLATION_PARAMETERS* pTessellationParameters);
	void    HSGetLastUsedTessellationParameters(D3D11X_TESSELLATION_PARAMETERS* pTessellationParameters);

private:
	ID3DXboxPerformanceContext*       m_pPerformanceDeviceContext;    // the wrapped performance device context
	_smart_ptr<ICryDeviceWrapperHook> m_pDeviceHooks;                 // linked lists of device hooks to execute
};
	#endif // DEVICE_SUPPORTS_PERFORMANCE_DEVICE

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// define private helper macros to prevent repeating the same code with slight changes
///////////////////////////////////////////////////////////////////////////////
	#if DX11_ENABLEDHOOKS
		#define _CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(FunctionName, ...) \
		  do {                                                                          \
		    ICryDeviceWrapperHook* pDeviceHooks = m_pDeviceHooks;                       \
		    while (pDeviceHooks)                                                        \
		    {                                                                           \
		      pDeviceHooks->FunctionName ## _PreCallHook(__VA_ARGS__);                  \
		      pDeviceHooks = pDeviceHooks->GetNext();                                   \
		    }                                                                           \
		  } while (0)

///////////////////////////////////////////////////////////////////////////////
		#define _CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(FunctionName, ...) \
		  do {                                                                           \
		    ICryDeviceWrapperHook* pDeviceHooks = m_pDeviceHooks;                        \
		    while (pDeviceHooks)                                                         \
		    {                                                                            \
		      pDeviceHooks->FunctionName ## _PostCallHook(__VA_ARGS__);                  \
		      pDeviceHooks = pDeviceHooks->GetNext();                                    \
		    }                                                                            \
		  } while (0)
	#endif

///////////////////////////////////////////////////////////////////////////////
	#if !DX11_ENABLEDHOOKS
		#define _CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(FunctionName, ...)  do {} while (0)
		#define _CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(FunctionName, ...) do {} while (0)
	#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// CryPerformanceDeviceWrapper methods
///////////////////////////////////////////////////////////////////////////////
inline CCryDeviceWrapper::CCryDeviceWrapper()
	: m_pDevice(NULL)
	, m_pDeviceHooks(NULL)
{
}

///////////////////////////////////////////////////////////////////////////////
inline bool CCryDeviceWrapper::IsValid() const
{
	return m_pDevice != NULL;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceWrapper::AssignDevice(D3DDevice* pDevice)
{
	if (m_pDevice != NULL && m_pDevice != pDevice)
		CryFatalError("Trying to assign two difference devices");

	#if (CRY_RENDERER_DIRECT3D >= 120)
	m_pDevice = (CCryDX12Device*)pDevice;
	#else
	m_pDevice = (D3DDevice*)pDevice;
	#endif
}

inline void CCryDeviceWrapper::SwitchNodeVisibility(UINT visibilityMask)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	m_pDevice->SetVisibilityMask(visibilityMask);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceWrapper::ReleaseDevice()
{
	if (m_pDevice != NULL)
	{
		m_pDevice->Release();
		m_pDevice = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////
inline D3DDevice* CCryDeviceWrapper::GetRealDevice() const
{
	return m_pDevice;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceWrapper::RegisterHook(ICryDeviceWrapperHook* pDeviceWrapperHook)
{
	pDeviceWrapperHook->SetNext(m_pDeviceHooks);
	m_pDeviceHooks = pDeviceWrapperHook;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceWrapper::UnregisterHook(const char* pDeviceHookName)
{
	ICryDeviceWrapperHook* pDeviceHook = m_pDeviceHooks;

	if (pDeviceHook == NULL)
		return;

	if (strcmp(pDeviceHook->Name(), pDeviceHookName) == 0)
	{
		m_pDeviceHooks = pDeviceHook->GetNext();
		return;
	}

	while (pDeviceHook->GetNext())
	{
		if (strcmp(pDeviceHook->GetNext()->Name(), pDeviceHookName) == 0)
		{
			pDeviceHook->SetNext(pDeviceHook->GetNext()->GetNext());
			return;
		}
		pDeviceHook = pDeviceHook->GetNext();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::QueryInterface(REFIID riid, void** ppvObj)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(QueryInterface_Device, riid, ppvObj);
	HRESULT hr = m_pDevice->QueryInterface(riid, ppvObj);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(QueryInterface_Device, hr, riid, ppvObj);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline ULONG CCryDeviceWrapper::AddRef()
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(AddRef_Device);
	ULONG nRefCount = m_pDevice->AddRef();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(AddRef_Device, nRefCount);
	return nRefCount;
}

///////////////////////////////////////////////////////////////////////////////
inline ULONG CCryDeviceWrapper::Release()
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(Release_Device);
	ULONG nRefCount = m_pDevice->Release();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(Release_Device, nRefCount);
	return nRefCount;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::GetDeviceRemovedReason()
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GetDeviceRemovedReason);
	HRESULT hr = m_pDevice->GetDeviceRemovedReason();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GetDeviceRemovedReason, hr);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::SetExceptionMode(UINT RaiseFlags)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(SetExceptionMode, RaiseFlags);
	HRESULT hr = m_pDevice->SetExceptionMode(RaiseFlags);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(SetExceptionMode, hr, RaiseFlags);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline UINT CCryDeviceWrapper::GetExceptionMode()
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GetExceptionMode);
	UINT nResult = m_pDevice->GetExceptionMode();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GetExceptionMode, nResult);
	return nResult;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GetPrivateData_Device, guid, pDataSize, pData);
	HRESULT hr = m_pDevice->GetPrivateData(guid, pDataSize, pData);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GetPrivateData_Device, hr, guid, pDataSize, pData);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::SetPrivateData(REFGUID guid, UINT DataSize, const void* pData)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(SetPrivateData_Device, guid, DataSize, pData);
	HRESULT hr = m_pDevice->SetPrivateData(guid, DataSize, pData);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(SetPrivateData_Device, hr, guid, DataSize, pData);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(SetPrivateDataInterface_Device, guid, pData);
	HRESULT hr = m_pDevice->SetPrivateDataInterface(guid, pData);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(SetPrivateDataInterface_Device, hr, guid, pData);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateBuffer, pDesc, pInitialData, ppBuffer);
	HRESULT hr = m_pDevice->CreateBuffer(pDesc, pInitialData, ppBuffer);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateBuffer, hr, pDesc, pInitialData, ppBuffer);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateTexture1D(const D3D11_TEXTURE1D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateTexture1D, pDesc, pInitialData, ppTexture1D);
	HRESULT hr = m_pDevice->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateTexture1D, hr, pDesc, pInitialData, ppTexture1D);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateTexture2D(const D3D11_TEXTURE2D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateTexture2D, pDesc, pInitialData, ppTexture2D);
	HRESULT hr = m_pDevice->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateTexture2D, hr, pDesc, pInitialData, ppTexture2D);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateTexture3D(const D3D11_TEXTURE3D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateTexture3D, pDesc, pInitialData, ppTexture3D);
	HRESULT hr = m_pDevice->CreateTexture3D(pDesc, pInitialData, ppTexture3D);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateTexture3D, hr, pDesc, pInitialData, ppTexture3D);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateShaderResourceView(ID3D11Resource* pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc, ID3D11ShaderResourceView** ppSRView)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateShaderResourceView, pResource, pDesc, ppSRView);
	HRESULT hr = m_pDevice->CreateShaderResourceView(pResource, pDesc, ppSRView);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateShaderResourceView, hr, pResource, pDesc, ppSRView);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateRenderTargetView(ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc, ID3D11RenderTargetView** ppRTView)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateRenderTargetView, pResource, pDesc, ppRTView);
	HRESULT hr = m_pDevice->CreateRenderTargetView(pResource, pDesc, ppRTView);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateRenderTargetView, hr, pResource, pDesc, ppRTView);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateDepthStencilView(ID3D11Resource* pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc, ID3D11DepthStencilView** ppDepthStencilView)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateDepthStencilView, pResource, pDesc, ppDepthStencilView);
	HRESULT hr = m_pDevice->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateDepthStencilView, hr, pResource, pDesc, ppDepthStencilView);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateUnorderedAccessView(ID3D11Resource* pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc, ID3D11UnorderedAccessView** ppUAView)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateUnorderedAccessView, pResource, pDesc, ppUAView);
	HRESULT hr = m_pDevice->CreateUnorderedAccessView(pResource, pDesc, ppUAView);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateUnorderedAccessView, hr, pResource, pDesc, ppUAView);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements, const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout** ppInputLayout)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateInputLayout, pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
	HRESULT hr = m_pDevice->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateInputLayout, hr, pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateVertexShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateVertexShader, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
	HRESULT hr = m_pDevice->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateVertexShader, hr, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateGeometryShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateGeometryShader, pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
	HRESULT hr = m_pDevice->CreateGeometryShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateGeometryShader, hr, pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateGeometryShaderWithStreamOutput(const void* pShaderBytecode, SIZE_T BytecodeLength, const D3D11_SO_DECLARATION_ENTRY* pSODeclaration, UINT NumEntries, const UINT* pBufferStrides, UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateGeometryShaderWithStreamOutput, pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader);
	HRESULT hr = m_pDevice->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateGeometryShaderWithStreamOutput, hr, pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreatePixelShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreatePixelShader, pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
	HRESULT hr = m_pDevice->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreatePixelShader, hr, pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateHullShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11HullShader** ppHullShader)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateHullShader, pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
	HRESULT hr = m_pDevice->CreateHullShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateHullShader, hr, pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateDomainShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11DomainShader** ppDomainShader)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateDomainShader, pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
	HRESULT hr = m_pDevice->CreateDomainShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateDomainShader, hr, pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateComputeShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11ComputeShader** ppComputeShader)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateComputeShader, pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
	HRESULT hr = m_pDevice->CreateComputeShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateComputeShader, hr, pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateBlendState(const D3D11_BLEND_DESC* pBlendStateDesc, ID3D11BlendState** ppBlendState)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateBlendState, pBlendStateDesc, ppBlendState);
	HRESULT hr = m_pDevice->CreateBlendState(pBlendStateDesc, ppBlendState);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateBlendState, hr, pBlendStateDesc, ppBlendState);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc, ID3D11DepthStencilState** ppDepthStencilState)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateDepthStencilState, pDepthStencilDesc, ppDepthStencilState);
	HRESULT hr = m_pDevice->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateDepthStencilState, hr, pDepthStencilDesc, ppDepthStencilState);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateRasterizerState(const D3D11_RASTERIZER_DESC* pRasterizerDesc, ID3D11RasterizerState** ppRasterizerState)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateRasterizerState, pRasterizerDesc, ppRasterizerState);
	HRESULT hr = m_pDevice->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateRasterizerState, hr, pRasterizerDesc, ppRasterizerState);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateSamplerState(const D3D11_SAMPLER_DESC* pSamplerDesc, ID3D11SamplerState** ppSamplerState)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateSamplerState, pSamplerDesc, ppSamplerState);
	HRESULT hr = m_pDevice->CreateSamplerState(pSamplerDesc, ppSamplerState);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateSamplerState, hr, pSamplerDesc, ppSamplerState);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateQuery(const D3D11_QUERY_DESC* pQueryDesc, ID3D11Query** ppQuery)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateQuery, pQueryDesc, ppQuery);
	HRESULT hr = m_pDevice->CreateQuery(pQueryDesc, ppQuery);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateQuery, hr, pQueryDesc, ppQuery);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreatePredicate(const D3D11_QUERY_DESC* pPredicateDesc, ID3D11Predicate** ppPredicate)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreatePredicate, pPredicateDesc, ppPredicate);
	HRESULT hr = m_pDevice->CreatePredicate(pPredicateDesc, ppPredicate);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreatePredicate, hr, pPredicateDesc, ppPredicate);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateClassLinkage(ID3D11ClassLinkage** ppLinkage)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	return S_OK;
	#else
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateClassLinkage, ppLinkage);
	HRESULT hr = m_pDevice->CreateClassLinkage(ppLinkage);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateClassLinkage, hr, ppLinkage);
	return hr;
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline UINT CCryDeviceWrapper::GetCreationFlags()
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GetCreationFlags);
	UINT nResult = m_pDevice->GetCreationFlags();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GetCreationFlags, nResult);
	return nResult;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceWrapper::GetImmediateContext(ID3D11DeviceContext** ppImmediateContext)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GetImmediateContext, ppImmediateContext);
	m_pDevice->GetImmediateContext(ppImmediateContext);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GetImmediateContext, ppImmediateContext);
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateDeferredContext(UINT ContextFlags, ID3D11DeviceContext** ppDeferredContext)
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	return S_OK;
	#else
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateDeferredContext, ContextFlags, ppDeferredContext);
	HRESULT hr = m_pDevice->CreateDeferredContext(ContextFlags, ppDeferredContext);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateDeferredContext, hr, ContextFlags, ppDeferredContext);
	return hr;
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline D3D_FEATURE_LEVEL CCryDeviceWrapper::GetFeatureLevel()
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	return D3D_FEATURE_LEVEL();
	#else
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GetFeatureLevel);
	D3D_FEATURE_LEVEL nResult = m_pDevice->GetFeatureLevel();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GetFeatureLevel, nResult);
	return nResult;
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline UINT CCryDeviceWrapper::GetNodeCount()
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GetNodeCount);
	UINT nResult = m_pDevice->GetNodeCount();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GetNodeCount, nResult);
	#else
	UINT nResult = 1;
	#endif

	return nResult;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CheckFeatureSupport(D3D11_FEATURE Feature, void* pFeatureSupportData, UINT FeatureSupportDataSize)
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	return D3D_FEATURE_LEVEL();
	#else
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CheckFeatureSupport, Feature, pFeatureSupportData, FeatureSupportDataSize);
	HRESULT hr = m_pDevice->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CheckFeatureSupport, hr, Feature, pFeatureSupportData, FeatureSupportDataSize);
	return hr;
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CheckFormatSupport(DXGI_FORMAT Format, UINT* pFormatSupport)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CheckFormatSupport, Format, pFormatSupport);
	HRESULT hr = m_pDevice->CheckFormatSupport(Format, pFormatSupport);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CheckFormatSupport, hr, Format, pFormatSupport);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount, UINT* pNumQualityLevels)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CheckMultisampleQualityLevels, Format, SampleCount, pNumQualityLevels);
	HRESULT hr = m_pDevice->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CheckMultisampleQualityLevels, hr, Format, SampleCount, pNumQualityLevels);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceWrapper::CheckCounterInfo(D3D11_COUNTER_INFO* pCounterInfo)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CheckCounterInfo, pCounterInfo);
	m_pDevice->CheckCounterInfo(pCounterInfo);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CheckCounterInfo, pCounterInfo);
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateCounter(const D3D11_COUNTER_DESC* pCounterDesc, ID3D11Counter** ppCounter)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateCounter, pCounterDesc, ppCounter);
	HRESULT hr = m_pDevice->CreateCounter(pCounterDesc, ppCounter);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateCounter, hr, pCounterDesc, ppCounter);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CheckCounter(const D3D11_COUNTER_DESC* pDesc, D3D11_COUNTER_TYPE* pType, UINT* pActiveCounters, LPSTR wszName, UINT* pNameLength, LPSTR wszUnits, UINT* pUnitsLength, LPSTR wszDescription, UINT* pDescriptionLength)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CheckCounter, pDesc, pType, pActiveCounters, wszName, pNameLength, wszUnits, pUnitsLength, wszDescription, pDescriptionLength);
	HRESULT hr = m_pDevice->CheckCounter(pDesc, pType, pActiveCounters, wszName, pNameLength, wszUnits, pUnitsLength, wszDescription, pDescriptionLength);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CheckCounter, hr, pDesc, pType, pActiveCounters, wszName, pNameLength, wszUnits, pUnitsLength, wszDescription, pDescriptionLength);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface, void** ppResource)
{
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(OpenSharedResource, hResource, ReturnedInterface, ppResource);
	HRESULT hr = m_pDevice->OpenSharedResource(hResource, ReturnedInterface, ppResource);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(OpenSharedResource, hr, hResource, ReturnedInterface, ppResource);
	return hr;
}

	#if (CRY_RENDERER_DIRECT3D >= 111)
///////////////////////////////////////////////////////////////////////////////
// below are functions which are only avilable on D3D11.1 and later
///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceWrapper::GetImmediateContext1(ID3D11DeviceContext1** ppImmediateContext)
{
		#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
		#else
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GetImmediateContext1, ppImmediateContext);
	m_pDevice->GetImmediateContext1(ppImmediateContext);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GetImmediateContext1, ppImmediateContext);
		#endif
}
///////////////////////////////////////////////////////////////////////////////
	#endif

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateTarget1D(const D3D11_TEXTURE1D_DESC* pDesc, const FLOAT ClearValue[4], const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateTarget1D, pDesc, ClearValue, pInitialData, ppTexture1D);
	CCryDX12Device* pDevice = (CCryDX12Device*)m_pDevice;
	HRESULT result = pDevice->CreateTarget1D(pDesc, ClearValue, pInitialData, ppTexture1D);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateTarget1D, result, pDesc, ClearValue, pInitialData, ppTexture1D);
	#else
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateTarget1D, pDesc, ClearValue, pInitialData, ppTexture1D);
	HRESULT result = m_pDevice->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateTarget1D, result, pDesc, ClearValue, pInitialData, ppTexture1D);
	#endif

	return result;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateTarget2D(const D3D11_TEXTURE2D_DESC* pDesc, const FLOAT ClearValue[4], const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateTarget2D, pDesc, ClearValue, pInitialData, ppTexture2D);
	CCryDX12Device* pDevice = (CCryDX12Device*)m_pDevice;
	HRESULT result = pDevice->CreateTarget2D(pDesc, ClearValue, pInitialData, ppTexture2D);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateTarget2D, result, pDesc, ClearValue, pInitialData, ppTexture2D);
	#else
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateTarget2D, pDesc, ClearValue, pInitialData, ppTexture2D);
	HRESULT result = m_pDevice->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateTarget2D, result, pDesc, ClearValue, pInitialData, ppTexture2D);
	#endif

	return result;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateTarget3D(const D3D11_TEXTURE3D_DESC* pDesc, const FLOAT ClearValue[4], const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateTarget3D, pDesc, ClearValue, pInitialData, ppTexture3D);
	CCryDX12Device* pDevice = (CCryDX12Device*)m_pDevice;
	HRESULT result = pDevice->CreateTarget3D(pDesc, ClearValue, pInitialData, ppTexture3D);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateTarget3D, result, pDesc, ClearValue, pInitialData, ppTexture3D);
	#else
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateTarget3D, pDesc, ClearValue, pInitialData, ppTexture3D);
	HRESULT result = m_pDevice->CreateTexture3D(pDesc, pInitialData, ppTexture3D);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateTarget3D, result, pDesc, ClearValue, pInitialData, ppTexture3D);
	#endif

	return result;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateNullResource(D3D11_RESOURCE_DIMENSION eType, ID3D11Resource** ppNullResource)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateNullResource, eType, ppNullResource);
	HRESULT result = m_pDevice->CreateNullResource(eType, ppNullResource);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateNullResource, result, eType, ppNullResource);
	#else
	HRESULT result = E_FAIL;
	CryFatalError("ReleaseNullResource not implemented right now any other than DX12");
	#endif

	return result;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::ReleaseNullResource(ID3D11Resource* pNullResource)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(ReleaseNullResource, pNullResource);
	HRESULT result = m_pDevice->ReleaseNullResource(pNullResource);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(ReleaseNullResource, result, pNullResource);
	#else
	HRESULT result = E_FAIL;
	CryFatalError("ReleaseNullResource not implemented right now any other than DX12");
	#endif

	return result;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::CreateStagingResource(ID3D11Resource* pInputResource, ID3D11Resource** ppStagingResource, BOOL Upload)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateStagingResource, pInputResource, ppStagingResource, Upload);
	HRESULT result = m_pDevice->CreateStagingResource(pInputResource, ppStagingResource, Upload);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateStagingResource, result, pInputResource, ppStagingResource, Upload);
	#else
	HRESULT result = E_FAIL;
	CryFatalError("ReleaseStagingResource not implemented right now any other than DX12");
	#endif

	return result;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceWrapper::ReleaseStagingResource(ID3D11Resource* pStagingResource)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(ReleaseStagingResource, pStagingResource);
	HRESULT result = m_pDevice->ReleaseStagingResource(pStagingResource);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(ReleaseStagingResource, result, pStagingResource);
	#else
	HRESULT result = E_FAIL;
	CryFatalError("ReleaseStagingResource not implemented right now any other than DX12");
	#endif

	return result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// CryDeviceContextWrapper methods
///////////////////////////////////////////////////////////////////////////////
inline CCryDeviceContextWrapper::CCryDeviceContextWrapper()
	: m_pDeviceContext(NULL)
	, m_pDeviceHooks(NULL)
{
}

///////////////////////////////////////////////////////////////////////////////
inline bool CCryDeviceContextWrapper::IsValid() const
{
	return m_pDeviceContext != NULL;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::AssignDeviceContext(D3DDeviceContext* pDeviceContext)
{
	if (m_pDeviceContext != NULL && m_pDeviceContext != pDeviceContext)
		CryFatalError("Trying to assign two difference devices context");

	#if (CRY_RENDERER_DIRECT3D >= 120)
	m_pDeviceContext = (CCryDX12DeviceContext*)pDeviceContext;
	#else
	m_pDeviceContext = (D3DDeviceContext*)pDeviceContext;
	#endif

	/***********************************/
	/*** ID3DDeviceContext delegates ***/

	mapSetShader[0] = (typeSetShader)&ID3D11DeviceContext::VSSetShader;
	mapSetShader[1] = (typeSetShader)&ID3D11DeviceContext::PSSetShader;
	mapSetShader[2] = (typeSetShader)&ID3D11DeviceContext::GSSetShader;
	mapSetShader[3] = (typeSetShader)&ID3D11DeviceContext::DSSetShader;
	mapSetShader[4] = (typeSetShader)&ID3D11DeviceContext::HSSetShader;
	mapSetShader[5] = (typeSetShader)&ID3D11DeviceContext::CSSetShader;

	mapGetShader[0] = (typeGetShader)&ID3D11DeviceContext::VSGetShader;
	mapGetShader[1] = (typeGetShader)&ID3D11DeviceContext::PSGetShader;
	mapGetShader[2] = (typeGetShader)&ID3D11DeviceContext::GSGetShader;
	mapGetShader[3] = (typeGetShader)&ID3D11DeviceContext::DSGetShader;
	mapGetShader[4] = (typeGetShader)&ID3D11DeviceContext::HSGetShader;
	mapGetShader[5] = (typeGetShader)&ID3D11DeviceContext::CSGetShader;

	mapSetSamplers[0] = (typeSetSamplers)&ID3D11DeviceContext::VSSetSamplers;
	mapSetSamplers[1] = (typeSetSamplers)&ID3D11DeviceContext::PSSetSamplers;
	mapSetSamplers[2] = (typeSetSamplers)&ID3D11DeviceContext::GSSetSamplers;
	mapSetSamplers[3] = (typeSetSamplers)&ID3D11DeviceContext::DSSetSamplers;
	mapSetSamplers[4] = (typeSetSamplers)&ID3D11DeviceContext::HSSetSamplers;
	mapSetSamplers[5] = (typeSetSamplers)&ID3D11DeviceContext::CSSetSamplers;

	mapGetSamplers[0] = (typeGetSamplers)&ID3D11DeviceContext::VSGetSamplers;
	mapGetSamplers[1] = (typeGetSamplers)&ID3D11DeviceContext::PSGetSamplers;
	mapGetSamplers[2] = (typeGetSamplers)&ID3D11DeviceContext::GSGetSamplers;
	mapGetSamplers[3] = (typeGetSamplers)&ID3D11DeviceContext::DSGetSamplers;
	mapGetSamplers[4] = (typeGetSamplers)&ID3D11DeviceContext::HSGetSamplers;
	mapGetSamplers[5] = (typeGetSamplers)&ID3D11DeviceContext::CSGetSamplers;

	mapSetConstantBuffers[0] = (typeSetConstantBuffers)&ID3D11DeviceContext::VSSetConstantBuffers;
	mapSetConstantBuffers[1] = (typeSetConstantBuffers)&ID3D11DeviceContext::PSSetConstantBuffers;
	mapSetConstantBuffers[2] = (typeSetConstantBuffers)&ID3D11DeviceContext::GSSetConstantBuffers;
	mapSetConstantBuffers[3] = (typeSetConstantBuffers)&ID3D11DeviceContext::DSSetConstantBuffers;
	mapSetConstantBuffers[4] = (typeSetConstantBuffers)&ID3D11DeviceContext::HSSetConstantBuffers;
	mapSetConstantBuffers[5] = (typeSetConstantBuffers)&ID3D11DeviceContext::CSSetConstantBuffers;

	mapGetConstantBuffers[0] = (typeGetConstantBuffers)&ID3D11DeviceContext::VSGetConstantBuffers;
	mapGetConstantBuffers[1] = (typeGetConstantBuffers)&ID3D11DeviceContext::PSGetConstantBuffers;
	mapGetConstantBuffers[2] = (typeGetConstantBuffers)&ID3D11DeviceContext::GSGetConstantBuffers;
	mapGetConstantBuffers[3] = (typeGetConstantBuffers)&ID3D11DeviceContext::DSGetConstantBuffers;
	mapGetConstantBuffers[4] = (typeGetConstantBuffers)&ID3D11DeviceContext::HSGetConstantBuffers;
	mapGetConstantBuffers[5] = (typeGetConstantBuffers)&ID3D11DeviceContext::CSGetConstantBuffers;

	mapSetShaderResources[0] = (typeSetShaderResources)&ID3D11DeviceContext::VSSetShaderResources;
	mapSetShaderResources[1] = (typeSetShaderResources)&ID3D11DeviceContext::PSSetShaderResources;
	mapSetShaderResources[2] = (typeSetShaderResources)&ID3D11DeviceContext::GSSetShaderResources;
	mapSetShaderResources[3] = (typeSetShaderResources)&ID3D11DeviceContext::DSSetShaderResources;
	mapSetShaderResources[4] = (typeSetShaderResources)&ID3D11DeviceContext::HSSetShaderResources;
	mapSetShaderResources[5] = (typeSetShaderResources)&ID3D11DeviceContext::CSSetShaderResources;

	mapGetShaderResources[0] = (typeGetShaderResources)&ID3D11DeviceContext::VSGetShaderResources;
	mapGetShaderResources[1] = (typeGetShaderResources)&ID3D11DeviceContext::PSGetShaderResources;
	mapGetShaderResources[2] = (typeGetShaderResources)&ID3D11DeviceContext::GSGetShaderResources;
	mapGetShaderResources[3] = (typeGetShaderResources)&ID3D11DeviceContext::DSGetShaderResources;
	mapGetShaderResources[4] = (typeGetShaderResources)&ID3D11DeviceContext::HSGetShaderResources;
	mapGetShaderResources[5] = (typeGetShaderResources)&ID3D11DeviceContext::CSGetShaderResources;

#if (CRY_RENDERER_DIRECT3D >= 111)
	mapSetConstantBuffers1[0] = (typeSetConstantBuffers1)&ID3D11DeviceContext1::VSSetConstantBuffers1;
	mapSetConstantBuffers1[1] = (typeSetConstantBuffers1)&ID3D11DeviceContext1::PSSetConstantBuffers1;
	mapSetConstantBuffers1[2] = (typeSetConstantBuffers1)&ID3D11DeviceContext1::GSSetConstantBuffers1;
	mapSetConstantBuffers1[3] = (typeSetConstantBuffers1)&ID3D11DeviceContext1::DSSetConstantBuffers1;
	mapSetConstantBuffers1[4] = (typeSetConstantBuffers1)&ID3D11DeviceContext1::HSSetConstantBuffers1;
	mapSetConstantBuffers1[5] = (typeSetConstantBuffers1)&ID3D11DeviceContext1::CSSetConstantBuffers1;

	mapGetConstantBuffers1[0] = (typeGetConstantBuffers1)&ID3D11DeviceContext1::VSGetConstantBuffers1;
	mapGetConstantBuffers1[1] = (typeGetConstantBuffers1)&ID3D11DeviceContext1::PSGetConstantBuffers1;
	mapGetConstantBuffers1[2] = (typeGetConstantBuffers1)&ID3D11DeviceContext1::GSGetConstantBuffers1;
	mapGetConstantBuffers1[3] = (typeGetConstantBuffers1)&ID3D11DeviceContext1::DSGetConstantBuffers1;
	mapGetConstantBuffers1[4] = (typeGetConstantBuffers1)&ID3D11DeviceContext1::HSGetConstantBuffers1;
	mapGetConstantBuffers1[5] = (typeGetConstantBuffers1)&ID3D11DeviceContext1::CSGetConstantBuffers1;
#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::SwitchToDeviceNode(UINT nodeMask)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	m_pDeviceContext = m_pDeviceContext->SetNode(nodeMask);
	#else
	//SELECT_GPU(nodeMask);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::ReleaseDeviceContext()
{
	if (m_pDeviceContext != NULL)
	{
		m_pDeviceContext->Release();
		m_pDeviceContext = NULL;

		/***********************************/
		/*** ID3DDeviceContext delegates ***/

		mapSetShader[0] = nullptr;
		mapSetShader[1] = nullptr;
		mapSetShader[2] = nullptr;
		mapSetShader[3] = nullptr;
		mapSetShader[4] = nullptr;
		mapSetShader[5] = nullptr;

		mapGetShader[0] = nullptr;
		mapGetShader[1] = nullptr;
		mapGetShader[2] = nullptr;
		mapGetShader[3] = nullptr;
		mapGetShader[4] = nullptr;
		mapGetShader[5] = nullptr;

		mapSetSamplers[0] = nullptr;
		mapSetSamplers[1] = nullptr;
		mapSetSamplers[2] = nullptr;
		mapSetSamplers[3] = nullptr;
		mapSetSamplers[4] = nullptr;
		mapSetSamplers[5] = nullptr;

		mapGetSamplers[0] = nullptr;
		mapGetSamplers[1] = nullptr;
		mapGetSamplers[2] = nullptr;
		mapGetSamplers[3] = nullptr;
		mapGetSamplers[4] = nullptr;
		mapGetSamplers[5] = nullptr;

		mapSetConstantBuffers[0] = nullptr;
		mapSetConstantBuffers[1] = nullptr;
		mapSetConstantBuffers[2] = nullptr;
		mapSetConstantBuffers[3] = nullptr;
		mapSetConstantBuffers[4] = nullptr;
		mapSetConstantBuffers[5] = nullptr;

		mapGetConstantBuffers[0] = nullptr;
		mapGetConstantBuffers[1] = nullptr;
		mapGetConstantBuffers[2] = nullptr;
		mapGetConstantBuffers[3] = nullptr;
		mapGetConstantBuffers[4] = nullptr;
		mapGetConstantBuffers[5] = nullptr;

		mapSetShaderResources[0] = nullptr;
		mapSetShaderResources[1] = nullptr;
		mapSetShaderResources[2] = nullptr;
		mapSetShaderResources[3] = nullptr;
		mapSetShaderResources[4] = nullptr;
		mapSetShaderResources[5] = nullptr;

		mapGetShaderResources[0] = nullptr;
		mapGetShaderResources[1] = nullptr;
		mapGetShaderResources[2] = nullptr;
		mapGetShaderResources[3] = nullptr;
		mapGetShaderResources[4] = nullptr;
		mapGetShaderResources[5] = nullptr;

#if (CRY_RENDERER_DIRECT3D >= 111)
		mapSetConstantBuffers1[0] = nullptr;
		mapSetConstantBuffers1[1] = nullptr;
		mapSetConstantBuffers1[2] = nullptr;
		mapSetConstantBuffers1[3] = nullptr;
		mapSetConstantBuffers1[4] = nullptr;
		mapSetConstantBuffers1[5] = nullptr;

		mapGetConstantBuffers1[0] = nullptr;
		mapGetConstantBuffers1[1] = nullptr;
		mapGetConstantBuffers1[2] = nullptr;
		mapGetConstantBuffers1[3] = nullptr;
		mapGetConstantBuffers1[4] = nullptr;
		mapGetConstantBuffers1[5] = nullptr;
#endif
	}
}

///////////////////////////////////////////////////////////////////////////////
inline D3DDeviceContext* CCryDeviceContextWrapper::GetRealDeviceContext() const
{
	return m_pDeviceContext;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::RegisterHook(ICryDeviceWrapperHook* pDeviceWrapperHook)
{
	pDeviceWrapperHook->SetNext(m_pDeviceHooks);
	m_pDeviceHooks = pDeviceWrapperHook;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::UnregisterHook(const char* pDeviceHookName)
{
	ICryDeviceWrapperHook* pDeviceHook = m_pDeviceHooks;

	if (pDeviceHook == NULL)
		return;

	if (strcmp(pDeviceHook->Name(), pDeviceHookName) == 0)
	{
		m_pDeviceHooks = pDeviceHook->GetNext();
		return;
	}

	while (pDeviceHook->GetNext())
	{
		if (strcmp(pDeviceHook->GetNext()->Name(), pDeviceHookName) == 0)
		{
			pDeviceHook->SetNext(pDeviceHook->GetNext()->GetNext());
			return;
		}
		pDeviceHook = pDeviceHook->GetNext();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceContextWrapper::QueryInterface(REFIID riid, void** ppvObj)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(QueryInterface_Context, riid, ppvObj);
	HRESULT hr = m_pDeviceContext->QueryInterface(riid, ppvObj);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(QueryInterface_Context, hr, riid, ppvObj);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline ULONG CCryDeviceContextWrapper::AddRef()
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(AddRef_Context);
	ULONG nResult = m_pDeviceContext->AddRef();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(AddRef_Context, nResult);
	return nResult;
}

///////////////////////////////////////////////////////////////////////////////
inline ULONG CCryDeviceContextWrapper::Release()
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(Release_Context);
	ULONG nResult = m_pDeviceContext->Release();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(Release_Context, nResult);
	return nResult;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::GetDevice(ID3D11Device** ppDevice)
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GetDevice, ppDevice);
	m_pDeviceContext->GetDevice(ppDevice);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GetDevice, ppDevice);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceContextWrapper::GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GetPrivateData_Context, guid, pDataSize, pData);
	HRESULT hr = m_pDeviceContext->GetPrivateData(guid, pDataSize, pData);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GetPrivateData_Context, hr, guid, pDataSize, pData);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceContextWrapper::SetPrivateData(REFGUID guid, UINT DataSize, const void* pData)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(SetPrivateData_Context, guid, DataSize, pData);
	HRESULT hr = m_pDeviceContext->SetPrivateData(guid, DataSize, pData);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(SetPrivateData_Context, hr, guid, DataSize, pData);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceContextWrapper::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(SetPrivateDataInterface_Context, guid, pData);
	HRESULT hr = m_pDeviceContext->SetPrivateDataInterface(guid, pData);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(SetPrivateDataInterface_Context, hr, guid, pData);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::TSSetShader(int type, ID3D11DeviceChild* pShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)
{
	(m_pDeviceContext->*mapSetShader[type])(pShader, ppClassInstances, NumClassInstances);
}

inline void CCryDeviceContextWrapper::TSGetShader(int type, ID3D11DeviceChild** ppShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)
{
	(m_pDeviceContext->*mapGetShader[type])(ppShader, ppClassInstances, pNumClassInstances);
}

inline void CCryDeviceContextWrapper::TSSetSamplers(int type, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)
{
	(m_pDeviceContext->*mapSetSamplers[type])(StartSlot, NumSamplers, ppSamplers);
}

inline void CCryDeviceContextWrapper::TSGetSamplers(int type, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)
{
	(m_pDeviceContext->*mapGetSamplers[type])(StartSlot, NumSamplers, ppSamplers);
}

inline void CCryDeviceContextWrapper::TSSetConstantBuffers(int type, UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)
{
	(m_pDeviceContext->*mapSetConstantBuffers[type])(StartSlot, NumBuffers, ppConstantBuffers);
}

inline void CCryDeviceContextWrapper::TSGetConstantBuffers(int type, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)
{
	(m_pDeviceContext->*mapGetConstantBuffers[type])(StartSlot, NumBuffers, ppConstantBuffers);
}

inline void CCryDeviceContextWrapper::TSSetShaderResources(int type, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	(m_pDeviceContext->*mapSetShaderResources[type])(StartSlot, NumViews, ppShaderResourceViews);
}

inline void CCryDeviceContextWrapper::TSGetShaderResources(int type, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)
{
	(m_pDeviceContext->*mapGetShaderResources[type])(StartSlot, NumViews, ppShaderResourceViews);
}

#if (CRY_RENDERER_DIRECT3D >= 111)
inline void CCryDeviceContextWrapper::TSSetConstantBuffers1(int type, UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)
{
	(m_pDeviceContext->*mapSetConstantBuffers1[type])(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

inline void CCryDeviceContextWrapper::TSGetConstantBuffers1(int type, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)
{
	(m_pDeviceContext->*mapGetConstantBuffers1[type])(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}
#endif

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::PSSetShader(ID3D11PixelShader* pPixelShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PSSetShader, pPixelShader, ppClassInstances, NumClassInstances);
	m_pDeviceContext->PSSetShader(pPixelShader, ppClassInstances, NumClassInstances);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PSSetShader, pPixelShader, ppClassInstances, NumClassInstances);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::PSGetShader(ID3D11PixelShader** ppPixelShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PSGetShader, ppPixelShader, ppClassInstances, pNumClassInstances);
	m_pDeviceContext->PSGetShader(ppPixelShader, ppClassInstances, pNumClassInstances);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PSGetShader, ppPixelShader, ppClassInstances, pNumClassInstances);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PSSetSamplers, StartSlot, NumSamplers, ppSamplers);
	m_pDeviceContext->PSSetSamplers(StartSlot, NumSamplers, ppSamplers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PSSetSamplers, StartSlot, NumSamplers, ppSamplers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PSGetSamplers, StartSlot, NumSamplers, ppSamplers);
	m_pDeviceContext->PSGetSamplers(StartSlot, NumSamplers, ppSamplers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PSGetSamplers, StartSlot, NumSamplers, ppSamplers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PSSetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
	m_pDeviceContext->PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PSSetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PSGetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
	m_pDeviceContext->PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PSGetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PSSetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
	m_pDeviceContext->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PSSetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PSGetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
	m_pDeviceContext->PSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PSGetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::VSSetShader(ID3D11VertexShader* pVertexShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(VSSetShader, pVertexShader, ppClassInstances, NumClassInstances);
	m_pDeviceContext->VSSetShader(pVertexShader, ppClassInstances, NumClassInstances);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(VSSetShader, pVertexShader, ppClassInstances, NumClassInstances);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::VSGetShader(ID3D11VertexShader** ppVertexShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(VSGetShader, ppVertexShader, ppClassInstances, pNumClassInstances);
	m_pDeviceContext->VSGetShader(ppVertexShader, ppClassInstances, pNumClassInstances);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(VSGetShader, ppVertexShader, ppClassInstances, pNumClassInstances);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(VSSetSamplers, StartSlot, NumSamplers, ppSamplers);
	m_pDeviceContext->VSSetSamplers(StartSlot, NumSamplers, ppSamplers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(VSSetSamplers, StartSlot, NumSamplers, ppSamplers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(VSGetSamplers, StartSlot, NumSamplers, ppSamplers);
	m_pDeviceContext->VSGetSamplers(StartSlot, NumSamplers, ppSamplers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(VSGetSamplers, StartSlot, NumSamplers, ppSamplers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(VSSetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
	m_pDeviceContext->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(VSSetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(VSGetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
	m_pDeviceContext->VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(VSGetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(VSSetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
	m_pDeviceContext->VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(VSSetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(VSGetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
	m_pDeviceContext->VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(VSGetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::GSSetShader(ID3D11GeometryShader* pShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GSSetShader, pShader, ppClassInstances, NumClassInstances);
	m_pDeviceContext->GSSetShader(pShader, ppClassInstances, NumClassInstances);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GSSetShader, pShader, ppClassInstances, NumClassInstances);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::GSGetShader(ID3D11GeometryShader** ppGeometryShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GSGetShader, ppGeometryShader, ppClassInstances, pNumClassInstances);
	m_pDeviceContext->GSGetShader(ppGeometryShader, ppClassInstances, pNumClassInstances);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GSGetShader, ppGeometryShader, ppClassInstances, pNumClassInstances);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GSSetSamplers, StartSlot, NumSamplers, ppSamplers);
	m_pDeviceContext->GSSetSamplers(StartSlot, NumSamplers, ppSamplers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GSSetSamplers, StartSlot, NumSamplers, ppSamplers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GSGetSamplers, StartSlot, NumSamplers, ppSamplers);
	m_pDeviceContext->GSGetSamplers(StartSlot, NumSamplers, ppSamplers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GSGetSamplers, StartSlot, NumSamplers, ppSamplers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GSSetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
	m_pDeviceContext->GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GSSetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GSGetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
	m_pDeviceContext->GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GSGetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GSSetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
	m_pDeviceContext->GSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GSSetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GSGetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
	m_pDeviceContext->GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GSGetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::HSSetShader(ID3D11HullShader* pHullShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSSetShader, pHullShader, ppClassInstances, NumClassInstances);
	m_pDeviceContext->HSSetShader(pHullShader, ppClassInstances, NumClassInstances);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSSetShader, pHullShader, ppClassInstances, NumClassInstances);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::HSGetShader(ID3D11HullShader** ppHullShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSGetShader, ppHullShader, ppClassInstances, pNumClassInstances);
	m_pDeviceContext->HSGetShader(ppHullShader, ppClassInstances, pNumClassInstances);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSGetShader, ppHullShader, ppClassInstances, pNumClassInstances);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::HSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSSetSamplers, StartSlot, NumSamplers, ppSamplers);
	m_pDeviceContext->HSSetSamplers(StartSlot, NumSamplers, ppSamplers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSSetSamplers, StartSlot, NumSamplers, ppSamplers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::HSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSGetSamplers, StartSlot, NumSamplers, ppSamplers);
	m_pDeviceContext->HSGetSamplers(StartSlot, NumSamplers, ppSamplers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSGetSamplers, StartSlot, NumSamplers, ppSamplers);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSSetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
	m_pDeviceContext->HSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSSetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSGetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
	m_pDeviceContext->HSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSGetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::HSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSSetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
	m_pDeviceContext->HSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSSetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::HSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSGetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
	m_pDeviceContext->HSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSGetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DSSetShader(ID3D11DomainShader* pDomainShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DSSetShader, pDomainShader, ppClassInstances, NumClassInstances);
	m_pDeviceContext->DSSetShader(pDomainShader, ppClassInstances, NumClassInstances);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DSSetShader, pDomainShader, ppClassInstances, NumClassInstances);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DSGetShader(ID3D11DomainShader** ppDomainShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DSGetShader, ppDomainShader, ppClassInstances, pNumClassInstances);
	m_pDeviceContext->DSGetShader(ppDomainShader, ppClassInstances, pNumClassInstances);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DSGetShader, ppDomainShader, ppClassInstances, pNumClassInstances);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DSSetSamplers, StartSlot, NumSamplers, ppSamplers);
	m_pDeviceContext->DSSetSamplers(StartSlot, NumSamplers, ppSamplers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DSSetSamplers, StartSlot, NumSamplers, ppSamplers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DSGetSamplers, StartSlot, NumSamplers, ppSamplers);
	m_pDeviceContext->DSGetSamplers(StartSlot, NumSamplers, ppSamplers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DSGetSamplers, StartSlot, NumSamplers, ppSamplers);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DSSetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
	m_pDeviceContext->DSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DSSetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DSGetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
	m_pDeviceContext->DSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DSGetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DSSetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
	m_pDeviceContext->DSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DSSetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DSGetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
	m_pDeviceContext->DSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DSGetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CSSetShader(ID3D11ComputeShader* pComputeShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSSetShader, pComputeShader, ppClassInstances, NumClassInstances);
	m_pDeviceContext->CSSetShader(pComputeShader, ppClassInstances, NumClassInstances);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSSetShader, pComputeShader, ppClassInstances, NumClassInstances);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CSGetShader(ID3D11ComputeShader** ppComputeShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSGetShader, ppComputeShader, ppClassInstances, pNumClassInstances);
	m_pDeviceContext->CSGetShader(ppComputeShader, ppClassInstances, pNumClassInstances);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSGetShader, ppComputeShader, ppClassInstances, pNumClassInstances);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSSetSamplers, StartSlot, NumSamplers, ppSamplers);
	m_pDeviceContext->CSSetSamplers(StartSlot, NumSamplers, ppSamplers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSSetSamplers, StartSlot, NumSamplers, ppSamplers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSGetSamplers, StartSlot, NumSamplers, ppSamplers);
	m_pDeviceContext->CSGetSamplers(StartSlot, NumSamplers, ppSamplers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSGetSamplers, StartSlot, NumSamplers, ppSamplers);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSSetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
	m_pDeviceContext->CSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSSetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSGetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
	m_pDeviceContext->CSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSGetConstantBuffers, StartSlot, NumBuffers, ppConstantBuffers);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSSetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
	m_pDeviceContext->CSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSSetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSGetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
	m_pDeviceContext->CSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSGetShaderResources, StartSlot, NumViews, ppShaderResourceViews);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CSSetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSSetUnorderedAccessViews, StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
	m_pDeviceContext->CSSetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSSetUnorderedAccessViews, StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CSGetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews)
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSGetUnorderedAccessViews, StartSlot, NumUAVs, ppUnorderedAccessViews);
	m_pDeviceContext->CSGetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSGetUnorderedAccessViews, StartSlot, NumUAVs, ppUnorderedAccessViews);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DrawAuto()
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DrawAuto);
	m_pDeviceContext->DrawAuto();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DrawAuto, );
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::Draw(UINT VertexCount, UINT StartVertexLocation)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(Draw, VertexCount, StartVertexLocation);
	m_pDeviceContext->Draw(VertexCount, StartVertexLocation);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(Draw, VertexCount, StartVertexLocation);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DrawInstanced, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	m_pDeviceContext->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DrawInstanced, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DrawInstancedIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DrawInstancedIndirect, pBufferForArgs, AlignedByteOffsetForArgs);
	m_pDeviceContext->DrawInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DrawInstancedIndirect, pBufferForArgs, AlignedByteOffsetForArgs);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DrawIndexed, IndexCount, StartIndexLocation, BaseVertexLocation);
	m_pDeviceContext->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DrawIndexed, IndexCount, StartIndexLocation, BaseVertexLocation);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DrawIndexedInstanced, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
	m_pDeviceContext->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DrawIndexedInstanced, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DrawIndexedInstancedIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DrawIndexedInstancedIndirect, pBufferForArgs, AlignedByteOffsetForArgs);
	m_pDeviceContext->DrawIndexedInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DrawIndexedInstancedIndirect, pBufferForArgs, AlignedByteOffsetForArgs);
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceContextWrapper::Map(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(Map, pResource, Subresource, MapType, MapFlags, pMappedResource);
	HRESULT hr = m_pDeviceContext->Map(pResource, Subresource, MapType, MapFlags, pMappedResource);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(Map, hr, pResource, Subresource, MapType, MapFlags, pMappedResource);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceContextWrapper::Map(ID3D11Resource* pResource, UINT Subresource, SIZE_T* BeginEnd, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(Map, pResource, Subresource, MapType, MapFlags, pMappedResource);
#if (CRY_RENDERER_DIRECT3D >= 120)
	HRESULT hr = m_pDeviceContext->Map(pResource, Subresource, BeginEnd, MapType, MapFlags, pMappedResource);
#else
	HRESULT hr = m_pDeviceContext->Map(pResource, Subresource, MapType, MapFlags, pMappedResource);
#endif
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(Map, hr, pResource, Subresource, MapType, MapFlags, pMappedResource);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::Unmap(ID3D11Resource* pResource, UINT Subresource)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(Unmap, pResource, Subresource);
	m_pDeviceContext->Unmap(pResource, Subresource);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(Unmap, pResource, Subresource);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::Unmap(ID3D11Resource* pResource, UINT Subresource, SIZE_T* BeginEnd)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(Unmap, pResource, Subresource);
#if (CRY_RENDERER_DIRECT3D >= 120)
	m_pDeviceContext->Unmap(pResource, Subresource, BeginEnd);
#else
	m_pDeviceContext->Unmap(pResource, Subresource);
#endif
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(Unmap, pResource, Subresource);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::IASetInputLayout(ID3D11InputLayout* pInputLayout)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(IASetInputLayout, pInputLayout);
	m_pDeviceContext->IASetInputLayout(pInputLayout);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(IASetInputLayout, pInputLayout);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::IAGetInputLayout(ID3D11InputLayout** ppInputLayout)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(IAGetInputLayout, ppInputLayout);
	m_pDeviceContext->IAGetInputLayout(ppInputLayout);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(IAGetInputLayout, ppInputLayout);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const UINT* pStrides, const UINT* pOffsets)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(IASetVertexBuffers, StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
	m_pDeviceContext->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(IASetVertexBuffers, StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppVertexBuffers, UINT* pStrides, UINT* pOffsets)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(IAGetVertexBuffers, StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
	m_pDeviceContext->IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(IAGetVertexBuffers, StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::IASetIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, UINT Offset)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(IASetIndexBuffer, pIndexBuffer, Format, Offset);
	m_pDeviceContext->IASetIndexBuffer(pIndexBuffer, Format, Offset);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(IASetIndexBuffer, pIndexBuffer, Format, Offset);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::IAGetIndexBuffer(ID3D11Buffer** pIndexBuffer, DXGI_FORMAT* Format, UINT* Offset)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(IAGetIndexBuffer, pIndexBuffer, Format, Offset);
	m_pDeviceContext->IAGetIndexBuffer(pIndexBuffer, Format, Offset);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(IAGetIndexBuffer, pIndexBuffer, Format, Offset);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(IASetPrimitiveTopology, Topology);
	m_pDeviceContext->IASetPrimitiveTopology(Topology);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(IASetPrimitiveTopology, Topology);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* pTopology)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(IAGetPrimitiveTopology, pTopology);
	m_pDeviceContext->IAGetPrimitiveTopology(pTopology);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(IAGetPrimitiveTopology, pTopology);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::Begin(ID3D11Asynchronous* pAsync)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(Begin, pAsync);
	m_pDeviceContext->Begin(pAsync);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(Begin, pAsync);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::End(ID3D11Asynchronous* pAsync)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(End, pAsync);
	m_pDeviceContext->End(pAsync);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(End, pAsync);
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceContextWrapper::GetData(ID3D11Asynchronous* pAsync, void* pData, UINT DataSize, UINT GetDataFlags)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GetData, pAsync, pData, DataSize, GetDataFlags);
	HRESULT hr = m_pDeviceContext->GetData(pAsync, pData, DataSize, GetDataFlags);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GetData, hr, pAsync, pData, DataSize, GetDataFlags);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::SetPredication(ID3D11Predicate* pPredicate, BOOL PredicateValue)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(SetPredication, pPredicate, PredicateValue);
	m_pDeviceContext->SetPredication(pPredicate, PredicateValue);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(SetPredication, pPredicate, PredicateValue);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::GetPredication(ID3D11Predicate** ppPredicate, BOOL* pPredicateValue)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GetPredication, ppPredicate, pPredicateValue);
	m_pDeviceContext->GetPredication(ppPredicate, pPredicateValue);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GetPredication, ppPredicate, pPredicateValue);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::OMSetRenderTargets(UINT NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(OMSetRenderTargets, NumViews, ppRenderTargetViews, pDepthStencilView);
	m_pDeviceContext->OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(OMSetRenderTargets, NumViews, ppRenderTargetViews, pDepthStencilView);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::OMGetRenderTargets(UINT NumViews, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(OMGetRenderTargets, NumViews, ppRenderTargetViews, ppDepthStencilView);
	m_pDeviceContext->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(OMGetRenderTargets, NumViews, ppRenderTargetViews, ppDepthStencilView);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::OMSetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(OMSetRenderTargetsAndUnorderedAccessViews, NumRTVs, ppRenderTargetViews, pDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
	m_pDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, pDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(OMSetRenderTargetsAndUnorderedAccessViews, NumRTVs, ppRenderTargetViews, pDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::OMGetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews)
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(OMGetRenderTargetsAndUnorderedAccessViews, NumRTVs, ppRenderTargetViews, ppDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews);
	m_pDeviceContext->OMGetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, ppDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(OMGetRenderTargetsAndUnorderedAccessViews, NumRTVs, ppRenderTargetViews, ppDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::OMSetBlendState(ID3D11BlendState* pBlendState, const FLOAT BlendFactor[4], UINT SampleMask)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(OMSetBlendState, pBlendState, BlendFactor, SampleMask);
	m_pDeviceContext->OMSetBlendState(pBlendState, BlendFactor, SampleMask);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(OMSetBlendState, pBlendState, BlendFactor, SampleMask);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::OMGetBlendState(ID3D11BlendState** ppBlendState, FLOAT BlendFactor[4], UINT* pSampleMask)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(OMGetBlendState, ppBlendState, BlendFactor, pSampleMask);
	m_pDeviceContext->OMGetBlendState(ppBlendState, BlendFactor, pSampleMask);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(OMGetBlendState, ppBlendState, BlendFactor, pSampleMask);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::OMSetDepthStencilState(ID3D11DepthStencilState* pDepthStencilState, UINT StencilRef)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(OMSetDepthStencilState, pDepthStencilState, StencilRef);
	m_pDeviceContext->OMSetDepthStencilState(pDepthStencilState, StencilRef);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(OMSetDepthStencilState, pDepthStencilState, StencilRef);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::OMGetDepthStencilState(ID3D11DepthStencilState** ppDepthStencilState, UINT* pStencilRef)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(OMGetDepthStencilState, ppDepthStencilState, pStencilRef);
	m_pDeviceContext->OMGetDepthStencilState(ppDepthStencilState, pStencilRef);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(OMGetDepthStencilState, ppDepthStencilState, pStencilRef);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::SOSetTargets(UINT NumBuffers, ID3D11Buffer* const* ppSOTargets, const UINT* pOffsets)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(SOSetTargets, NumBuffers, ppSOTargets, pOffsets);
	m_pDeviceContext->SOSetTargets(NumBuffers, ppSOTargets, pOffsets);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(SOSetTargets, NumBuffers, ppSOTargets, pOffsets);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::SOGetTargets(UINT NumBuffers, ID3D11Buffer** ppSOTargets)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(SOGetTargets, NumBuffers, ppSOTargets);
	m_pDeviceContext->SOGetTargets(NumBuffers, ppSOTargets);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(SOGetTargets, NumBuffers, ppSOTargets);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(Dispatch, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	m_pDeviceContext->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(Dispatch, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DispatchIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DispatchIndirect, pBufferForArgs, AlignedByteOffsetForArgs);
	m_pDeviceContext->DispatchIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DispatchIndirect, pBufferForArgs, AlignedByteOffsetForArgs);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::RSSetState(ID3D11RasterizerState* pRasterizerState)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(RSSetState, pRasterizerState);
	m_pDeviceContext->RSSetState(pRasterizerState);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(RSSetState, pRasterizerState);
}

inline void CCryDeviceContextWrapper::ResetCachedState(bool bGraphics, bool bCompute)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	#if (CRY_RENDERER_DIRECT3D >= 120)
	m_pDeviceContext->ResetCachedState(bGraphics, bCompute);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::RSGetState(ID3D11RasterizerState** ppRasterizerState)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(RSGetState, ppRasterizerState);
	m_pDeviceContext->RSGetState(ppRasterizerState);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(RSGetState, ppRasterizerState);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::RSSetViewports(UINT NumViewports, const D3D11_VIEWPORT* pViewports)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(RSSetViewports, NumViewports, pViewports);
	m_pDeviceContext->RSSetViewports(NumViewports, pViewports);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(RSSetViewports, NumViewports, pViewports);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::RSGetViewports(UINT* pNumViewports, D3D11_VIEWPORT* pViewports)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(RSGetViewports, pNumViewports, pViewports);
	m_pDeviceContext->RSGetViewports(pNumViewports, pViewports);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(RSGetViewports, pNumViewports, pViewports);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::RSSetScissorRects(UINT NumRects, const D3D11_RECT* pRects)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(RSSetScissorRects, NumRects, pRects);
	m_pDeviceContext->RSSetScissorRects(NumRects, pRects);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(RSSetScissorRects, NumRects, pRects);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::RSGetScissorRects(UINT* pNumRects, D3D11_RECT* pRects)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(RSGetScissorRects, pNumRects, pRects);
	m_pDeviceContext->RSGetScissorRects(pNumRects, pRects);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(RSGetScissorRects, pNumRects, pRects);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CopyResource(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CopyResource, pDstResource, pSrcResource);
	m_pDeviceContext->CopyResource(pDstResource, pSrcResource);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CopyResource, pDstResource, pSrcResource);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CopySubresourceRegion(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CopySubresourceRegion, pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
	m_pDeviceContext->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CopySubresourceRegion, pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CopySubresourcesRegion(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT NumSubresources)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();

#if (CRY_RENDERER_DIRECT3D >= 120)
	{
		_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CopySubresourcesRegion, pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, NumSubresources);
		m_pDeviceContext->CopySubresourcesRegion(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, NumSubresources);
		_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CopySubresourcesRegion, pDstResource, DstSubresource , DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, NumSubresources);
	}
#else
	if (NumSubresources <= 1)
	{
		_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CopySubresourceRegion, pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
		m_pDeviceContext->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
		_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CopySubresourceRegion, pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
	}
	else
	{
		D3D11_RESOURCE_DIMENSION srcDim;  pSrcResource->GetType(&srcDim);
		D3D11_RESOURCE_DIMENSION dstDim;  pDstResource->GetType(&dstDim);
		UINT srcMipLevels = 0;
		UINT dstMipLevels = 0;
		if (srcDim == D3D11_RESOURCE_DIMENSION_TEXTURE1D) { D3D11_TEXTURE1D_DESC srcDesc; ((ID3D11Texture1D*)pSrcResource)->GetDesc(&srcDesc); srcMipLevels = srcDesc.MipLevels; }
		if (srcDim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) { D3D11_TEXTURE2D_DESC srcDesc; ((ID3D11Texture2D*)pSrcResource)->GetDesc(&srcDesc); srcMipLevels = srcDesc.MipLevels; }
		if (srcDim == D3D11_RESOURCE_DIMENSION_TEXTURE3D) { D3D11_TEXTURE3D_DESC srcDesc; ((ID3D11Texture3D*)pSrcResource)->GetDesc(&srcDesc); srcMipLevels = srcDesc.MipLevels; }
		if (dstDim == D3D11_RESOURCE_DIMENSION_TEXTURE1D) { D3D11_TEXTURE1D_DESC dstDesc; ((ID3D11Texture1D*)pDstResource)->GetDesc(&dstDesc); dstMipLevels = dstDesc.MipLevels; }
		if (dstDim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) { D3D11_TEXTURE2D_DESC dstDesc; ((ID3D11Texture2D*)pDstResource)->GetDesc(&dstDesc); dstMipLevels = dstDesc.MipLevels; }
		if (dstDim == D3D11_RESOURCE_DIMENSION_TEXTURE3D) { D3D11_TEXTURE3D_DESC dstDesc; ((ID3D11Texture3D*)pDstResource)->GetDesc(&dstDesc); dstMipLevels = dstDesc.MipLevels; }

		D3D11_BOX srcBoxBck = { 0 };
		D3D11_BOX srcRegion = { 0 };
		D3D11_BOX dstRegion = { DstX, DstY, DstZ };
		if (pSrcBox)
		{
			srcBoxBck = *pSrcBox;
			srcRegion = *pSrcBox;
			pSrcBox = &srcRegion;
		}

		// NOTE: Copying multiple slices is too complex a case, which is not supported as it leads to
		// fe. [slice,mip] sequences like [0,4],[0,5],[0,6],[1,0],[1,1],...
		// which we don't support because the offsets and dimensions are relative to a intermediate
		// mip-level, while crossing the slice-boundary forces us to extrapolate dimensions to larger
		// mips, which is probably not what is wanted in the first place.
		//
		// Verify that the sub-resources don't wrap around into the next slice. Only copies of mips
		// within the same slice are allowed!
		CRY_ASSERT(((SrcSubresource + 0) / (srcMipLevels)) == ((SrcSubresource + NumSubresources - 1) / (srcMipLevels)));
		CRY_ASSERT(((DstSubresource + 0) / (dstMipLevels)) == ((DstSubresource + NumSubresources - 1) / (dstMipLevels)));

		for (UINT n = 0; n < NumSubresources; ++n)
		{
			const UINT srcSlice = (SrcSubresource + n) / (srcMipLevels);
			const UINT dstSlice = (DstSubresource + n) / (dstMipLevels);
			const UINT srcLevel = (SrcSubresource + n) % (srcMipLevels);
			const UINT dstLevel = (DstSubresource + n) % (dstMipLevels);

			// reset dimensions/coordinates when crossing slice-boundary
			if (!srcLevel)
				srcRegion = srcBoxBck;
			if (!dstLevel)
				dstRegion = { DstX, DstY, DstZ };

			_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CopySubresourceRegion, pDstResource, DstSubresource + n, dstRegion.left, dstRegion.top, dstRegion.front, pSrcResource, SrcSubresource + n, pSrcBox);
			m_pDeviceContext->CopySubresourceRegion(pDstResource, DstSubresource + n, dstRegion.left, dstRegion.top, dstRegion.front, pSrcResource, SrcSubresource + n, pSrcBox);
			_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CopySubresourceRegion, pDstResource, DstSubresource + n, dstRegion.left, dstRegion.top, dstRegion.front, pSrcResource, SrcSubresource + n, pSrcBox);

			srcRegion.left   >>= 1;
			srcRegion.top    >>= 1;
			srcRegion.front  >>= 1;
			
			srcRegion.right  >>= 1; if (srcRegion.right  == srcRegion.left ) srcRegion.right  = srcRegion.left + 1;
			srcRegion.bottom >>= 1; if (srcRegion.bottom == srcRegion.top  ) srcRegion.bottom = srcRegion.top  + 1;
			srcRegion.back   >>= 1; if (srcRegion.back   == srcRegion.front) srcRegion.back   = srcRegion.front+ 1;
				
			dstRegion.left   >>= 1;
			dstRegion.top    >>= 1;
			dstRegion.front  >>= 1;
		}
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::UpdateSubresource(ID3D11Resource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(UpdateSubresource, pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
	m_pDeviceContext->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(UpdateSubresource, pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CopyStructureCount(ID3D11Buffer* pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView* pSrcView)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CopyStructureCount, pDstBuffer, DstAlignedByteOffset, pSrcView);
	m_pDeviceContext->CopyStructureCount(pDstBuffer, DstAlignedByteOffset, pSrcView);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CopyStructureCount, pDstBuffer, DstAlignedByteOffset, pSrcView);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::ClearRenderTargetView(ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4])
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(ClearRenderTargetView, pRenderTargetView, ColorRGBA);
	m_pDeviceContext->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(ClearRenderTargetView, pRenderTargetView, ColorRGBA);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView* pUnorderedAccessView, const UINT Values[4])
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(ClearUnorderedAccessViewUint, pUnorderedAccessView, Values);
	m_pDeviceContext->ClearUnorderedAccessViewUint(pUnorderedAccessView, Values);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(ClearUnorderedAccessViewUint, pUnorderedAccessView, Values);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView* pUnorderedAccessView, const FLOAT Values[4])
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(ClearUnorderedAccessViewFloat, pUnorderedAccessView, Values);
	m_pDeviceContext->ClearUnorderedAccessViewFloat(pUnorderedAccessView, Values);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(ClearUnorderedAccessViewFloat, pUnorderedAccessView, Values);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::ClearDepthStencilView(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(ClearDepthStencilView, pDepthStencilView, ClearFlags, Depth, Stencil);
	m_pDeviceContext->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(ClearDepthStencilView, pDepthStencilView, ClearFlags, Depth, Stencil);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::GenerateMips(ID3D11ShaderResourceView* pShaderResourceView)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GenerateMips, pShaderResourceView);
	m_pDeviceContext->GenerateMips(pShaderResourceView);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GenerateMips, pShaderResourceView);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::SetResourceMinLOD(ID3D11Resource* pResource, FLOAT MinLOD)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(SetResourceMinLOD, pResource, MinLOD);
	m_pDeviceContext->SetResourceMinLOD(pResource, MinLOD);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(SetResourceMinLOD, pResource, MinLOD);
}

///////////////////////////////////////////////////////////////////////////////
inline FLOAT CCryDeviceContextWrapper::GetResourceMinLOD(ID3D11Resource* pResource)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GetResourceMinLOD, pResource);
	FLOAT f = m_pDeviceContext->GetResourceMinLOD(pResource);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GetResourceMinLOD, pResource);
	return f;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::ResolveSubresource(ID3D11Resource* pDstResource, UINT DstSubresource, ID3D11Resource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(ResolveSubresource, pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
	m_pDeviceContext->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(ResolveSubresource, pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}

	#if !CRY_PLATFORM_ORBIS

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::ExecuteCommandList(ID3D11CommandList* pCommandList, BOOL RestoreContextState)
{
		#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
		#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(ExecuteCommandList, pCommandList, RestoreContextState);
	m_pDeviceContext->ExecuteCommandList(pCommandList, RestoreContextState);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(ExecuteCommandList, pCommandList, RestoreContextState);
		#endif
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceContextWrapper::FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList** ppCommandList)
{
		#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	return HRESULT();
		#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(FinishCommandList, RestoreDeferredContextState, ppCommandList);
	HRESULT result = m_pDeviceContext->FinishCommandList(RestoreDeferredContextState, ppCommandList);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(FinishCommandList, RestoreDeferredContextState, ppCommandList);
	return result;
		#endif
}

	#endif

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::SetModifiedWMode(bool enabled, uint32_t numViewports, const float* pA, const float* pB)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();

#if defined(USE_NV_API) && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)

	NV_MODIFIED_W_PARAMS modifiedWParams = { };
	modifiedWParams.version = NV_MODIFIED_W_PARAMS_VER;

	if (enabled && numViewports)
	{
		assert(pA != nullptr && pB != nullptr);

		modifiedWParams.numEntries = numViewports;

		for (uint32_t vp = 0; vp < numViewports; vp++)
		{
			modifiedWParams.modifiedWCoefficients[vp].fA = pA[vp];
			modifiedWParams.modifiedWCoefficients[vp].fB = pB[vp];
		}
	}
	else
	{
		modifiedWParams.numEntries = 0;
	}

	NvAPI_D3D_SetModifiedWMode(m_pDeviceContext, &modifiedWParams);

#endif
}


///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::QueryNvidiaProjectionFeatureSupport(bool& bMultiResSupported, bool& bLensMatchedSupported)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();

	bMultiResSupported = false;
	bLensMatchedSupported = false;

#if defined(USE_NV_API) && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	
	if (NvAPI_Initialize() != NVAPI_OK)
		return;

	ID3D11Device* pDevice = nullptr;
	m_pDeviceContext->GetDevice(&pDevice);

	NvAPI_D3D_RegisterDevice(pDevice);

	#include "Common/NvidiaFastGeometryShaderTest.h"

	NvAPI_D3D11_CREATE_FASTGS_EXPLICIT_DESC FastGSArgs = {};
	FastGSArgs.version = NVAPI_D3D11_CREATEFASTGSEXPLICIT_VER;
	FastGSArgs.flags = NV_FASTGS_USE_VIEWPORT_MASK;

	ID3D11GeometryShader* pShader = nullptr;
	if (NvAPI_D3D11_CreateFastGeometryShaderExplicit(pDevice, g_main, sizeof(g_main), nullptr, &FastGSArgs, &pShader) == NVAPI_OK && pShader != nullptr)
	{
		bMultiResSupported = true;
		pShader->Release();

		NV_QUERY_MODIFIED_W_SUPPORT_PARAMS ModifiedWParams = {};
		ModifiedWParams.version = NV_QUERY_MODIFIED_W_SUPPORT_PARAMS_VER;
		if (NvAPI_D3D_QueryModifiedWSupport(pDevice, &ModifiedWParams) == NVAPI_OK)
		{
			bLensMatchedSupported = ModifiedWParams.bModifiedWSupported != 0;
		}
	}

	pDevice->Release();

#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::ClearState()
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(ClearState);
	m_pDeviceContext->ClearState();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(ClearState);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::Flush()
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(Flush);
	m_pDeviceContext->Flush();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(Flush);
}

///////////////////////////////////////////////////////////////////////////////
inline D3D11_DEVICE_CONTEXT_TYPE CCryDeviceContextWrapper::GetType()
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	return D3D11_DEVICE_CONTEXT_TYPE();
	#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GetType);
	D3D11_DEVICE_CONTEXT_TYPE nResult = m_pDeviceContext->GetType();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GetType, nResult);
	return nResult;
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline UINT CCryDeviceContextWrapper::GetContextFlags()
{
	#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
	return UINT();
	#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GetContextFlags);
	UINT nResult = m_pDeviceContext->GetContextFlags();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GetContextFlags, nResult);
	return nResult;
	#endif
}

#if (CRY_RENDERER_DIRECT3D >= 111)
///////////////////////////////////////////////////////////////////////////////
// below are functions which are only avilable on D3D11.1 and later
///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CopySubresourceRegion1(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT CopyFlags)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CopySubresourceRegion1, pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, CopyFlags);
	m_pDeviceContext->CopySubresourceRegion1(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, CopyFlags);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CopySubresourceRegion1, pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, CopyFlags);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CopySubresourcesRegion1(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT CopyFlags, UINT NumSubresources)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();

#if (CRY_RENDERER_DIRECT3D >= 120)
	{
		_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CopySubresourcesRegion1, pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, CopyFlags, NumSubresources);
		m_pDeviceContext->CopySubresourcesRegion1(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, CopyFlags, NumSubresources);
		_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CopySubresourcesRegion1, pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, CopyFlags, NumSubresources);
	}
#else
	if (NumSubresources <= 1)
	{
		_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CopySubresourceRegion1, pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, CopyFlags);
		m_pDeviceContext->CopySubresourceRegion1(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, CopyFlags);
		_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CopySubresourceRegion1, pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, CopyFlags);
	}
	else
	{
		D3D11_RESOURCE_DIMENSION srcDim;  pSrcResource->GetType(&srcDim);
		D3D11_RESOURCE_DIMENSION dstDim;  pDstResource->GetType(&dstDim);
		UINT srcMipLevels = 0;
		UINT dstMipLevels = 0;
		if (srcDim == D3D11_RESOURCE_DIMENSION_TEXTURE1D) { D3D11_TEXTURE1D_DESC srcDesc; ((ID3D11Texture1D*)pSrcResource)->GetDesc(&srcDesc); srcMipLevels = srcDesc.MipLevels; }
		if (srcDim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) { D3D11_TEXTURE2D_DESC srcDesc; ((ID3D11Texture2D*)pSrcResource)->GetDesc(&srcDesc); srcMipLevels = srcDesc.MipLevels; }
		if (srcDim == D3D11_RESOURCE_DIMENSION_TEXTURE3D) { D3D11_TEXTURE3D_DESC srcDesc; ((ID3D11Texture3D*)pSrcResource)->GetDesc(&srcDesc); srcMipLevels = srcDesc.MipLevels; }
		if (dstDim == D3D11_RESOURCE_DIMENSION_TEXTURE1D) { D3D11_TEXTURE1D_DESC dstDesc; ((ID3D11Texture1D*)pDstResource)->GetDesc(&dstDesc); dstMipLevels = dstDesc.MipLevels; }
		if (dstDim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) { D3D11_TEXTURE2D_DESC dstDesc; ((ID3D11Texture2D*)pDstResource)->GetDesc(&dstDesc); dstMipLevels = dstDesc.MipLevels; }
		if (dstDim == D3D11_RESOURCE_DIMENSION_TEXTURE3D) { D3D11_TEXTURE3D_DESC dstDesc; ((ID3D11Texture3D*)pDstResource)->GetDesc(&dstDesc); dstMipLevels = dstDesc.MipLevels; }

		D3D11_BOX srcBoxBck = { 0 };
		D3D11_BOX srcRegion = { 0 };
		D3D11_BOX dstRegion = { DstX, DstY, DstZ };
		if (pSrcBox)
		{
			srcBoxBck = *pSrcBox;
			srcRegion = *pSrcBox;
			pSrcBox = &srcRegion;
		}

		// NOTE: too complex case which is not supported as it leads to fe. [slice,mip] sequences like [0,4],[0,5],[0,6],[1,0],[1,1],...
		// which we don't support because the offsets and dimensions are relative to a intermediate mip-level, while crossing the
		// slice-boundary forces us to extrapolate dimensions to larger mips, which is probably not what is wanted in the first place.
		CRY_ASSERT(!srcMipLevels || !((SrcSubresource) % (srcMipLevels)) || (SrcSubresource + NumSubresources <= srcMipLevels));
		CRY_ASSERT(!dstMipLevels || !((DstSubresource) % (dstMipLevels)) || (DstSubresource + NumSubresources <= dstMipLevels));

		for (UINT n = 0; n < NumSubresources; ++n)
		{
			const UINT srcSlice = (SrcSubresource + n) / (srcMipLevels);
			const UINT dstSlice = (DstSubresource + n) / (dstMipLevels);
			const UINT srcLevel = (SrcSubresource + n) % (srcMipLevels);
			const UINT dstLevel = (DstSubresource + n) % (dstMipLevels);

			// reset dimensions/coordinates when crossing slice-boundary
			if (!srcLevel)
				srcRegion = srcBoxBck;
			if (!dstLevel)
				dstRegion = { DstX, DstY, DstZ };

			_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CopySubresourceRegion1, pDstResource, DstSubresource + n, dstRegion.left, dstRegion.top, dstRegion.front, pSrcResource, SrcSubresource + n, pSrcBox, CopyFlags);
			m_pDeviceContext->CopySubresourceRegion1(pDstResource, DstSubresource + n, dstRegion.left, dstRegion.top, dstRegion.front, pSrcResource, SrcSubresource + n, pSrcBox, CopyFlags);
			_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CopySubresourceRegion1, pDstResource, DstSubresource + n, dstRegion.left, dstRegion.top, dstRegion.front, pSrcResource, SrcSubresource + n, pSrcBox, CopyFlags);

			srcRegion.left   >>= 1;
			srcRegion.top    >>= 1;
			srcRegion.front  >>= 1;
			
			srcRegion.right  >>= 1; if (srcRegion.right  == srcRegion.left ) srcRegion.right  = srcRegion.left + 1;
			srcRegion.bottom >>= 1; if (srcRegion.bottom == srcRegion.top  ) srcRegion.bottom = srcRegion.top  + 1;
			srcRegion.back   >>= 1; if (srcRegion.back   == srcRegion.front) srcRegion.back   = srcRegion.front+ 1;
				
			dstRegion.left   >>= 1;
			dstRegion.top    >>= 1;
			dstRegion.front  >>= 1;
		}
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::UpdateSubresource1(ID3D11Resource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags)
{
		#if CRY_PLATFORM_ORBIS
	UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch); // Runtime is allowed to discard flags, and DXOrbis does so :)
		#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(UpdateSubresource1, pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch, CopyFlags);
	m_pDeviceContext->UpdateSubresource1(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch, CopyFlags);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(UpdateSubresource1, pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch, CopyFlags);
		#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DiscardResource(ID3D11Resource* pResource)
{
		#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
		#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DiscardResource, pResource);
	m_pDeviceContext->DiscardResource(pResource);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DiscardResource, pResource);
		#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DiscardView(ID3D11View* pResourceView)
{
		#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
		#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DiscardView, pResourceView);
	m_pDeviceContext->DiscardView(pResourceView);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DiscardView, pResourceView);
		#endif
}

	#endif

	#if (CRY_RENDERER_DIRECT3D >= 111)
///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::VSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(VSSetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	ID3D11DeviceContext1* pContext1 = (ID3D11DeviceContext1*)m_pDeviceContext;
	pContext1->VSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(VSSetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::HSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSSetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	ID3D11DeviceContext1* pContext1 = (ID3D11DeviceContext1*)m_pDeviceContext;
	pContext1->HSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSSetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DSSetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	ID3D11DeviceContext1* pContext1 = (ID3D11DeviceContext1*)m_pDeviceContext;
	pContext1->DSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DSSetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::GSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GSSetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	ID3D11DeviceContext1* pContext1 = (ID3D11DeviceContext1*)m_pDeviceContext;
	pContext1->GSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GSSetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::PSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PSSetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	ID3D11DeviceContext1* pContext1 = (ID3D11DeviceContext1*)m_pDeviceContext;
	pContext1->PSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PSSetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)
{
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSSetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	ID3D11DeviceContext1* pContext1 = (ID3D11DeviceContext1*)m_pDeviceContext;
	pContext1->CSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSSetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::VSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)
{
		#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
		#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(VSGetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	m_pDeviceContext->VSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(VSGetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
		#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::HSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)
{
		#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
		#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSGetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	m_pDeviceContext->HSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSGetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
		#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)
{
		#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
		#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DSGetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	m_pDeviceContext->DSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DSGetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
		#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::GSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)
{
		#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
		#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GSGetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	m_pDeviceContext->GSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GSGetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
		#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::PSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)
{
		#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
		#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PSGetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	m_pDeviceContext->PSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PSGetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
		#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)
{
		#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
		#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSGetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	m_pDeviceContext->CSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSGetConstantBuffers1, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
		#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::SwapDeviceContextState(ID3DDeviceContextState* pState, ID3DDeviceContextState** ppPreviousState)
{
		#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
		#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(SwapDeviceContextState, pState, ppPreviousState);
	m_pDeviceContext->SwapDeviceContextState(pState, ppPreviousState);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(SwapDeviceContextState, pState, ppPreviousState);
		#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::ClearView(ID3D11View* pView, const FLOAT Color[4], UINT NumRects, const D3D11_RECT* pRect)
{
		#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
		#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(ClearView, pView, Color, pRect, NumRects);
	m_pDeviceContext->ClearView(pView, Color, pRect, NumRects);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(ClearView, pView, Color, pRect, NumRects);
		#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::DiscardView1(ID3D11View* pResourceView, UINT NumRects, const D3D11_RECT* pRects)
{
		#if CRY_PLATFORM_ORBIS
	CryFatalError("CreateClassLinkage not implemented right now on Orbis");
		#else
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DiscardView1, pResourceView, pRects, NumRects);
	m_pDeviceContext->DiscardView1(pResourceView, pRects, NumRects);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DiscardView1, pResourceView, pRects, NumRects);
		#endif
}
///////////////////////////////////////////////////////////////////////////////
	#endif

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::ClearRectsRenderTargetView(ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D11_RECT* pRects)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(ClearRectsRenderTargetView, pRenderTargetView, ColorRGBA, NumRects, pRects);
	m_pDeviceContext->ClearRectsRenderTargetView(pRenderTargetView, ColorRGBA, NumRects, pRects);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(ClearRectsRenderTargetView, pRenderTargetView, ColorRGBA, NumRects, pRects);
	#else
	ClearRenderTargetView(pRenderTargetView, ColorRGBA);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::ClearRectsUnorderedAccessViewUint(ID3D11UnorderedAccessView* pUnorderedAccessView, const UINT Values[4], UINT NumRects, const D3D11_RECT* pRects)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(ClearRectsUnorderedAccessViewUint, pUnorderedAccessView, Values, NumRects, pRects);
	m_pDeviceContext->ClearRectsUnorderedAccessViewUint(pUnorderedAccessView, Values, NumRects, pRects);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(ClearRectsUnorderedAccessViewUint, pUnorderedAccessView, Values, NumRects, pRects);
	#else
	ClearUnorderedAccessViewUint(pUnorderedAccessView, Values);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::ClearRectsUnorderedAccessViewFloat(ID3D11UnorderedAccessView* pUnorderedAccessView, const FLOAT Values[4], UINT NumRects, const D3D11_RECT* pRects)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(ClearRectsUnorderedAccessViewFloat, pUnorderedAccessView, Values, NumRects, pRects);
	m_pDeviceContext->ClearRectsUnorderedAccessViewFloat(pUnorderedAccessView, Values, NumRects, pRects);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(ClearRectsUnorderedAccessViewFloat, pUnorderedAccessView, Values, NumRects, pRects);
	#else
	ClearUnorderedAccessViewFloat(pUnorderedAccessView, Values);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::ClearRectsDepthStencilView(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D11_RECT* pRects)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(ClearRectsDepthStencilView, pDepthStencilView, ClearFlags, Depth, Stencil, NumRects, pRects);
	m_pDeviceContext->ClearRectsDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil, NumRects, pRects);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(ClearRectsDepthStencilView, pDepthStencilView, ClearFlags, Depth, Stencil, NumRects, pRects);
	#else
	ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryDeviceContextWrapper::CopyResourceOvercross(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CopyResourceOvercross, pDstResource, pSrcResource);
	m_pDeviceContext->CopyResourceOvercross(pDstResource, pSrcResource);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CopyResourceOvercross, pDstResource, pSrcResource);
	#else
	#endif
}

inline void CCryDeviceContextWrapper::JoinSubresourceRegion(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResourceL, ID3D11Resource* pSrcResourceR, UINT SrcSubresource, const D3D11_BOX* pSrcBox)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(JoinSubresourceRegion, pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResourceL, pSrcResourceR, SrcSubresource, pSrcBox);
	m_pDeviceContext->JoinSubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResourceL, pSrcResourceR, SrcSubresource, pSrcBox);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(JoinSubresourceRegion, pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResourceL, pSrcResourceR, SrcSubresource, pSrcBox);
	#else
	// TODO: make this type-safe!
	D3D11_TEXTURE2D_DESC desc;
	((ID3D11Texture2D*)pDstResource)->GetDesc(&desc);

	m_pDeviceContext->CopySubresourceRegion(
	  pDstResource,
	  0,
	  0,
	  0,
	  0,
	  pSrcResourceL,
	  0,
	  nullptr);

	m_pDeviceContext->CopySubresourceRegion(
	  pDstResource,
	  0,
	  desc.Width >> 1,
	  0,
	  0,
	  pSrcResourceR,
	  0,
	  nullptr);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
	#if (CRY_RENDERER_DIRECT3D >= 111)
inline void CCryDeviceContextWrapper::CopyResourceOvercross1(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource, UINT CopyFlags)
{
		#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CopyResourceOvercross1, pDstResource, pSrcResource, CopyFlags);
	m_pDeviceContext->CopyResourceOvercross1(pDstResource, pSrcResource, CopyFlags);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CopyResourceOvercross1, pDstResource, pSrcResource, CopyFlags);
		#else
		#endif
}

inline void CCryDeviceContextWrapper::JoinSubresourceRegion1(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResourceL, ID3D11Resource* pSrcResourceR, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT CopyFlags)
{
		#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(JoinSubresourceRegion1, pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResourceL, pSrcResourceR, SrcSubresource, pSrcBox, CopyFlags);
	m_pDeviceContext->JoinSubresourceRegion1(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResourceL, pSrcResourceR, SrcSubresource, pSrcBox, CopyFlags);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(JoinSubresourceRegion1, pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResourceL, pSrcResourceR, SrcSubresource, pSrcBox, CopyFlags);
		#else
	// TODO: make this type-safe!
	D3D11_TEXTURE2D_DESC desc;
	((ID3D11Texture2D*)pDstResource)->GetDesc(&desc);

	m_pDeviceContext->CopySubresourceRegion1(
	  pDstResource,
	  0,
	  0,
	  0,
	  0,
	  pSrcResourceL,
	  0,
	  nullptr,
	  CopyFlags);

	m_pDeviceContext->CopySubresourceRegion1(
	  pDstResource,
	  0,
	  desc.Width >> 1,
	  0,
	  0,
	  pSrcResourceR,
	  0,
	  nullptr,
	  CopyFlags);
		#endif
}
	#endif

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceContextWrapper::CopyStagingResource(ID3D11Resource* pStagingResource, ID3D11Resource* pSourceResource, UINT SubResource, BOOL Upload)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CopyStagingResource, pStagingResource, pSourceResource, SubResource, Upload);
	HRESULT result = m_pDeviceContext->CopyStagingResource(pStagingResource, pSourceResource, SubResource, Upload);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CopyStagingResource, result, pStagingResource, pSourceResource, SubResource, Upload);
	#else
	HRESULT result = E_FAIL;
	CryFatalError("CopyStagingResource not implemented right now any other than DX12");
	#endif

	return result;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceContextWrapper::TestStagingResource(ID3D11Resource* pStagingResource)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(TestStagingResource, pStagingResource);
	HRESULT result = m_pDeviceContext->TestStagingResource(pStagingResource);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(TestStagingResource, result, pStagingResource);
	#else
	HRESULT result = E_FAIL;
	CryFatalError("TestStagingResource not implemented right now any other than DX12");
	#endif

	return result;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceContextWrapper::WaitStagingResource(ID3D11Resource* pStagingResource)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(WaitStagingResource, pStagingResource);
	HRESULT result = m_pDeviceContext->WaitStagingResource(pStagingResource);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(WaitStagingResource, result, pStagingResource);
	#else
	HRESULT result = E_FAIL;
	CryFatalError("WaitStagingResource not implemented right now any other than DX12");
	#endif

	return result;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceContextWrapper::MapStagingResource(ID3D11Resource* pStagingResource, BOOL Upload, void** ppStagingMemory)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(MapStagingResource, pStagingResource, Upload, ppStagingMemory);
	HRESULT result = m_pDeviceContext->MapStagingResource(pStagingResource, Upload, ppStagingMemory);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(MapStagingResource, result, pStagingResource, Upload, ppStagingMemory);
	#else
	HRESULT result = E_FAIL;
	CryFatalError("MapStagingResource not implemented right now any other than DX12");
	#endif

	return result;
}

inline void CCryDeviceContextWrapper::UnmapStagingResource(ID3D11Resource* pStagingResource, BOOL Upload)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(UnmapStagingResource, pStagingResource, Upload);
	m_pDeviceContext->UnmapStagingResource(pStagingResource, Upload);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(UnmapStagingResource, pStagingResource, Upload);
	#else
	CryFatalError("UnmapStagingResource not implemented right now any other than DX12");
	#endif
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryDeviceContextWrapper::MappedWriteToSubresource(ID3D11Resource* pResource, UINT Subresource, SIZE_T Offset, SIZE_T Size, D3D11_MAP MapType, const void* pData, UINT numDataBlocks)
{
#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(MappedWriteToSubresource, pResource, Subresource, Offset, Size, MapType, pData, numDataBlocks);
	HRESULT result = m_pDeviceContext->MappedWriteToSubresource(pResource, Subresource, Offset, Size, MapType, pData, numDataBlocks);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(MappedWriteToSubresource, result, pResource, Subresource, Offset, Size, MapType, pData, numDataBlocks);
#else
	HRESULT result = E_FAIL;
	CryFatalError("MappedWriteToSubresource not implemented right now any other than DX12");
#endif

	return result;
}

inline HRESULT CCryDeviceContextWrapper::MappedReadFromSubresource(ID3D11Resource* pResource, UINT Subresource, SIZE_T Offset, SIZE_T Size, D3D11_MAP MapType, void* pData, UINT numDataBlocks)
{
#if (CRY_RENDERER_DIRECT3D >= 120)
	assert(m_pDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(MappedReadFromSubresource, pResource, Subresource, Offset, Size, MapType, pData, numDataBlocks);
	HRESULT result = m_pDeviceContext->MappedReadFromSubresource(pResource, Subresource, Offset, Size, MapType, pData, numDataBlocks);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(MappedReadFromSubresource, result, pResource, Subresource, Offset, Size, MapType, pData, numDataBlocks);
#else
	HRESULT result = E_FAIL;
	CryFatalError("MappedReadFromSubresource not implemented right now any other than DX12");
#endif

	return result;
}

	#if defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE)
///////////////////////////////////////////////////////////////////////////////
// below are functions which are only avilable on a D3D11 Performance Device
// and Context, which are not supported on all platforms
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// CryDeviceContextWrapper methods
///////////////////////////////////////////////////////////////////////////////
inline CCryPerformanceDeviceWrapper::CCryPerformanceDeviceWrapper()
	: m_pPerformanceDevice(NULL)
	, m_pDeviceHooks(NULL)
{
}

///////////////////////////////////////////////////////////////////////////////
inline bool CCryPerformanceDeviceWrapper::IsValid() const
{
	return m_pPerformanceDevice != NULL;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceWrapper::AssignPerformanceDevice(ID3DXboxPerformanceDevice* pPerformanceDevice)
{
	if (m_pPerformanceDevice != NULL && m_pPerformanceDevice != pPerformanceDevice)
		CryFatalError("Trying to assign two difference performance devices");

	m_pPerformanceDevice = pPerformanceDevice;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceWrapper::ReleasePerformanceDevice()
{
	if (m_pPerformanceDevice != NULL)
	{
		m_pPerformanceDevice->Release();
		m_pPerformanceDevice = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////
inline ID3DXboxPerformanceDevice* CCryPerformanceDeviceWrapper::GetRealPerformanceDevice() const
{
	return m_pPerformanceDevice;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceWrapper::RegisterHook(ICryDeviceWrapperHook* pDeviceWrapperHook)
{
	pDeviceWrapperHook->SetNext(m_pDeviceHooks);
	m_pDeviceHooks = pDeviceWrapperHook;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceWrapper::UnregisterHook(const char* pDeviceHookName)
{
	ICryDeviceWrapperHook* pDeviceHook = m_pDeviceHooks;

	if (pDeviceHook == NULL)
		return;

	if (strcmp(pDeviceHook->Name(), pDeviceHookName) == 0)
	{
		m_pDeviceHooks = pDeviceHook->GetNext();
		return;
	}

	while (pDeviceHook->GetNext())
	{
		if (strcmp(pDeviceHook->GetNext()->Name(), pDeviceHookName) == 0)
		{
			pDeviceHook->SetNext(pDeviceHook->GetNext()->GetNext());
			return;
		}
		pDeviceHook = pDeviceHook->GetNext();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryPerformanceDeviceWrapper::CreateCounterSet(const D3D11X_COUNTER_SET_DESC* pCounterSetDesc, ID3DXboxCounterSet** ppCounterSet)
{
	assert(m_pPerformanceDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateCounterSet, pCounterSetDesc, ppCounterSet);
	HRESULT hr = m_pPerformanceDevice->CreateCounterSet(pCounterSetDesc, ppCounterSet);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateCounterSet, hr, pCounterSetDesc, ppCounterSet);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryPerformanceDeviceWrapper::CreateCounterSample(ID3DXboxCounterSample** ppCounterSample)
{
	assert(m_pPerformanceDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateCounterSample, ppCounterSample);
	HRESULT hr = m_pPerformanceDevice->CreateCounterSample(ppCounterSample);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateCounterSample, hr, ppCounterSample);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryPerformanceDeviceWrapper::SetDriverHint(UINT Feature, UINT Value)
{
	assert(m_pPerformanceDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(SetDriverHint, Feature, Value);
	HRESULT hr = m_pPerformanceDevice->SetDriverHint(Feature, Value);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(SetDriverHint, hr, Feature, Value);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryPerformanceDeviceWrapper::CreateDmaEngineContext(const D3D11_DMA_ENGINE_CONTEXT_DESC* pDmaEngineContextDesc, ID3D11DmaEngineContextX** ppDmaDeviceContext)
{
	assert(m_pPerformanceDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreateDmaEngineContext, pDmaEngineContextDesc, ppDmaDeviceContext);
	HRESULT hr = m_pPerformanceDevice->CreateDmaEngineContext(pDmaEngineContextDesc, ppDmaDeviceContext);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreateDmaEngineContext, hr, pDmaEngineContextDesc, ppDmaDeviceContext);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline BOOL CCryPerformanceDeviceWrapper::IsFencePending(UINT64 Fence)
{
	assert(m_pPerformanceDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(IsFencePending, Fence);
	HRESULT hr = m_pPerformanceDevice->IsFencePending(Fence);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(IsFencePending, hr, Fence);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline BOOL CCryPerformanceDeviceWrapper::IsResourcePending(ID3D11Resource* pResource)
{
	assert(m_pPerformanceDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(IsResourcePending, pResource);
	BOOL bResult = m_pPerformanceDevice->IsResourcePending(pResource);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(IsResourcePending, bResult, pResource);
	return bResult;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryPerformanceDeviceWrapper::CreatePlacementBuffer(const D3D11_BUFFER_DESC* pDesc, void* pCpuVirtualAddress, ID3D11Buffer** ppBuffer)
{
	assert(m_pPerformanceDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreatePlacementBuffer, pDesc, pCpuVirtualAddress, ppBuffer);
	HRESULT hr = m_pPerformanceDevice->CreatePlacementBuffer(pDesc, pCpuVirtualAddress, ppBuffer);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreatePlacementBuffer, hr, pDesc, pCpuVirtualAddress, ppBuffer);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryPerformanceDeviceWrapper::CreatePlacementTexture1D(const D3D11_TEXTURE1D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture1D** ppTexture1D)
{
	assert(m_pPerformanceDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreatePlacementTexture1D, pDesc, TileModeIndex, Pitch, pCpuVirtualAddress, ppTexture1D);
	HRESULT hr = m_pPerformanceDevice->CreatePlacementTexture1D(pDesc, TileModeIndex, Pitch, pCpuVirtualAddress, ppTexture1D);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreatePlacementTexture1D, hr, pDesc, TileModeIndex, Pitch, pCpuVirtualAddress, ppTexture1D);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryPerformanceDeviceWrapper::CreatePlacementTexture2D(const D3D11_TEXTURE2D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture2D** ppTexture2D)
{
	assert(m_pPerformanceDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreatePlacementTexture2D, pDesc, TileModeIndex, Pitch, pCpuVirtualAddress, ppTexture2D);
	HRESULT hr = m_pPerformanceDevice->CreatePlacementTexture2D(pDesc, TileModeIndex, Pitch, pCpuVirtualAddress, ppTexture2D);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreatePlacementTexture2D, hr, pDesc, TileModeIndex, Pitch, pCpuVirtualAddress, ppTexture2D);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryPerformanceDeviceWrapper::CreatePlacementTexture3D(const D3D11_TEXTURE3D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture3D** ppTexture3D)
{
	assert(m_pPerformanceDevice != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CreatePlacementTexture3D, pDesc, TileModeIndex, Pitch, pCpuVirtualAddress, ppTexture3D);
	HRESULT hr = m_pPerformanceDevice->CreatePlacementTexture3D(pDesc, TileModeIndex, Pitch, pCpuVirtualAddress, ppTexture3D);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CreatePlacementTexture3D, hr, pDesc, TileModeIndex, Pitch, pCpuVirtualAddress, ppTexture3D);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// ID3DPerformanceDeviceContext Wrapper functions
///////////////////////////////////////////////////////////////////////////////
inline CCryPerformanceDeviceContextWrapper::CCryPerformanceDeviceContextWrapper()
	: m_pPerformanceDeviceContext(NULL)
	, m_pDeviceHooks(NULL)
{
}

///////////////////////////////////////////////////////////////////////////////
inline bool CCryPerformanceDeviceContextWrapper::IsValid() const
{
	return m_pPerformanceDeviceContext != NULL;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::AssignPerformanceDeviceContext(ID3DXboxPerformanceContext* pPerformanceDeviceContext)
{
	if (m_pPerformanceDeviceContext != NULL && m_pPerformanceDeviceContext != pPerformanceDeviceContext)
		CryFatalError("Trying to assign two difference performance device context");

	m_pPerformanceDeviceContext = pPerformanceDeviceContext;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::ReleasePerformanceDeviceContext()
{
	if (m_pPerformanceDeviceContext != NULL)
	{
		m_pPerformanceDeviceContext->Release();
		m_pPerformanceDeviceContext = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////
inline ID3DXboxPerformanceContext* CCryPerformanceDeviceContextWrapper::GetRealPerformanceDeviceContext() const
{
	return m_pPerformanceDeviceContext;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::RegisterHook(ICryDeviceWrapperHook* pDeviceWrapperHook)
{
	pDeviceWrapperHook->SetNext(m_pDeviceHooks);
	m_pDeviceHooks = pDeviceWrapperHook;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::UnregisterHook(const char* pDeviceHookName)
{
	ICryDeviceWrapperHook* pDeviceHook = m_pDeviceHooks;

	if (pDeviceHook == NULL)
		return;

	if (strcmp(pDeviceHook->Name(), pDeviceHookName) == 0)
	{
		m_pDeviceHooks = pDeviceHook->GetNext();
		return;
	}

	while (pDeviceHook->GetNext())
	{
		if (strcmp(pDeviceHook->GetNext()->Name(), pDeviceHookName) == 0)
		{
			pDeviceHook->SetNext(pDeviceHook->GetNext()->GetNext());
			return;
		}
		pDeviceHook = pDeviceHook->GetNext();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::FlushGpuCaches(ID3D11Resource* pResource)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(FlushGpuCaches, pResource);
	m_pPerformanceDeviceContext->FlushGpuCaches(pResource);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(FlushGpuCaches, pResource);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::FlushGpuCacheRange(UINT Flags, void* pBaseAddress, SIZE_T SizeInBytes)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(FlushGpuCacheRange, Flags, pBaseAddress, SizeInBytes);
	m_pPerformanceDeviceContext->FlushGpuCacheRange(Flags, pBaseAddress, SizeInBytes);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(FlushGpuCacheRange, Flags, pBaseAddress, SizeInBytes);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::InsertWaitUntilIdle(UINT Flags)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(InsertWaitUntilIdle, Flags);
	m_pPerformanceDeviceContext->InsertWaitUntilIdle(Flags);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(InsertWaitUntilIdle, Flags);
}

///////////////////////////////////////////////////////////////////////////////
inline UINT64 CCryPerformanceDeviceContextWrapper::InsertFence()
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(InsertFence, 0);
	UINT64 nResult = m_pPerformanceDeviceContext->InsertFence();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(InsertFence, nResult);
	return nResult;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::RemapConstantBufferInheritance(D3D11_STAGE Stage, UINT Slot, D3D11_STAGE InheritStage, UINT InheritSlot)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(RemapConstantBufferInheritance, Stage, Slot, InheritStage, InheritSlot);
	m_pPerformanceDeviceContext->RemapConstantBufferInheritance(Stage, Slot, InheritStage, InheritSlot);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(RemapConstantBufferInheritance, Stage, Slot, InheritStage, InheritSlot);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::RemapShaderResourceInheritance(D3D11_STAGE Stage, UINT Slot, D3D11_STAGE InheritStage, UINT InheritSlot)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(RemapShaderResourceInheritance, Stage, Slot, InheritStage, InheritSlot);
	m_pPerformanceDeviceContext->RemapShaderResourceInheritance(Stage, Slot, InheritStage, InheritSlot);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(RemapShaderResourceInheritance, Stage, Slot, InheritStage, InheritSlot);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::RemapSamplerInheritance(D3D11_STAGE Stage, UINT Slot, D3D11_STAGE InheritStage, UINT InheritSlot)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(RemapSamplerInheritance, Stage, Slot, InheritStage, InheritSlot);
	m_pPerformanceDeviceContext->RemapSamplerInheritance(Stage, Slot, InheritStage, InheritSlot);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(RemapSamplerInheritance, Stage, Slot, InheritStage, InheritSlot);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::RemapVertexBufferInheritance(UINT Slot, UINT InheritSlot)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(RemapVertexBufferInheritance, Slot, InheritSlot);
	m_pPerformanceDeviceContext->RemapVertexBufferInheritance(Slot, InheritSlot);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(RemapVertexBufferInheritance, Slot, InheritSlot);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::PSSetFastConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PSSetFastConstantBuffer, Slot, pConstantBuffer);
	m_pPerformanceDeviceContext->PSSetFastConstantBuffer(Slot, pConstantBuffer);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PSSetFastConstantBuffer, Slot, pConstantBuffer);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::PSSetFastShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PSSetFastShaderResource, Slot, pShaderResourceView);
	m_pPerformanceDeviceContext->PSSetFastShaderResource(Slot, pShaderResourceView);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PSSetFastShaderResource, Slot, pShaderResourceView);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::PSSetFastSampler(UINT Slot, ID3D11SamplerState* pSampler)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PSSetFastSampler, Slot, pSampler);
	m_pPerformanceDeviceContext->PSSetFastSampler(Slot, pSampler);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PSSetFastSampler, Slot, pSampler);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::VSSetFastConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(VSSetFastConstantBuffer, Slot, pConstantBuffer);
	m_pPerformanceDeviceContext->VSSetFastConstantBuffer(Slot, pConstantBuffer);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(VSSetFastConstantBuffer, Slot, pConstantBuffer);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::VSSetFastShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(VSSetFastShaderResource, Slot, pShaderResourceView);
	m_pPerformanceDeviceContext->VSSetFastShaderResource(Slot, pShaderResourceView);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(VSSetFastShaderResource, Slot, pShaderResourceView);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::VSSetFastSampler(UINT Slot, ID3D11SamplerState* pSampler)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(VSSetFastSampler, Slot, pSampler);
	m_pPerformanceDeviceContext->VSSetFastSampler(Slot, pSampler);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(VSSetFastSampler, Slot, pSampler);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::GSSetFastConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GSSetFastConstantBuffer, Slot, pConstantBuffer);
	m_pPerformanceDeviceContext->GSSetFastConstantBuffer(Slot, pConstantBuffer);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GSSetFastConstantBuffer, Slot, pConstantBuffer);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::GSSetFastShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GSSetFastShaderResource, Slot, pShaderResourceView);
	m_pPerformanceDeviceContext->GSSetFastShaderResource(Slot, pShaderResourceView);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GSSetFastShaderResource, Slot, pShaderResourceView);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::GSSetFastSampler(UINT Slot, ID3D11SamplerState* pSampler)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GSSetFastSampler, Slot, pSampler);
	m_pPerformanceDeviceContext->GSSetFastSampler(Slot, pSampler);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GSSetFastSampler, Slot, pSampler);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::CSSetFastConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSSetFastConstantBuffer, Slot, pConstantBuffer);
	m_pPerformanceDeviceContext->CSSetFastConstantBuffer(Slot, pConstantBuffer);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSSetFastConstantBuffer, Slot, pConstantBuffer);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::CSSetFastShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSSetFastShaderResource, Slot, pShaderResourceView);
	m_pPerformanceDeviceContext->CSSetFastShaderResource(Slot, pShaderResourceView);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSSetFastShaderResource, Slot, pShaderResourceView);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::CSSetFastSampler(UINT Slot, ID3D11SamplerState* pSampler)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSSetFastSampler, Slot, pSampler);
	m_pPerformanceDeviceContext->CSSetFastSampler(Slot, pSampler);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSSetFastSampler, Slot, pSampler);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::HSSetFastConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSSetFastConstantBuffer, Slot, pConstantBuffer);
	m_pPerformanceDeviceContext->HSSetFastConstantBuffer(Slot, pConstantBuffer);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSSetFastConstantBuffer, Slot, pConstantBuffer);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::HSSetFastShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSSetFastShaderResource, Slot, pShaderResourceView);
	m_pPerformanceDeviceContext->HSSetFastShaderResource(Slot, pShaderResourceView);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSSetFastShaderResource, Slot, pShaderResourceView);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::HSSetFastSampler(UINT Slot, ID3D11SamplerState* pSampler)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSSetFastSampler, Slot, pSampler);
	m_pPerformanceDeviceContext->HSSetFastSampler(Slot, pSampler);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSSetFastSampler, Slot, pSampler);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::DSSetFastConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DSSetFastConstantBuffer, Slot, pConstantBuffer);
	m_pPerformanceDeviceContext->DSSetFastConstantBuffer(Slot, pConstantBuffer);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DSSetFastConstantBuffer, Slot, pConstantBuffer);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::DSSetFastShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DSSetFastShaderResource, Slot, pShaderResourceView);
	m_pPerformanceDeviceContext->DSSetFastShaderResource(Slot, pShaderResourceView);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DSSetFastShaderResource, Slot, pShaderResourceView);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::DSSetFastSampler(UINT Slot, ID3D11SamplerState* pSampler)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DSSetFastSampler, Slot, pSampler);
	m_pPerformanceDeviceContext->DSSetFastSampler(Slot, pSampler);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DSSetFastSampler, Slot, pSampler);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::IASetFastVertexBuffer(UINT Slot, ID3D11Buffer* pVertexBuffer, UINT Stride)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(IASetFastVertexBuffer, Slot, pVertexBuffer, Stride);
	m_pPerformanceDeviceContext->IASetFastVertexBuffer(Slot, pVertexBuffer, Stride);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(IASetFastVertexBuffer, Slot, pVertexBuffer, Stride);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::IASetFastIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(IASetFastIndexBuffer, pIndexBuffer, Format);
	m_pPerformanceDeviceContext->IASetFastIndexBuffer(pIndexBuffer, Format);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(IASetFastIndexBuffer, pIndexBuffer, Format);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::PSSetPlacementConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PSSetPlacementConstantBuffer, Slot, pConstantBuffer, pBaseAddress);
	m_pPerformanceDeviceContext->PSSetPlacementConstantBuffer(Slot, pConstantBuffer, pBaseAddress);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PSSetPlacementConstantBuffer, Slot, pConstantBuffer, pBaseAddress);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::PSSetPlacementShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PSSetPlacementShaderResource, Slot, pShaderResourceView, pBaseAddress);
	m_pPerformanceDeviceContext->PSSetPlacementShaderResource(Slot, pShaderResourceView, pBaseAddress);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PSSetPlacementShaderResource, Slot, pShaderResourceView, pBaseAddress);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::VSSetPlacementConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(VSSetPlacementConstantBuffer, Slot, pConstantBuffer, pBaseAddress);
	m_pPerformanceDeviceContext->VSSetPlacementConstantBuffer(Slot, pConstantBuffer, pBaseAddress);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(VSSetPlacementConstantBuffer, Slot, pConstantBuffer, pBaseAddress);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::VSSetPlacementShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(VSSetPlacementShaderResource, Slot, pShaderResourceView, pBaseAddress);
	m_pPerformanceDeviceContext->VSSetPlacementShaderResource(Slot, pShaderResourceView, pBaseAddress);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(VSSetPlacementShaderResource, Slot, pShaderResourceView, pBaseAddress);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::GSSetPlacementConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GSSetPlacementConstantBuffer, Slot, pConstantBuffer, pBaseAddress);
	m_pPerformanceDeviceContext->GSSetPlacementConstantBuffer(Slot, pConstantBuffer, pBaseAddress);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GSSetPlacementConstantBuffer, Slot, pConstantBuffer, pBaseAddress);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::GSSetPlacementShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GSSetPlacementShaderResource, Slot, pShaderResourceView, pBaseAddress);
	m_pPerformanceDeviceContext->GSSetPlacementShaderResource(Slot, pShaderResourceView, pBaseAddress);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GSSetPlacementShaderResource, Slot, pShaderResourceView, pBaseAddress);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::CSSetPlacementConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSSetPlacementConstantBuffer, Slot, pConstantBuffer, pBaseAddress);
	m_pPerformanceDeviceContext->CSSetPlacementConstantBuffer(Slot, pConstantBuffer, pBaseAddress);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSSetPlacementConstantBuffer, Slot, pConstantBuffer, pBaseAddress);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::CSSetPlacementShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(CSSetPlacementShaderResource, Slot, pShaderResourceView, pBaseAddress);
	m_pPerformanceDeviceContext->CSSetPlacementShaderResource(Slot, pShaderResourceView, pBaseAddress);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(CSSetPlacementShaderResource, Slot, pShaderResourceView, pBaseAddress);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::HSSetPlacementConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSSetPlacementConstantBuffer, Slot, pConstantBuffer, pBaseAddress);
	m_pPerformanceDeviceContext->HSSetPlacementConstantBuffer(Slot, pConstantBuffer, pBaseAddress);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSSetPlacementConstantBuffer, Slot, pConstantBuffer, pBaseAddress);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::HSSetPlacementShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSSetPlacementShaderResource, Slot, pShaderResourceView, pBaseAddress);
	m_pPerformanceDeviceContext->HSSetPlacementShaderResource(Slot, pShaderResourceView, pBaseAddress);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSSetPlacementShaderResource, Slot, pShaderResourceView, pBaseAddress);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::DSSetPlacementConstantBuffer(UINT Slot, ID3D11Buffer* pConstantBuffer, void* pBaseAddress)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DSSetPlacementConstantBuffer, Slot, pConstantBuffer, pBaseAddress);
	m_pPerformanceDeviceContext->DSSetPlacementConstantBuffer(Slot, pConstantBuffer, pBaseAddress);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DSSetPlacementConstantBuffer, Slot, pConstantBuffer, pBaseAddress);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::DSSetPlacementShaderResource(UINT Slot, ID3D11ShaderResourceView* pShaderResourceView, void* pBaseAddress)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(DSSetPlacementShaderResource, Slot, pShaderResourceView, pBaseAddress);
	m_pPerformanceDeviceContext->DSSetPlacementShaderResource(Slot, pShaderResourceView, pBaseAddress);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(DSSetPlacementShaderResource, Slot, pShaderResourceView, pBaseAddress);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::IASetPlacementVertexBuffer(UINT Slot, ID3D11Buffer* pVertexBuffer, void* pBaseAddress, UINT Stride)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(IASetPlacementVertexBuffer, Slot, pVertexBuffer, pBaseAddress, Stride);
	m_pPerformanceDeviceContext->IASetPlacementVertexBuffer(Slot, pVertexBuffer, pBaseAddress, Stride);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(IASetPlacementVertexBuffer, Slot, pVertexBuffer, pBaseAddress, Stride);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::IASetPlacementIndexBuffer(ID3D11Buffer* pIndexBuffer, void* pBaseAddress, DXGI_FORMAT Format)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(IASetPlacementIndexBuffer, pIndexBuffer, pBaseAddress, Format);
	m_pPerformanceDeviceContext->IASetPlacementIndexBuffer(pIndexBuffer, pBaseAddress, Format);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(IASetPlacementIndexBuffer, pIndexBuffer, pBaseAddress, Format);
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryPerformanceDeviceContextWrapper::PIXGpuCaptureNextFrame(UINT Flags, LPCWSTR lpOutputFileName)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PIXGpuCaptureNextFrame, Flags, lpOutputFileName);
	HRESULT hr = m_pPerformanceDeviceContext->PIXGpuCaptureNextFrame(Flags, lpOutputFileName);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PIXGpuCaptureNextFrame, hr, Flags, lpOutputFileName);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryPerformanceDeviceContextWrapper::PIXGpuBeginCapture(UINT Flags, LPCWSTR lpOutputFileName)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PIXGpuBeginCapture, Flags, lpOutputFileName);
	HRESULT hr = m_pPerformanceDeviceContext->PIXGpuBeginCapture(Flags, lpOutputFileName);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PIXGpuBeginCapture, hr, Flags, lpOutputFileName);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryPerformanceDeviceContextWrapper::PIXGpuEndCapture()
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PIXGpuEndCapture);
	HRESULT hr = m_pPerformanceDeviceContext->PIXGpuEndCapture();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PIXGpuEndCapture, hr);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// below are functions which are only avilable on the Durango Mono D3D Driver
///////////////////////////////////////////////////////////////////////////////
inline UINT64 CCryPerformanceDeviceContextWrapper::InsertFence(UINT Flags)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(InsertFence, Flags);
	UINT64 nResult = m_pPerformanceDeviceContext->InsertFence(Flags);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(InsertFence, nResult, Flags);
	return nResult;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::InsertWaitOnFence(UINT Flags, UINT64 Fence)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(InsertWaitOnFence, Flags, Fence);
	m_pPerformanceDeviceContext->InsertWaitOnFence(Flags, Fence);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(InsertWaitOnFence, Flags, Fence);
}

///////////////////////////////////////////////////////////////////////////////
inline INT CCryPerformanceDeviceContextWrapper::PIXBeginEvent(LPCWSTR Name)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PIXBeginEvent, Name);
	INT nResult = m_pPerformanceDeviceContext->PIXBeginEvent(Name);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PIXBeginEvent, nResult, Name);
	return nResult;
}

///////////////////////////////////////////////////////////////////////////////
inline INT CCryPerformanceDeviceContextWrapper::PIXEndEvent()
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PIXEndEvent);
	INT nResult = m_pPerformanceDeviceContext->PIXEndEvent();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PIXEndEvent, nResult);
	return nResult;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::PIXSetMarker(LPCWSTR Name)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PIXSetMarker, Name);
	m_pPerformanceDeviceContext->PIXSetMarker(Name);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PIXSetMarker, Name);
}

///////////////////////////////////////////////////////////////////////////////
inline BOOL CCryPerformanceDeviceContextWrapper::PIXGetStatus()
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(PIXGetStatus);
	BOOL nResult = m_pPerformanceDeviceContext->PIXGetStatus();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(PIXGetStatus, nResult);
	return nResult;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::StartCounters(ID3D11CounterSetX* pCounterSet)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(StartCounters, pCounterSet);
	m_pPerformanceDeviceContext->StartCounters(pCounterSet);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(StartCounters, pCounterSet);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::SampleCounters(ID3D11CounterSampleX* pCounterSample)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(SampleCounters, pCounterSample);
	m_pPerformanceDeviceContext->SampleCounters(pCounterSample);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(SampleCounters, pCounterSample);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::StopCounters()
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(StopCounters);
	m_pPerformanceDeviceContext->StopCounters();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(StopCounters);
}

///////////////////////////////////////////////////////////////////////////////
inline HRESULT CCryPerformanceDeviceContextWrapper::GetCounterData(ID3D11CounterSampleX* pCounterSample, D3D11X_COUNTER_DATA* pData, UINT GetCounterDataFlags)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(GetCounterData, pCounterSample, pData, GetCounterDataFlags);
	HRESULT hr = m_pPerformanceDeviceContext->GetCounterData(pCounterSample, pData, GetCounterDataFlags);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(GetCounterData, hr, pCounterSample, pData, GetCounterDataFlags);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::HSSetTessellationParameters(const D3D11X_TESSELLATION_PARAMETERS* pTessellationParameters)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSSetTessellationParameters, pTessellationParameters);
	m_pPerformanceDeviceContext->HSSetTessellationParameters(pTessellationParameters);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSSetTessellationParameters, pTessellationParameters);
}

///////////////////////////////////////////////////////////////////////////////
inline void CCryPerformanceDeviceContextWrapper::HSGetLastUsedTessellationParameters(D3D11X_TESSELLATION_PARAMETERS* pTessellationParameters)
{
	assert(m_pPerformanceDeviceContext != NULL);
	CRY_DEVICE_WRAPPER_PROFILE();
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_(HSGetLastUsedTessellationParameters, pTessellationParameters);
	m_pPerformanceDeviceContext->HSGetLastUsedTessellationParameters(pTessellationParameters);
	_CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_(HSGetLastUsedTessellationParameters, pTessellationParameters);
}
///////////////////////////////////////////////////////////////////////////////
	#endif   // DEVICE_SUPPORTS_PERFORMANCE_DEVICE

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// clear private helper macros
#undef _CRY_DEVICE_WRAPPER_DETAIL_INVOKE_PRE_FUNCTION_HOOK_
#undef _CRY_DEVICE_WRAPPER_DETAIL_INVOKE_POST_FUNCTION_HOOK_

#endif // CRY_DEVICE_WRAPPER_H_
