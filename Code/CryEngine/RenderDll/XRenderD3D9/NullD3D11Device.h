// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if defined(ENABLE_NULL_D3D11DEVICE)

	#if (CRY_RENDERER_DIRECT3D >= 111)
class NullD3D11Device : public ID3D11Device1
	#else
class NullD3D11Device : public ID3D11Device
	#endif
{
public:
	// IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj)
	{
		if (ppvObj)
			*ppvObj = 0;
		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef()
	{
		return CryInterlockedIncrement(&m_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release()
	{
		long refCount = CryInterlockedDecrement(&m_refCount);
		if (refCount <= 0)
			delete this;
		return refCount;
	};

	// ID3D11Device
	virtual HRESULT STDMETHODCALLTYPE CreateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer)
	{
		if (ppBuffer)
			*ppBuffer = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateTexture1D(const D3D11_TEXTURE1D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D)
	{
		if (ppTexture1D)
			*ppTexture1D = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateTexture2D(const D3D11_TEXTURE2D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D)
	{
		if (ppTexture2D)
			*ppTexture2D = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateTexture3D(const D3D11_TEXTURE3D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D)
	{
		if (ppTexture3D)
			*ppTexture3D = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateShaderResourceView(ID3D11Resource* pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc, ID3D11ShaderResourceView** ppSRView)
	{
		if (ppSRView)
			*ppSRView = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(ID3D11Resource* pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc, ID3D11UnorderedAccessView** ppUAView)
	{
		if (ppUAView)
			*ppUAView = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateRenderTargetView(ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc, ID3D11RenderTargetView** ppRTView)
	{
		if (ppRTView)
			*ppRTView = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilView(ID3D11Resource* pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc, ID3D11DepthStencilView** ppDepthStencilView)
	{
		if (ppDepthStencilView)
			*ppDepthStencilView = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements, const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout** ppInputLayout)
	{
		if (ppInputLayout)
			*ppInputLayout = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateVertexShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader)
	{
		if (ppVertexShader)
			*ppVertexShader = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateGeometryShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader)
	{
		if (ppGeometryShader)
			*ppGeometryShader = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(const void* pShaderBytecode, SIZE_T BytecodeLength, const D3D11_SO_DECLARATION_ENTRY* pSODeclaration, UINT NumEntries, const UINT* pBufferStrides, UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader)
	{
		if (ppGeometryShader)
			*ppGeometryShader = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreatePixelShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader)
	{
		if (ppPixelShader)
			*ppPixelShader = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateHullShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11HullShader** ppHullShader)
	{
		if (ppHullShader)
			*ppHullShader = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateDomainShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11DomainShader** ppDomainShader)
	{
		if (ppDomainShader)
			*ppDomainShader = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateComputeShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11ComputeShader** ppComputeShader)
	{
		if (ppComputeShader)
			*ppComputeShader = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateClassLinkage(ID3D11ClassLinkage** ppLinkage)
	{
		if (ppLinkage)
			*ppLinkage = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateBlendState(const D3D11_BLEND_DESC* pBlendStateDesc, ID3D11BlendState** ppBlendState)
	{
		if (ppBlendState)
			*ppBlendState = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc, ID3D11DepthStencilState** ppDepthStencilState)
	{
		if (ppDepthStencilState)
			*ppDepthStencilState = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateRasterizerState(const D3D11_RASTERIZER_DESC* pRasterizerDesc, ID3D11RasterizerState** ppRasterizerState)
	{
		if (ppRasterizerState)
			*ppRasterizerState = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSamplerState(const D3D11_SAMPLER_DESC* pSamplerDesc, ID3D11SamplerState** ppSamplerState)
	{
		if (ppSamplerState)
			*ppSamplerState = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateQuery(const D3D11_QUERY_DESC* pQueryDesc, ID3D11Query** ppQuery)
	{
		if (ppQuery)
			*ppQuery = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreatePredicate(const D3D11_QUERY_DESC* pPredicateDesc, ID3D11Predicate** ppPredicate)
	{
		if (ppPredicate)
			*ppPredicate = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateCounter(const D3D11_COUNTER_DESC* pCounterDesc, ID3D11Counter** ppCounter)
	{
		if (ppCounter)
			*ppCounter = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateDeferredContext(UINT ContextFlags, ID3D11DeviceContext** ppDeferredContext)
	{
		if (ppDeferredContext)
			*ppDeferredContext = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface, void** ppResource)
	{
		if (ppResource)
			*ppResource = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CheckFormatSupport(DXGI_FORMAT Format, UINT* pFormatSupport)
	{
		if (pFormatSupport)
			pFormatSupport = 0;
		return E_FAIL;
	}

	virtual HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount, UINT* pNumQualityLevels)
	{
		if (pNumQualityLevels)
			pNumQualityLevels = 0;
		return S_FALSE;
	}

	virtual void STDMETHODCALLTYPE CheckCounterInfo(D3D11_COUNTER_INFO* pCounterInfo)
	{
		if (pCounterInfo)
		{
			pCounterInfo->LastDeviceDependentCounter = D3D11_COUNTER_DEVICE_DEPENDENT_0;
			pCounterInfo->NumDetectableParallelUnits = 0;
			pCounterInfo->NumSimultaneousCounters = 0;
		}
	}

	virtual HRESULT STDMETHODCALLTYPE CheckCounter(const D3D11_COUNTER_DESC* pDesc, D3D11_COUNTER_TYPE* pType, UINT* pActiveCounters, LPSTR szName, UINT* pNameLength, LPSTR szUnits, UINT* pUnitsLength, LPSTR szDescription, UINT* pDescriptionLength)
	{
		if (pType)
			*pType = D3D11_COUNTER_TYPE_UINT32;
		if (pActiveCounters)
			*pActiveCounters = 0;
		if (szName)
			*szName = 0;
		if (pNameLength)
			*pNameLength = 0;
		if (szUnits)
			*szUnits = 0;
		if (pUnitsLength)
			*pUnitsLength = 0;
		if (szDescription)
			*szDescription = 0;
		if (pDescriptionLength)
			*pDescriptionLength = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CheckFeatureSupport(D3D11_FEATURE Feature, void* pFeatureSupportData, UINT FeatureSupportDataSize)
	{
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData)
	{
		if (pDataSize)
			*pDataSize = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void* pData)
	{
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown* pData)
	{
		return S_FALSE;
	}

	virtual D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel()
	{
		return D3D_FEATURE_LEVEL_11_0;
	}

	virtual UINT STDMETHODCALLTYPE GetCreationFlags()
	{
		return 0;
	}

	virtual HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason()
	{
		return S_OK;
	}

	virtual void STDMETHODCALLTYPE    GetImmediateContext(ID3D11DeviceContext** ppImmediateContext);

	virtual HRESULT STDMETHODCALLTYPE SetExceptionMode(UINT RaiseFlags)
	{
		return S_FALSE;
	}

	virtual UINT STDMETHODCALLTYPE GetExceptionMode()
	{
		return 0;
	}

	#if (CRY_RENDERER_DIRECT3D >= 111)
	virtual void STDMETHODCALLTYPE    GetImmediateContext1(ID3D11DeviceContext1** ppImmediateContext);

	virtual HRESULT STDMETHODCALLTYPE CreateDeferredContext1(UINT ContextFlags, ID3D11DeviceContext1** ppDeferredContext)
	{
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateBlendState1(const D3D11_BLEND_DESC1* pBlendStateDesc, ID3D11BlendState1** ppBlendState)
	{
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateRasterizerState1(const D3D11_RASTERIZER_DESC1* pRasterizerDesc, ID3D11RasterizerState1** ppRasterizerState)
	{
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateDeviceContextState(UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, REFIID EmulatedInterface, D3D_FEATURE_LEVEL* pChosenFeatureLevel, ID3DDeviceContextState** ppContextState)
	{
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE OpenSharedResource1(HANDLE hResource, REFIID returnedInterface, void** ppResource)
	{
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE OpenSharedResourceByName(LPCWSTR lpName, DWORD dwDesiredAccess, REFIID returnedInterface, void** ppResource)
	{
		return S_FALSE;
	}
	#endif

public:
	NullD3D11Device() : m_refCount(1), m_pImmediateCtx(0) {}

protected:
	virtual ~NullD3D11Device()
	{
		SAFE_RELEASE(m_pImmediateCtx);
	}

protected:
	volatile int m_refCount;

	#if (CRY_RENDERER_DIRECT3D >= 111)
	ID3D11DeviceContext1* m_pImmediateCtx;
	#else
	ID3D11DeviceContext*  m_pImmediateCtx;
	#endif
};

namespace NullD3D11DeviceInternal
{

	#if (CRY_RENDERER_DIRECT3D >= 111)
class DeviceContext : public ID3D11DeviceContext1
	#else
class DeviceContext : public ID3D11DeviceContext
	#endif
{
public:
	// IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj)
	{
		if (ppvObj)
			*ppvObj = 0;
		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef()
	{
		return CryInterlockedIncrement(&m_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release()
	{
		long refCount = CryInterlockedDecrement(&m_refCount);
		if (refCount <= 0)
			delete this;
		return refCount;
	};

	// ID3D11DeviceChild
	virtual void STDMETHODCALLTYPE GetDevice(ID3D11Device** ppDevice)
	{
		if (ppDevice)
		{
			if (m_pDevice)
				m_pDevice->AddRef();
			*ppDevice = m_pDevice;
		}
	}

	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData)
	{
		if (pDataSize)
			*pDataSize = 0;
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void* pData)
	{
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown* pData)
	{
		return S_FALSE;
	}

	// ID3D11DeviceContext
	virtual void STDMETHODCALLTYPE    VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                        {}
	virtual void STDMETHODCALLTYPE    PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)          {}
	virtual void STDMETHODCALLTYPE    PSSetShader(ID3D11PixelShader* pPixelShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)   {}
	virtual void STDMETHODCALLTYPE    PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                               {}
	virtual void STDMETHODCALLTYPE    VSSetShader(ID3D11VertexShader* pVertexShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) {}
	virtual void STDMETHODCALLTYPE    DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)                                        {}
	virtual void STDMETHODCALLTYPE    Draw(UINT VertexCount, UINT StartVertexLocation)                                                                     {}

	virtual HRESULT STDMETHODCALLTYPE Map(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource)
	{
		if (pMappedResource)
		{
			pMappedResource->RowPitch = 0;
			pMappedResource->DepthPitch = 0;
			pMappedResource->pData = 0;
		}
		return S_FALSE;
	}

	virtual void STDMETHODCALLTYPE    Unmap(ID3D11Resource* pResource, UINT Subresource)                                                                                                {}
	virtual void STDMETHODCALLTYPE    PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                     {}
	virtual void STDMETHODCALLTYPE    IASetInputLayout(ID3D11InputLayout* pInputLayout)                                                                                                 {}
	virtual void STDMETHODCALLTYPE    IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const UINT* pStrides, const UINT* pOffsets)             {}
	virtual void STDMETHODCALLTYPE    IASetIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, UINT Offset)                                                                     {}
	virtual void STDMETHODCALLTYPE    DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation) {}
	virtual void STDMETHODCALLTYPE    DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)                              {}
	virtual void STDMETHODCALLTYPE    GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                     {}
	virtual void STDMETHODCALLTYPE    GSSetShader(ID3D11GeometryShader* pShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                  {}
	virtual void STDMETHODCALLTYPE    IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology)                                                                                         {}
	virtual void STDMETHODCALLTYPE    VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                       {}
	virtual void STDMETHODCALLTYPE    VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                            {}
	virtual void STDMETHODCALLTYPE    Begin(ID3D11Asynchronous* pAsync)                                                                                                                 {}
	virtual void STDMETHODCALLTYPE    End(ID3D11Asynchronous* pAsync)                                                                                                                   {}

	virtual HRESULT STDMETHODCALLTYPE GetData(ID3D11Asynchronous* pAsync, void* pData, UINT DataSize, UINT GetDataFlags)
	{
		return S_FALSE;
	}

	virtual void STDMETHODCALLTYPE  SetPredication(ID3D11Predicate* pPredicate, BOOL PredicateValue)                                                                                                                                                                                                                 {}
	virtual void STDMETHODCALLTYPE  GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                                                                                                                                                      {}
	virtual void STDMETHODCALLTYPE  GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                                                                                                                                                           {}
	virtual void STDMETHODCALLTYPE  OMSetRenderTargets(UINT NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView)                                                                                                                                                 {}
	virtual void STDMETHODCALLTYPE  OMSetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts) {}
	virtual void STDMETHODCALLTYPE  OMSetBlendState(ID3D11BlendState* pBlendState, const FLOAT BlendFactor[4], UINT SampleMask)                                                                                                                                                                                      {}
	virtual void STDMETHODCALLTYPE  OMSetDepthStencilState(ID3D11DepthStencilState* pDepthStencilState, UINT StencilRef)                                                                                                                                                                                             {}
	virtual void STDMETHODCALLTYPE  SOSetTargets(UINT NumBuffers, ID3D11Buffer* const* ppSOTargets, const UINT* pOffsets)                                                                                                                                                                                            {}
	virtual void STDMETHODCALLTYPE  DrawAuto(void)                                                                                                                                                                                                                                                                   {}
	virtual void STDMETHODCALLTYPE  DrawIndexedInstancedIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)                                                                                                                                                                                        {}
	virtual void STDMETHODCALLTYPE  DrawInstancedIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)                                                                                                                                                                                               {}
	virtual void STDMETHODCALLTYPE  Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)                                                                                                                                                                                                 {}
	virtual void STDMETHODCALLTYPE  DispatchIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs)                                                                                                                                                                                                    {}
	virtual void STDMETHODCALLTYPE  RSSetState(ID3D11RasterizerState* pRasterizerState)                                                                                                                                                                                                                              {}
	virtual void STDMETHODCALLTYPE  RSSetViewports(UINT NumViewports, const D3D11_VIEWPORT* pViewports)                                                                                                                                                                                                              {}
	virtual void STDMETHODCALLTYPE  RSSetScissorRects(UINT NumRects, const D3D11_RECT* pRects)                                                                                                                                                                                                                       {}
	virtual void STDMETHODCALLTYPE  CopySubresourceRegion(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox)                                                                                           {}
	virtual void STDMETHODCALLTYPE  CopyResource(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource)                                                                                                                                                                                                         {}
	virtual void STDMETHODCALLTYPE  UpdateSubresource(ID3D11Resource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)                                                                                                                       {}
	virtual void STDMETHODCALLTYPE  CopyStructureCount(ID3D11Buffer* pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView* pSrcView)                                                                                                                                                                     {}
	virtual void STDMETHODCALLTYPE  ClearRenderTargetView(ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4])                                                                                                                                                                                       {}
	virtual void STDMETHODCALLTYPE  ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView* pUnorderedAccessView, const UINT Values[4])                                                                                                                                                                              {}
	virtual void STDMETHODCALLTYPE  ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView* pUnorderedAccessView, const FLOAT Values[4])                                                                                                                                                                            {}
	virtual void STDMETHODCALLTYPE  ClearDepthStencilView(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)                                                                                                                                                                    {}
	virtual void STDMETHODCALLTYPE  GenerateMips(ID3D11ShaderResourceView* pShaderResourceView)                                                                                                                                                                                                                      {}
	virtual void STDMETHODCALLTYPE  SetResourceMinLOD(ID3D11Resource* pResource, FLOAT MinLOD)                                                                                                                                                                                                                       {}

	virtual FLOAT STDMETHODCALLTYPE GetResourceMinLOD(ID3D11Resource* pResource)
	{
		return 0;
	}

	virtual void STDMETHODCALLTYPE ResolveSubresource(ID3D11Resource* pDstResource, UINT DstSubresource, ID3D11Resource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)     {}
	virtual void STDMETHODCALLTYPE ExecuteCommandList(ID3D11CommandList* pCommandList, BOOL RestoreContextState)                                                                    {}
	virtual void STDMETHODCALLTYPE HSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                      {}
	virtual void STDMETHODCALLTYPE HSSetShader(ID3D11HullShader* pHullShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                                 {}
	virtual void STDMETHODCALLTYPE HSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                           {}
	virtual void STDMETHODCALLTYPE HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                    {}
	virtual void STDMETHODCALLTYPE DSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                      {}
	virtual void STDMETHODCALLTYPE DSSetShader(ID3D11DomainShader* pDomainShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                             {}
	virtual void STDMETHODCALLTYPE DSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                           {}
	virtual void STDMETHODCALLTYPE DSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                    {}
	virtual void STDMETHODCALLTYPE CSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews)                                      {}
	virtual void STDMETHODCALLTYPE CSSetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts) {}
	virtual void STDMETHODCALLTYPE CSSetShader(ID3D11ComputeShader* pComputeShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances)                           {}
	virtual void STDMETHODCALLTYPE CSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers)                                                           {}
	virtual void STDMETHODCALLTYPE CSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers)                                                    {}

	virtual void STDMETHODCALLTYPE VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)
	{
		if (ppConstantBuffers)
		{
			for (UINT i = 0; i < NumBuffers; ++i)
				ppConstantBuffers[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)
	{
		if (ppShaderResourceViews)
		{
			for (UINT i = 0; i < NumViews; ++i)
				ppShaderResourceViews[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE PSGetShader(ID3D11PixelShader** ppPixelShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)
	{
		if (ppPixelShader)
			*ppPixelShader = 0;
		if (ppClassInstances)
			*ppClassInstances = 0;
		if (pNumClassInstances)
			pNumClassInstances = 0;
	}

	virtual void STDMETHODCALLTYPE PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)
	{
		if (ppSamplers)
		{
			for (UINT i = 0; i < NumSamplers; ++i)
				ppSamplers[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE VSGetShader(ID3D11VertexShader** ppVertexShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)
	{
		if (ppVertexShader)
			*ppVertexShader = 0;
		if (ppClassInstances)
			*ppClassInstances = 0;
		if (pNumClassInstances)
			pNumClassInstances = 0;
	}

	virtual void STDMETHODCALLTYPE PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)
	{
		if (ppConstantBuffers)
		{
			for (UINT i = 0; i < NumBuffers; ++i)
				ppConstantBuffers[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE IAGetInputLayout(ID3D11InputLayout** ppInputLayout)
	{
		if (ppInputLayout)
			*ppInputLayout = 0;
	}

	virtual void STDMETHODCALLTYPE IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppVertexBuffers, UINT* pStrides, UINT* pOffsets)
	{
		if (ppVertexBuffers)
		{
			for (UINT i = 0; i < NumBuffers; ++i)
			{
				ppVertexBuffers[i] = 0;
				pStrides[i] = 0;
				pOffsets[i] = 0;
			}
		}
	}

	virtual void STDMETHODCALLTYPE IAGetIndexBuffer(ID3D11Buffer** pIndexBuffer, DXGI_FORMAT* Format, UINT* Offset)
	{
		if (pIndexBuffer)
			pIndexBuffer = 0;
		if (Format)
			*Format = DXGI_FORMAT_UNKNOWN;
		if (Offset)
			*Offset = 0;
	}

	virtual void STDMETHODCALLTYPE GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)
	{
		if (ppConstantBuffers)
		{
			for (UINT i = 0; i < NumBuffers; ++i)
				ppConstantBuffers[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE GSGetShader(ID3D11GeometryShader** ppGeometryShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)
	{
		if (ppGeometryShader)
			*ppGeometryShader = 0;
		if (ppClassInstances)
			*ppClassInstances = 0;
		if (pNumClassInstances)
			pNumClassInstances = 0;
	}

	virtual void STDMETHODCALLTYPE IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* pTopology)
	{
		if (pTopology)
			*pTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	}

	virtual void STDMETHODCALLTYPE VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)
	{
		if (ppShaderResourceViews)
		{
			for (UINT i = 0; i < NumViews; ++i)
				ppShaderResourceViews[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)
	{
		if (ppSamplers)
		{
			for (UINT i = 0; i < NumSamplers; ++i)
				ppSamplers[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE GetPredication(ID3D11Predicate** ppPredicate, BOOL* pPredicateValue)
	{
		if (ppPredicate)
			*ppPredicate = 0;
		if (pPredicateValue)
			*pPredicateValue = FALSE;
	}

	virtual void STDMETHODCALLTYPE GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)
	{
		if (ppShaderResourceViews)
		{
			for (UINT i = 0; i < NumViews; ++i)
				ppShaderResourceViews[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)
	{
		if (ppSamplers)
		{
			for (UINT i = 0; i < NumSamplers; ++i)
				ppSamplers[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE OMGetRenderTargets(UINT NumViews, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView)
	{
		if (ppRenderTargetViews)
		{
			for (UINT i = 0; i < NumViews; ++i)
				ppRenderTargetViews[i] = 0;
		}

		if (ppDepthStencilView)
			*ppDepthStencilView = 0;
	}

	virtual void STDMETHODCALLTYPE OMGetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews)
	{
		if (ppRenderTargetViews)
		{
			for (UINT i = 0; i < NumRTVs; ++i)
				ppRenderTargetViews[i] = 0;
		}

		if (ppDepthStencilView)
			*ppDepthStencilView = 0;

		if (ppUnorderedAccessViews)
		{
			for (UINT i = 0; i < NumUAVs; ++i)
				ppUnorderedAccessViews[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE OMGetBlendState(ID3D11BlendState** ppBlendState, FLOAT BlendFactor[4], UINT* pSampleMask)
	{
		if (ppBlendState)
			*ppBlendState = 0;
		BlendFactor[0] = BlendFactor[1] = BlendFactor[2] = BlendFactor[3] = 0;
		if (pSampleMask)
			*pSampleMask = 0;
	}

	virtual void STDMETHODCALLTYPE OMGetDepthStencilState(ID3D11DepthStencilState** ppDepthStencilState, UINT* pStencilRef)
	{
		if (ppDepthStencilState)
			*ppDepthStencilState = 0;
		if (pStencilRef)
			*pStencilRef = 0;
	}

	virtual void STDMETHODCALLTYPE SOGetTargets(UINT NumBuffers, ID3D11Buffer** ppSOTargets)
	{
		if (ppSOTargets)
		{
			for (UINT i = 0; i < NumBuffers; ++i)
				ppSOTargets[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE RSGetState(ID3D11RasterizerState** ppRasterizerState)
	{
		if (ppRasterizerState)
			*ppRasterizerState = 0;
	}

	virtual void STDMETHODCALLTYPE RSGetViewports(UINT* pNumViewports, D3D11_VIEWPORT* pViewports)
	{
		if (pNumViewports)
			*pNumViewports = 0;
	}

	virtual void STDMETHODCALLTYPE RSGetScissorRects(UINT* pNumRects, D3D11_RECT* pRects)
	{
		if (pNumRects)
			*pNumRects = 0;
	}

	virtual void STDMETHODCALLTYPE HSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)
	{
		if (ppShaderResourceViews)
		{
			for (UINT i = 0; i < NumViews; ++i)
				ppShaderResourceViews[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE HSGetShader(ID3D11HullShader** ppHullShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)
	{
		if (ppHullShader)
			*ppHullShader = 0;
		if (ppClassInstances)
			*ppClassInstances = 0;
		if (pNumClassInstances)
			pNumClassInstances = 0;
	}

	virtual void STDMETHODCALLTYPE HSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)
	{
		if (ppSamplers)
		{
			for (UINT i = 0; i < NumSamplers; ++i)
				ppSamplers[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)
	{
		if (ppConstantBuffers)
		{
			for (UINT i = 0; i < NumBuffers; ++i)
				ppConstantBuffers[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE DSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)
	{
		if (ppShaderResourceViews)
		{
			for (UINT i = 0; i < NumViews; ++i)
				ppShaderResourceViews[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE DSGetShader(ID3D11DomainShader** ppDomainShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)
	{
		if (ppDomainShader)
			*ppDomainShader = 0;
		if (ppClassInstances)
			*ppClassInstances = 0;
		if (pNumClassInstances)
			pNumClassInstances = 0;
	}

	virtual void STDMETHODCALLTYPE DSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)
	{
		if (ppSamplers)
		{
			for (UINT i = 0; i < NumSamplers; ++i)
				ppSamplers[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE DSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)
	{
		if (ppConstantBuffers)
		{
			for (UINT i = 0; i < NumBuffers; ++i)
				ppConstantBuffers[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE CSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews)
	{
		if (ppShaderResourceViews)
		{
			for (UINT i = 0; i < NumViews; ++i)
				ppShaderResourceViews[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE CSGetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews)
	{
		if (ppUnorderedAccessViews)
		{
			for (UINT i = 0; i < NumUAVs; ++i)
				ppUnorderedAccessViews[i] = 0;
		}
	}
	virtual void STDMETHODCALLTYPE CSGetShader(ID3D11ComputeShader** ppComputeShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances)
	{
		if (ppComputeShader)
			*ppComputeShader = 0;
		if (ppClassInstances)
			*ppClassInstances = 0;
		if (pNumClassInstances)
			pNumClassInstances = 0;
	}

	virtual void STDMETHODCALLTYPE CSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers)
	{
		if (ppSamplers)
		{
			for (UINT i = 0; i < NumSamplers; ++i)
				ppSamplers[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE CSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers)
	{
		if (ppConstantBuffers)
		{
			for (UINT i = 0; i < NumBuffers; ++i)
				ppConstantBuffers[i] = 0;
		}
	}

	virtual void STDMETHODCALLTYPE                      ClearState() {}
	virtual void STDMETHODCALLTYPE                      Flush()      {}

	virtual D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType()
	{
		return D3D11_DEVICE_CONTEXT_IMMEDIATE;
	}

	virtual UINT STDMETHODCALLTYPE GetContextFlags()
	{
		return 0;
	}

	virtual HRESULT STDMETHODCALLTYPE FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList** ppCommandList)
	{
		if (ppCommandList)
			*ppCommandList = 0;
		return DXGI_ERROR_INVALID_CALL;
	}

	#if (CRY_RENDERER_DIRECT3D >= 111)
	void STDMETHODCALLTYPE CopySubresourceRegion1(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox, UINT CopyFlags) {}
	void STDMETHODCALLTYPE CopyResource1(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource, UINT CopyFlags)                                                                                                               {}
	void STDMETHODCALLTYPE UpdateSubresource1(ID3D11Resource* pDstResource, UINT DstSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags)                             {}
	void STDMETHODCALLTYPE DiscardResource(ID3D11Resource* pResource)                                                                                                                                                              {}
	void STDMETHODCALLTYPE DiscardView(ID3D11View* pResourceView)                                                                                                                                                                  {}

	void STDMETHODCALLTYPE VSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)                                                   {}
	void STDMETHODCALLTYPE HSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)                                                   {}
	void STDMETHODCALLTYPE DSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)                                                   {}
	void STDMETHODCALLTYPE GSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)                                                   {}
	void STDMETHODCALLTYPE PSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)                                                   {}
	void STDMETHODCALLTYPE CSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)                                                   {}

	void STDMETHODCALLTYPE VSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)                                                                     {}
	void STDMETHODCALLTYPE HSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)                                                                     {}
	void STDMETHODCALLTYPE DSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)                                                                     {}
	void STDMETHODCALLTYPE GSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)                                                                     {}
	void STDMETHODCALLTYPE PSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)                                                                     {}
	void STDMETHODCALLTYPE CSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)                                                                     {}

	void STDMETHODCALLTYPE SwapDeviceContextState(ID3DDeviceContextState* pState, ID3DDeviceContextState** ppPreviousState)                                                                                                        {}
	void STDMETHODCALLTYPE ClearView(ID3D11View* pView, const FLOAT Color[4], const D3D11_RECT* pRect, UINT NumRects)                                                                                                              {}
	void STDMETHODCALLTYPE DiscardView1(ID3D11View* pResourceView, const D3D11_RECT* pRects, UINT NumRects)                                                                                                                        {}
	#endif

protected:
	virtual ~DeviceContext()
	{
	}

	#if (CRY_RENDERER_DIRECT3D >= 111)
public:
	DeviceContext(ID3D11Device1* pDevice) : m_refCount(1), m_pDevice(pDevice) {}

protected:
	volatile int   m_refCount;
	ID3D11Device1* m_pDevice;
	#else
public:
	DeviceContext(ID3D11Device* pDevice) : m_refCount(1), m_pDevice(pDevice) {}

protected:
	volatile int  m_refCount;
	ID3D11Device* m_pDevice;
	#endif
};

} // namespace NullD3D11DeviceInternal

void NullD3D11Device::GetImmediateContext(ID3D11DeviceContext** ppImmediateContext)
{
	if (ppImmediateContext)
	{
		if (!m_pImmediateCtx)
		{
			m_pImmediateCtx = new NullD3D11DeviceInternal::DeviceContext(this);
		}

		m_pImmediateCtx->AddRef();
		*ppImmediateContext = m_pImmediateCtx;
	}
}

	#if (CRY_RENDERER_DIRECT3D >= 111)
void NullD3D11Device::GetImmediateContext1(ID3D11DeviceContext1** ppImmediateContext)
{
	if (ppImmediateContext)
	{
		if (!m_pImmediateCtx)
		{
			m_pImmediateCtx = new NullD3D11DeviceInternal::DeviceContext(this);
		}

		m_pImmediateCtx->AddRef();
		*ppImmediateContext = m_pImmediateCtx;
	}
}
	#endif

#endif // #if defined(ENABLE_NULL_D3D11DEVICE)
