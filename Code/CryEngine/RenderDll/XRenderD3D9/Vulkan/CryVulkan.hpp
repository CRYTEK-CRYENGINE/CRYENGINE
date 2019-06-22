// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if !_RELEASE
	#define VK_ERROR(...) \
		do { CryLog("Vulkan Error: ", __VA_ARGS__); } while (false)
	#define VK_ASSERT(cond, ...) \
		do { if (!(cond)) { VK_ERROR(__VA_ARGS__); CRY_ASSERT_MESSAGE(false, __VA_ARGS__); } } while (false)
#else
	#define VK_ERROR(...)        ((void)0)
	#define VK_ASSERT(cond, ...) (0 ? (void)(cond) : (void)0)
#endif

#ifdef _DEBUG
	#define VK_LOG(cond, ...) \
		do { if (cond) CryLog("Vulkan Log: ", __VA_ARGS__); } while (false)
	#define VK_WARNING(...) \
		do { CryLog("Vulkan Warning: ", __VA_ARGS__); } while (false)
	#define VK_ASSERT_DEBUG(cond, ...) VK_ASSERT(cond, __VA_ARGS__)
#else
	#define VK_LOG(cond, ...)          ((void)0)
	#define VK_WARNING(...)            ((void)0)
	#define VK_ASSERT_DEBUG(cond, ...) ((void)0)
#endif

#define VK_NOT_IMPLEMENTED VK_ASSERT(false, "Not implemented!");
#define VK_FUNC_LOG() VK_LOG(true, "%s() called", __FUNC__)

namespace Cry
{
	template<typename T, typename... Args>
	inline T const& VulkanVerify(T const& cond, Args&&... args)
	{
		VK_ASSERT(cond, std::forward<Args>(args)...);
		return cond;
	}
}

#define CRY_VULKAN_VERIFY(cond, ...) Cry::VulkanVerify(cond, __VA_ARGS__)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class ID3D11DeviceChildDerivative>
inline void ClearDebugName(ID3D11DeviceChildDerivative* pWrappedResource)
{
#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
	if (!pWrappedResource)
		return;

	return pWrappedResource->ClearDebugName();
#endif
}

template<class ID3D11DeviceChildDerivative>
inline void SetDebugName(ID3D11DeviceChildDerivative* pWrappedResource, const char* name)
{
#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
	if (!pWrappedResource)
		return;

	pWrappedResource->SetDebugName(name);
#endif
}

template<class ID3D11DeviceChildDerivative>
inline std::string GetDebugName(ID3D11DeviceChildDerivative* pWrappedResource)
{
#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
	if (!pWrappedResource)
		return "nullptr";

	return pWrappedResource->GetDebugName();
#endif

	return "";
}
