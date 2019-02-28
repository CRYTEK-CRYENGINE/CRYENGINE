// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// Pull in everything needed to implement device objects
#include "API/VKBase.hpp"
#include "API/VKDevice.hpp"
#include "API/VKShader.hpp"
#include "D3DVKConversionUtility.hpp"

// DXGI emulation
#include "CryVulkanWrappers/Resources/CCryVKShaderReflection.hpp"
#include "CryVulkanWrappers/GI/CCryVKGIAdapter.hpp"
#include "CryVulkanWrappers/GI/CCryVKGIFactory.hpp"
#include "CryVulkanWrappers/GI/CCryVKGIOutput.hpp"
#include "CryVulkanWrappers/GI/CCryVKSwapChain.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT WINAPI D3DCreateBlob(SIZE_T NumBytes, ID3DBlob** ppBuffer);

HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void** ppFactory);

HRESULT WINAPI VKCreateDevice(
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

#if defined(USE_SDL2_VIDEO)
bool VKCreateSDLWindow(const char* szTitle, uint32 uWidth, uint32 uHeight, bool bFullScreen, HWND* pHandle);
void VKDestroySDLWindow(HWND kHandle);
#endif
