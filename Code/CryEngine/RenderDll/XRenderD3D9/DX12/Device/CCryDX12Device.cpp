// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryDX12Device.hpp"

#include "CCryDX12DeviceContext.hpp"

#include "DX12/GI/CCryDX12GIAdapter.hpp"

#include "DX12/Resource/Misc/CCryDX12Buffer.hpp"
#include "DX12/Resource/Misc/CCryDX12InputLayout.hpp"
#include "DX12/Resource/Misc/CCryDX12Query.hpp"
#include "DX12/Resource/Misc/CCryDX12Shader.hpp"

#include "DX12/Resource/State/CCryDX12BlendState.hpp"
#include "DX12/Resource/State/CCryDX12DepthStencilState.hpp"
#include "DX12/Resource/State/CCryDX12RasterizerState.hpp"
#include "DX12/Resource/State/CCryDX12SamplerState.hpp"

#include "DX12/Resource/Texture/CCryDX12Texture1D.hpp"
#include "DX12/Resource/Texture/CCryDX12Texture2D.hpp"
#include "DX12/Resource/Texture/CCryDX12Texture3D.hpp"

#include "DX12/Resource/View/CCryDX12DepthStencilView.hpp"
#include "DX12/Resource/View/CCryDX12RenderTargetView.hpp"
#include "DX12/Resource/View/CCryDX12ShaderResourceView.hpp"
#include "DX12/Resource/View/CCryDX12UnorderedAccessView.hpp"

