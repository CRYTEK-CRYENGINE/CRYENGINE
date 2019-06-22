// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern int g_nPrintDX12;

#if !_RELEASE
	#define DX12_ERROR(...) \
		do { CryLog("DX12 Error: " __VA_ARGS__); } while (false)
	#define DX12_ASSERT(cond, ...) \
		do { if (!(cond)) { DX12_ERROR(#cond); CRY_ASSERT_MESSAGE(false, __VA_ARGS__); } } while (false)
#else
	#define DX12_ERROR(...)        ((void)0)
	#define DX12_ASSERT(cond, ...) ((void)0)
#endif

#ifdef _DEBUG
	#define DX12_LOG(cond, ...) \
		do { if (cond || g_nPrintDX12) { CryLog("DX12 Log: " __VA_ARGS__); } } while (false)
	#define DX12_WARNING(cond, ...) \
		do { if (!(cond)) { DX12_LOG(__VA_ARGS__); } } while (false)
	#define DX12_ASSERT_DEBUG(cond, ...) DX12_ASSERT(cond, __VA_ARGS__)
#else
	#define DX12_LOG(cond, ...)          ((void)0)
	#define DX12_WARNING(cond, ...)      ((void)0)
	#define DX12_ASSERT_DEBUG(cond, ...) ((void)0)
#endif

#define DX12_NOT_IMPLEMENTED DX12_ASSERT(false, "Not implemented!");

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "API/DX12Base.hpp"
#include "API/DX12PSO.hpp"

// DX11 emulation
#include "Misc/SCryDX11PipelineState.hpp"
#include "Device/CCryDX12Device.hpp"
#include "Device/CCryDX12DeviceContext.hpp"
#include "Device/CCryDX12DeviceChild.hpp"
#include "Resource/State/CCryDX12SamplerState.hpp"
#include "Resource/CCryDX12View.hpp"
#include "Resource/View/CCryDX12DepthStencilView.hpp"
#include "Resource/View/CCryDX12RenderTargetView.hpp"
#include "Resource/View/CCryDX12ShaderResourceView.hpp"
#include "Resource/View/CCryDX12UnorderedAccessView.hpp"
#include "Resource/Misc/CCryDX12Buffer.hpp"
#include "Resource/Texture/CCryDX12Texture1D.hpp"
#include "Resource/Texture/CCryDX12Texture2D.hpp"
#include "Resource/Texture/CCryDX12Texture3D.hpp"
#include "Resource/CCryDX12Asynchronous.hpp"
#include "Resource/Misc/CCryDX12Query.hpp"
#include "Resource/Misc/CCryDX12InputLayout.hpp"

// DXGI emulation
#include "GI/CCryDX12GIOutput.hpp"
#include "GI/CCryDX12GIAdapter.hpp"
#include "GI/CCryDX12SwapChain.hpp"
#include "GI/CCryDX12GIFactory.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT WINAPI D3DReflectDXILorDXBC(
	_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
	_In_ SIZE_T SrcDataSize,
	_In_ REFIID pInterface,
	_Out_ void** ppReflector);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT WINAPI DX12CreateDXGIFactory1(REFIID riid, void** ppFactory);

HRESULT WINAPI DX12CreateDevice(
  IDXGIAdapter* pAdapter,
  D3D_DRIVER_TYPE DriverType,
  HMODULE Software,
  UINT Flags,
  CONST D3D_FEATURE_LEVEL* pFeatureLevels,
  UINT FeatureLevels,
  UINT SDKVersion,
  ID3D11Device** ppDevice,
  D3D_FEATURE_LEVEL* pFeatureLevel,
  ID3D11DeviceContext** ppImmediateContext);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class ID3D11DeviceChildDerivative>
inline void ClearDebugName(ID3D11DeviceChildDerivative* pWrappedResource)
{
#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
	if (!pWrappedResource)
		return;

	pWrappedResource->SetPrivateData(WKPDID_D3DDebugObjectName, 0, nullptr);
#endif
}

template<class ID3D11DeviceChildDerivative>
inline void SetDebugName(ID3D11DeviceChildDerivative* pWrappedResource, const char* name)
{
#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
	if (!pWrappedResource)
		return;

	pWrappedResource->SetPrivateData(WKPDID_D3DDebugObjectName, 0, nullptr);
	pWrappedResource->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(name)+1, name);
#endif
}

template<class ID3D11DeviceChildDerivative>
inline std::string GetDebugName(ID3D11DeviceChildDerivative* pWrappedResource)
{
#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
	if (!pWrappedResource)
		return "nullptr";

	UINT length = 512;
	do
	{
		char* buffer = (char*)_alloca(length);
		HRESULT hr = pWrappedResource->GetPrivateData(WKPDID_D3DDebugObjectName, &length, buffer);
		if (hr == S_OK)
			return buffer;
		if (hr != D3D12_MESSAGE_ID_GETPRIVATEDATA_MOREDATA)
			return "failure";

		length += 512;
	} while (true);
#endif

	return "";
}
