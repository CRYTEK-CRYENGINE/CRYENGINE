// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern int g_nPrintDX11;

#ifdef _DEBUG
	#define DX11_LOG(cond, ...) \
		do { if (cond || g_nPrintDX11) { CryLog("DX11 Log: " __VA_ARGS__); } } while (0)
	#define DX11_ERROR(...) \
		do { CryLog("DX11 Error: " __VA_ARGS__); } while (0)
	#define DX11_ASSERT(cond, ...) \
		do { if (!(cond)) { DX11_ERROR(__VA_ARGS__); CRY_ASSERT_MESSAGE(0, __VA_ARGS__); } } while (0)
	#define DX11_WARNING(cond, ...) \
		do { if (!(cond)) { DX11_LOG(__VA_ARGS__); } } while (0)
#else
	#define DX11_LOG(cond, ...) do {} while (0)
	#define DX11_ERROR(...)     do {} while (0)
	#define DX11_ASSERT(cond, ...)
	#define DX11_WARNING(cond, ...)
#endif

#define DX11_NOT_IMPLEMENTED DX11_ASSERT(0, "Not implemented!");

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
