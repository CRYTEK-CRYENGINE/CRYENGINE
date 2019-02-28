// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern int g_nPrintDX12;

#ifdef _DEBUG
	#define DX12_LOG(cond, ...) \
		do { if (cond || g_nPrintDX12) { CryLog("DX12 Log: " __VA_ARGS__); } } while (0)
	#define DX12_ERROR(...) \
		do { CryLog("DX12 Error: " __VA_ARGS__); } while (0)
	#define DX12_ASSERT(cond, ...) \
		do { if (!(cond)) { DX12_ERROR(__VA_ARGS__); CRY_ASSERT_MESSAGE(0, __VA_ARGS__); } } while (0)
	#define DX12_WARNING(cond, ...) \
		do { if (!(cond)) { DX12_LOG(__VA_ARGS__); } } while (0)
#else
	#define DX12_LOG(cond, ...) do {} while (0)
	#define DX12_ERROR(...)     do {} while (0)
	#define DX12_ASSERT(cond, ...)
	#define DX12_WARNING(cond, ...)
#endif

#define DX12_NOT_IMPLEMENTED DX12_ASSERT(0, "Not implemented!");

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