CCryDX12Device* CCryDX12Device::Create(CCryDX12GIAdapter* pAdapter, D3D_FEATURE_LEVEL* pFeatureLevel)
{
	DX12_PTR(NCryDX12::CDevice) device = NCryDX12::CDevice::Create(pAdapter, pFeatureLevel);

	if (!device)
	{
		DX12_ERROR("Could not create DX12 Device!");
		return NULL;
	}

	return DX12_NEW_RAW(CCryDX12Device(device));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12Device::CCryDX12Device(NCryDX12::CDevice* device)
	: Super()
	, m_pDevice(device)
{
	DX12_FUNC_LOG
		
#ifdef DX12_LINKEDADAPTER
	// TODO: CVar ...
	if (CRenderer::CV_r_StereoEnableMgpu)
	{
		const UINT numNodes = m_numNodes = m_pDevice->GetNodeCount();
		const UINT allMask = m_allMask = (1UL << numNodes) - 1UL;
		const UINT crtMask = m_crtMask = allMask;
		const UINT visMask = m_visMask = allMask;
		const UINT shrMask = m_shrMask = 1U;

		m_pMainContext = CCryDX12DeviceContext::Create(this, allMask, false);

		//		m_pNodeContexts.reserve(numNodes);
		//		for (UINT c = 0, n = numNodes; c < n; ++c)
		//			m_pNodeContexts.push_back(CCryDX12DeviceContext::Create(this, 1UL << c, false));
	}
	else
#endif
	{
		const UINT numNodes = m_numNodes = 1U;
		const UINT allMask = m_allMask = (1UL << numNodes) - 1UL;
		const UINT crtMask = m_crtMask = 1U;
		const UINT visMask = m_visMask = 1U;
		const UINT shrMask = m_shrMask = 1U;

		m_pMainContext = CCryDX12DeviceContext::Create(this, allMask, false);
	}
	//report the node count used
	gRenDev->m_adapterInfo.nNodeCount = m_numNodes;
}

#pragma region /* ID3D11Device implementation */

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateBuffer(
  _In_ const D3D11_BUFFER_DESC* pDesc,
  _In_opt_ const D3D11_SUBRESOURCE_DATA* pInitialData,
  _Out_opt_ ID3D11Buffer** ppBuffer)
{
	*ppBuffer = CCryDX12Buffer::Create(this, pDesc, pInitialData);
	return *ppBuffer ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateTexture1D(
  _In_ const D3D11_TEXTURE1D_DESC* pDesc,
  _In_reads_opt_(_Inexpressible_(pDesc->MipLevels * pDesc->ArraySize)) const D3D11_SUBRESOURCE_DATA* pInitialData,
  _Out_opt_ ID3D11Texture1D** ppTexture1D)
{
	DX12_FUNC_LOG
	* ppTexture1D = CCryDX12Texture1D::Create(this, nullptr, pDesc, pInitialData);
	return *ppTexture1D ? S_OK : S_FALSE;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateTexture2D(
  _In_ const D3D11_TEXTURE2D_DESC* pDesc,
  _In_reads_opt_(_Inexpressible_(pDesc->MipLevels * pDesc->ArraySize)) const D3D11_SUBRESOURCE_DATA* pInitialData,
  _Out_opt_ ID3D11Texture2D** ppTexture2D)
{
	DX12_FUNC_LOG
	* ppTexture2D = CCryDX12Texture2D::Create(this, nullptr, pDesc, pInitialData);
	return *ppTexture2D ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateTexture3D(
  _In_ const D3D11_TEXTURE3D_DESC* pDesc,
  _In_reads_opt_(_Inexpressible_(pDesc->MipLevels))  const D3D11_SUBRESOURCE_DATA* pInitialData,
  _Out_opt_ ID3D11Texture3D** ppTexture3D)
{
	DX12_FUNC_LOG
	* ppTexture3D = CCryDX12Texture3D::Create(this, nullptr, pDesc, pInitialData);
	return *ppTexture3D ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateShaderResourceView(
  _In_ ID3D11Resource* pResource,
  _In_opt_ const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc,
  _Out_opt_ ID3D11ShaderResourceView** ppSRView)
{
	DX12_FUNC_LOG
	* ppSRView = NULL;

	if (CCryDX12ShaderResourceView* pResult = CCryDX12ShaderResourceView::Create(this, pResource, pDesc))
	{
		auto descriptorHandle = GetDX12Device()->CacheShaderResourceView(&pResult->GetDX12View().GetSRVDesc(), DX12_EXTRACT_D3D12RESOURCE(pResource));
		pResult->GetDX12View().SetDescriptorHandle(descriptorHandle);

		if (INVALID_CPU_DESCRIPTOR_HANDLE == descriptorHandle)
			SAFE_RELEASE(pResult);

		*ppSRView = pResult;
	}

	return *ppSRView ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateUnorderedAccessView(
  _In_ ID3D11Resource* pResource,
  _In_opt_ const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
  _Out_opt_ ID3D11UnorderedAccessView** ppUAView)
{
	DX12_FUNC_LOG
	* ppUAView = NULL;

	if (CCryDX12UnorderedAccessView* pResult = CCryDX12UnorderedAccessView::Create(this, pResource, pDesc))
	{
		auto descriptorHandle = GetDX12Device()->CacheUnorderedAccessView(&pResult->GetDX12View().GetUAVDesc(), DX12_EXTRACT_D3D12RESOURCE(pResource));
		pResult->GetDX12View().SetDescriptorHandle(descriptorHandle);

		if (INVALID_CPU_DESCRIPTOR_HANDLE == descriptorHandle)
			SAFE_RELEASE(pResult);

		*ppUAView = pResult;
	}

	return *ppUAView ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateRenderTargetView(
  _In_ ID3D11Resource* pResource,
  _In_opt_ const D3D11_RENDER_TARGET_VIEW_DESC* pDesc,
  _Out_opt_ ID3D11RenderTargetView** ppRTView)
{
	DX12_FUNC_LOG
	* ppRTView = NULL;

	if (CCryDX12RenderTargetView* pResult = CCryDX12RenderTargetView::Create(this, pResource, pDesc))
	{
		auto descriptorHandle = GetDX12Device()->CacheRenderTargetView(&pResult->GetDX12View().GetRTVDesc(), DX12_EXTRACT_D3D12RESOURCE(pResource));
		pResult->GetDX12View().SetDescriptorHandle(descriptorHandle);

		if (INVALID_CPU_DESCRIPTOR_HANDLE == descriptorHandle)
			SAFE_RELEASE(pResult);

		*ppRTView = pResult;
	}

	return *ppRTView ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateDepthStencilView(
  _In_ ID3D11Resource* pResource,
  _In_opt_ const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc,
  _Out_opt_ ID3D11DepthStencilView** ppDSView)
{
	DX12_FUNC_LOG
	* ppDSView = NULL;

	if (CCryDX12DepthStencilView* pResult = CCryDX12DepthStencilView::Create(this, pResource, pDesc))
	{
		auto descriptorHandle = GetDX12Device()->CacheDepthStencilView(&pResult->GetDX12View().GetDSVDesc(), DX12_EXTRACT_D3D12RESOURCE(pResource));
		pResult->GetDX12View().SetDescriptorHandle(descriptorHandle);

		if (INVALID_CPU_DESCRIPTOR_HANDLE == descriptorHandle)
			SAFE_RELEASE(pResult);

		*ppDSView = pResult;
	}

	return *ppDSView ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateInputLayout(
  _In_reads_(NumElements)  const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs,
  _In_range_(0, D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT)  UINT NumElements,
  _In_ const void* pShaderBytecodeWithInputSignature,
  _In_ SIZE_T BytecodeLength,
  _Out_opt_ ID3D11InputLayout** ppInputLayout)
{
	DX12_FUNC_LOG
	* ppInputLayout = CCryDX12InputLayout::Create(this, pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength);
	return *ppInputLayout ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateVertexShader(
  _In_ const void* pShaderBytecode,
  _In_ SIZE_T BytecodeLength,
  _In_opt_ ID3D11ClassLinkage* pClassLinkage,
  _Out_opt_ ID3D11VertexShader** ppVertexShader)
{
	DX12_FUNC_LOG
	* ppVertexShader = reinterpret_cast<ID3D11VertexShader*>(CCryDX12Shader::Create(this, pShaderBytecode, BytecodeLength, pClassLinkage));
	return *ppVertexShader ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateGeometryShader(
  _In_ const void* pShaderBytecode,
  _In_ SIZE_T BytecodeLength,
  _In_opt_ ID3D11ClassLinkage* pClassLinkage,
  _Out_opt_ ID3D11GeometryShader** ppGeometryShader)
{
	DX12_FUNC_LOG
	* ppGeometryShader = reinterpret_cast<ID3D11GeometryShader*>(CCryDX12Shader::Create(this, pShaderBytecode, BytecodeLength, pClassLinkage));
	return *ppGeometryShader ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateGeometryShaderWithStreamOutput(
  _In_ const void* pShaderBytecode,
  _In_ SIZE_T BytecodeLength,
  _In_reads_opt_(NumEntries)  const D3D11_SO_DECLARATION_ENTRY* pSODeclaration,
  _In_range_(0, D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT)  UINT NumEntries,
  _In_reads_opt_(NumStrides)  const UINT* pBufferStrides,
  _In_range_(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumStrides,
  _In_ UINT RasterizedStream,
  _In_opt_ ID3D11ClassLinkage* pClassLinkage,
  _Out_opt_ ID3D11GeometryShader** ppGeometryShader)
{
	DX12_FUNC_LOG
	return -1;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreatePixelShader(
  _In_ const void* pShaderBytecode,
  _In_ SIZE_T BytecodeLength,
  _In_opt_ ID3D11ClassLinkage* pClassLinkage,
  _Out_opt_ ID3D11PixelShader** ppPixelShader)
{
	DX12_FUNC_LOG
	* ppPixelShader = reinterpret_cast<ID3D11PixelShader*>(CCryDX12Shader::Create(this, pShaderBytecode, BytecodeLength, pClassLinkage));
	return *ppPixelShader ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateHullShader(
  _In_ const void* pShaderBytecode,
  _In_ SIZE_T BytecodeLength,
  _In_opt_ ID3D11ClassLinkage* pClassLinkage,
  _Out_opt_ ID3D11HullShader** ppHullShader)
{
	DX12_FUNC_LOG
	* ppHullShader = reinterpret_cast<ID3D11HullShader*>(CCryDX12Shader::Create(this, pShaderBytecode, BytecodeLength, pClassLinkage));
	return *ppHullShader ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateDomainShader(
  _In_ const void* pShaderBytecode,
  _In_ SIZE_T BytecodeLength,
  _In_opt_ ID3D11ClassLinkage* pClassLinkage,
  _Out_opt_ ID3D11DomainShader** ppDomainShader)
{
	DX12_FUNC_LOG
	* ppDomainShader = reinterpret_cast<ID3D11DomainShader*>(CCryDX12Shader::Create(this, pShaderBytecode, BytecodeLength, pClassLinkage));
	return *ppDomainShader ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateComputeShader(
  _In_ const void* pShaderBytecode,
  _In_ SIZE_T BytecodeLength,
  _In_opt_ ID3D11ClassLinkage* pClassLinkage,
  _Out_opt_ ID3D11ComputeShader** ppComputeShader)
{
	DX12_FUNC_LOG
	* ppComputeShader = reinterpret_cast<ID3D11ComputeShader*>(CCryDX12Shader::Create(this, pShaderBytecode, BytecodeLength, pClassLinkage));
	return *ppComputeShader ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateClassLinkage(
  _Out_ ID3D11ClassLinkage** ppLinkage)
{
	DX12_FUNC_LOG
	return -1;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateBlendState(
  _In_ const D3D11_BLEND_DESC* pBlendStateDesc,
  _Out_opt_ ID3D11BlendState** ppBlendState)
{
	DX12_FUNC_LOG
	* ppBlendState = CCryDX12BlendState::Create(pBlendStateDesc);
	return *ppBlendState ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateDepthStencilState(
  _In_ const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc,
  _Out_opt_ ID3D11DepthStencilState** ppDepthStencilState)
{
	DX12_FUNC_LOG
	* ppDepthStencilState = CCryDX12DepthStencilState::Create(pDepthStencilDesc);
	return *ppDepthStencilState ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateRasterizerState(
  _In_ const D3D11_RASTERIZER_DESC* pRasterizerDesc,
  _Out_opt_ ID3D11RasterizerState** ppRasterizerState)
{
	DX12_FUNC_LOG
	* ppRasterizerState = CCryDX12RasterizerState::Create(pRasterizerDesc);
	return *ppRasterizerState ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateSamplerState(
  _In_ const D3D11_SAMPLER_DESC* pSamplerDesc,
  _Out_opt_ ID3D11SamplerState** ppSamplerState)
{
	DX12_FUNC_LOG
	* ppSamplerState = NULL;

	if (CCryDX12SamplerState* pResult = CCryDX12SamplerState::Create(pSamplerDesc))
	{
		auto descriptorHandle = GetDX12Device()->CacheSampler(&pResult->GetDX12SamplerState().GetSamplerDesc());
		pResult->GetDX12SamplerState().SetDescriptorHandle(descriptorHandle);

		if (INVALID_CPU_DESCRIPTOR_HANDLE == descriptorHandle)
			SAFE_RELEASE(pResult);

		*ppSamplerState = pResult;
	}

	return *ppSamplerState ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateQuery(
  _In_ const D3D11_QUERY_DESC* pQueryDesc,
  _Out_opt_ ID3D11Query** ppQuery)
{
	*ppQuery = CCryDX12Query::Create(GetD3D12Device(), pQueryDesc);
	return *ppQuery ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreatePredicate(
  _In_ const D3D11_QUERY_DESC* pPredicateDesc,
  _Out_opt_ ID3D11Predicate** ppPredicate)
{
	DX12_FUNC_LOG
	return -1;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateCounter(
  _In_ const D3D11_COUNTER_DESC* pCounterDesc,
  _Out_opt_ ID3D11Counter** ppCounter)
{
	DX12_FUNC_LOG
	return -1;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateDeferredContext(
  UINT ContextFlags,
  _Out_opt_ ID3D11DeviceContext** ppDeferredContext)
{
	DX12_FUNC_LOG

	if (ppDeferredContext)
	{
		*ppDeferredContext = new CCryDX12DeviceContext(this, 0, true);
		return S_OK;
	}

	return -1;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::OpenSharedResource(
  _In_ HANDLE hResource,
  _In_ REFIID ReturnedInterface,
  _Out_opt_ void** ppResource)
{
	DX12_FUNC_LOG
	return -1;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CheckFormatSupport(
  _In_ DXGI_FORMAT Format,
  _Out_ UINT* pFormatSupport)
{
	DX12_FUNC_LOG

	D3D12_FEATURE_DATA_FORMAT_SUPPORT data;
	data.Format = Format;

	if (S_OK != m_pDevice->GetD3D12Device()->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &data, sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)))
		return S_FALSE;

	*pFormatSupport = data.Support1;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CheckMultisampleQualityLevels(
  _In_ DXGI_FORMAT Format,
  _In_ UINT SampleCount,
  _Out_ UINT* pNumQualityLevels)
{
	DX12_FUNC_LOG

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS data;
	data.Format = Format;
	data.SampleCount = SampleCount;
	data.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

	if (S_OK != m_pDevice->GetD3D12Device()->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &data, sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS)))
		return S_FALSE;

	*pNumQualityLevels = data.NumQualityLevels;
	return S_OK;
}

void STDMETHODCALLTYPE CCryDX12Device::CheckCounterInfo(
  _Out_ D3D11_COUNTER_INFO* pCounterInfo)
{
	DX12_FUNC_LOG

}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CheckCounter(
  _In_ const D3D11_COUNTER_DESC* pDesc,
  _Out_ D3D11_COUNTER_TYPE* pType,
  _Out_ UINT* pActiveCounters,
  _Out_writes_opt_(*pNameLength)  LPSTR szName,
  _Inout_opt_ UINT* pNameLength,
  _Out_writes_opt_(*pUnitsLength)  LPSTR szUnits,
  _Inout_opt_ UINT* pUnitsLength,
  _Out_writes_opt_(*pDescriptionLength)  LPSTR szDescription,
  _Inout_opt_ UINT* pDescriptionLength)
{
	DX12_FUNC_LOG
	return -1;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CheckFeatureSupport(
  D3D11_FEATURE Feature,
  _Out_writes_bytes_(FeatureSupportDataSize)  void* pFeatureSupportData,
  UINT FeatureSupportDataSize)
{
	DX12_FUNC_LOG
	return -1;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::GetPrivateData(
  _In_ REFGUID guid,
  _Inout_ UINT* pDataSize,
  _Out_writes_bytes_opt_(*pDataSize)  void* pData)
{
	DX12_FUNC_LOG
	return -1;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::SetPrivateData(
  _In_ REFGUID guid,
  _In_ UINT DataSize,
  _In_reads_bytes_opt_(DataSize)  const void* pData)
{
	DX12_FUNC_LOG
	return -1;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::SetPrivateDataInterface(
  _In_ REFGUID guid,
  _In_opt_ const IUnknown* pData)
{
	DX12_FUNC_LOG
	return -1;
}

D3D_FEATURE_LEVEL STDMETHODCALLTYPE CCryDX12Device::GetFeatureLevel()
{
	DX12_FUNC_LOG
	return D3D_FEATURE_LEVEL_11_1;
}

UINT STDMETHODCALLTYPE CCryDX12Device::GetCreationFlags()
{
	DX12_FUNC_LOG
	return 0;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::GetDeviceRemovedReason()
{
	DX12_FUNC_LOG
	return -1;
}

void STDMETHODCALLTYPE CCryDX12Device::GetImmediateContext(
  _Out_ ID3D11DeviceContext** ppImmediateContext)
{
	DX12_FUNC_LOG
	if (ppImmediateContext)
	{
		*ppImmediateContext = GetDeviceContext();
		(*ppImmediateContext)->AddRef();
	}
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::SetExceptionMode(
  UINT RaiseFlags)
{
	DX12_FUNC_LOG
	return -1;
}

UINT STDMETHODCALLTYPE CCryDX12Device::GetExceptionMode()
{
	DX12_FUNC_LOG
	return 0;
}

#pragma endregion

#pragma region /* ID3D11Device1 implementation */

void STDMETHODCALLTYPE CCryDX12Device::GetImmediateContext1(
  _Out_ ID3D11DeviceContext1** ppImmediateContext)
{
	DX12_FUNC_LOG

	if (ppImmediateContext)
	{
		*ppImmediateContext = GetDeviceContext();
		(*ppImmediateContext)->AddRef();
	}
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateDeferredContext1(
  UINT ContextFlags,
  /* [annotation] */
  _COM_Outptr_opt_ ID3D11DeviceContext1** ppDeferredContext)
{
	DX12_FUNC_LOG

	if (ppDeferredContext)
	{
		*ppDeferredContext = new CCryDX12DeviceContext(this, 0, true);
		return S_OK;
	}

	return -1;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateBlendState1(
  /* [annotation] */
  _In_ const D3D11_BLEND_DESC1* pBlendStateDesc,
  /* [annotation] */
  _COM_Outptr_opt_ ID3D11BlendState1** ppBlendState)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
	return E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateRasterizerState1(
  /* [annotation] */
  _In_ const D3D11_RASTERIZER_DESC1* pRasterizerDesc,
  /* [annotation] */
  _COM_Outptr_opt_ ID3D11RasterizerState1** ppRasterizerState)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
	return E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateDeviceContextState(
  UINT Flags,
  /* [annotation] */
  _In_reads_(FeatureLevels)  const D3D_FEATURE_LEVEL* pFeatureLevels,
  UINT FeatureLevels,
  UINT SDKVersion,
  REFIID EmulatedInterface,
  /* [annotation] */
  _Out_opt_ D3D_FEATURE_LEVEL* pChosenFeatureLevel,
  /* [annotation] */
  _Out_opt_ ID3DDeviceContextState** ppContextState)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
	return E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::OpenSharedResource1(
  /* [annotation] */
  _In_ HANDLE hResource,
  /* [annotation] */
  _In_ REFIID returnedInterface,
  /* [annotation] */
  _COM_Outptr_ void** ppResource)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
	return E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::OpenSharedResourceByName(
  /* [annotation] */
  _In_ LPCWSTR lpName,
  /* [annotation] */
  _In_ DWORD dwDesiredAccess,
  /* [annotation] */
  _In_ REFIID returnedInterface,
  /* [annotation] */
  _COM_Outptr_ void** ppResource)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
	return E_FAIL;
}

#pragma endregion

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateTarget1D(
  _In_ const D3D11_TEXTURE1D_DESC* pDesc,
  _In_ const FLOAT cClearValue[4],
  _In_reads_opt_(_Inexpressible_(pDesc->MipLevels * pDesc->ArraySize)) const D3D11_SUBRESOURCE_DATA* pInitialData,
  _Out_opt_ ID3D11Texture1D** ppTexture1D)
{
	DX12_FUNC_LOG
	* ppTexture1D = CCryDX12Texture1D::Create(this, cClearValue, pDesc, pInitialData);
	return *ppTexture1D ? S_OK : S_FALSE;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateTarget2D(
  _In_ const D3D11_TEXTURE2D_DESC* pDesc,
  _In_ const FLOAT cClearValue[4],
  _In_reads_opt_(_Inexpressible_(pDesc->MipLevels * pDesc->ArraySize)) const D3D11_SUBRESOURCE_DATA* pInitialData,
  _Out_opt_ ID3D11Texture2D** ppTexture2D)
{
	DX12_FUNC_LOG
	* ppTexture2D = CCryDX12Texture2D::Create(this, cClearValue, pDesc, pInitialData);
	return *ppTexture2D ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateTarget3D(
  _In_ const D3D11_TEXTURE3D_DESC* pDesc,
  _In_ const FLOAT cClearValue[4],
  _In_reads_opt_(_Inexpressible_(pDesc->MipLevels))  const D3D11_SUBRESOURCE_DATA* pInitialData,
  _Out_opt_ ID3D11Texture3D** ppTexture3D)
{
	DX12_FUNC_LOG
	* ppTexture3D = CCryDX12Texture3D::Create(this, cClearValue, pDesc, pInitialData);
	return *ppTexture3D ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateNullResource(
  _In_ D3D11_RESOURCE_DIMENSION eType,
  _Out_opt_ ID3D11Resource** ppNullResource)
{
	DX12_FUNC_LOG
	switch (eType)
	{
	case D3D11_RESOURCE_DIMENSION_BUFFER:
		*ppNullResource = CCryDX12Buffer::Create(this);
		break;
	case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
		*ppNullResource = CCryDX12Texture1D::Create(this);
		break;
	case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
		*ppNullResource = CCryDX12Texture2D::Create(this);
		break;
	case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
		*ppNullResource = CCryDX12Texture3D::Create(this);
		break;
	default:
		*ppNullResource = nullptr;
		break;
	}
	return *ppNullResource ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::ReleaseNullResource(
  _In_ ID3D11Resource* pNullResource)
{
	pNullResource->Release();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::CreateStagingResource(
  _In_ ID3D11Resource* pInputResource,
  _Out_opt_ ID3D11Resource** ppStagingResource,
  _In_ BOOL Upload)
{
	ICryDX12Resource* dx12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pInputResource);
	NCryDX12::CResource& rResource = dx12Resource->GetDX12Resource();
	ID3D12Resource* d3d12Resource = rResource.GetD3D12Resource();

	D3D12_RESOURCE_STATES initialState = Upload ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST;
	ID3D12Resource* stagingResource = nullptr;
	HRESULT result = GetDX12Device()->CreateOrReuseStagingResource(d3d12Resource, &stagingResource, Upload);

	if (result == S_OK && stagingResource != nullptr)
	{
		*ppStagingResource = CCryDX12Buffer::Create(this, stagingResource, initialState);
		stagingResource->Release();

		return S_OK;
	}

	return result;
}

HRESULT STDMETHODCALLTYPE CCryDX12Device::ReleaseStagingResource(
  _In_ ID3D11Resource* pStagingResource)
{
	ICryDX12Resource* dx12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pStagingResource);
	NCryDX12::CResource& rResource = dx12Resource->GetDX12Resource();
	ID3D12Resource* d3d12Resource = rResource.GetD3D12Resource();

	GetDX12Device()->ReleaseLater(rResource.GetFenceValues(CMDTYPE_ANY), d3d12Resource);

	pStagingResource->Release();
	return S_OK;
}
