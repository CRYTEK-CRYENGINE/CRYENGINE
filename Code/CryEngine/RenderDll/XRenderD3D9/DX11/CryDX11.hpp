// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern int g_nPrintDX11;

#if !_RELEASE
	#define DX11_ERROR(...) \
		do { CryLog("DX11 Error: ", __VA_ARGS__); } while (false)
	#define DX11_ASSERT(cond, ...) \
		do { if (!(cond)) { DX11_ERROR(__VA_ARGS__); CRY_ASSERT_MESSAGE(false, __VA_ARGS__); } } while (false)
#else
	#define DX11_ERROR(...)        ((void)0)
	#define DX11_ASSERT(cond, ...) ((void)0)
#endif

#ifdef _DEBUG
	#define DX11_LOG(cond, ...) \
		do { if (cond || g_nPrintDX11) { CryLog("DX11 Log: ", __VA_ARGS__); } } while (false)
	#define DX11_WARNING(cond, ...) \
		do { if (!(cond)) { DX11_LOG(__VA_ARGS__); } } while (false)
	#define DX11_ASSERT_DEBUG(cond, ...) DX11_ASSERT(cond, __VA_ARGS__)
#else
	#define DX11_LOG(cond, ...)          ((void)0)
	#define DX11_WARNING(cond, ...)      ((void)0)
	#define DX11_ASSERT_DEBUG(cond, ...) ((void)0)
#endif

#define DX11_NOT_IMPLEMENTED DX11_ASSERT(false, "Not implemented!");

namespace Cry
{
	template<typename T, typename... Args>
	inline T const& DX11Verify(T const& cond, Args&&... args)
	{
		DX11_ASSERT(cond, std::forward<Args>(args)...);
		return cond;
	}
}

#define CRY_DX11_VERIFY(cond, ...) Cry::DX11Verify(cond, __VA_ARGS__)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "API/DX11Base.hpp"

#if CRY_PLATFORM_WINDOWS
HRESULT WINAPI DX11CreateDevice(
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
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline void ClearDebugName(ID3D11DeviceChild* pNativeResource)
{
#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
	if (!pNativeResource)
		return;

	pNativeResource->SetPrivateData(WKPDID_D3DDebugObjectName, 0, nullptr);
#endif
}

inline void SetDebugName(ID3D11DeviceChild* pNativeResource, const char* name)
{
#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
	if (!pNativeResource)
		return;

	pNativeResource->SetPrivateData(WKPDID_D3DDebugObjectName, 0, nullptr);
	pNativeResource->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(name)+1, name);
#endif
}

inline std::string GetDebugName(ID3D11DeviceChild* pNativeResource)
{
#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
	if (!pNativeResource)
		return "nullptr";

	UINT length = 512;
	do
	{
		char* buffer = (char*)_alloca(length);
		HRESULT hr = pNativeResource->GetPrivateData(WKPDID_D3DDebugObjectName, &length, buffer);
		if (hr == S_OK)
			return buffer;
		if (hr != D3D11_MESSAGE_ID_GETPRIVATEDATA_MOREDATA)
			return "failure";
		
		length += 512;
	}
	while (true);
#endif

	return "";
}
