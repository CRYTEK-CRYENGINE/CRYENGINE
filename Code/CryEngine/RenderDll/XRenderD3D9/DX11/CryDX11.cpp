// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "CryDX11.hpp"

#if USE_AMD_API
	#include AMD_API_HEADER
#endif

D3D11_MAP D3D11_MAP_WRITE_NO_OVERWRITE_OPTIONAL[3] = {
	D3D11_MAP_WRITE_DISCARD,
	D3D11_MAP_WRITE_DISCARD,
	D3D11_MAP_WRITE_DISCARD
};

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
  ID3D11DeviceContext** ppImmediateContext)
{
#if USE_AMD_API
	extern AGSContext* g_pAGSContext;
	extern AGSConfiguration g_pAGSContextConfiguration;
	extern AGSGPUInfo g_gpuInfo;
	extern unsigned int g_extensionsSupported;

	AGSReturnCode status = g_pAGSContext ? AGS_SUCCESS : agsInit(&g_pAGSContext, &g_pAGSContextConfiguration, &g_gpuInfo);
	if (status == AGS_SUCCESS)
	{
		AGSDX11ReturnedParams returnedParams;
		AGSDX11ExtensionParams extensionParams =
		{
			7, // UAV slot for intrinsics
			L"APPNAME", 0,
			L"CRYENGINE", 0,
		};
		AGSDX11DeviceCreationParams creationParams =
		{
			pAdapter,
			DriverType,
			Software,
			Flags,
			pFeatureLevels,
			FeatureLevels,
			SDKVersion,
			nullptr //&m_swapChainDesc
		};

		if (agsDriverExtensionsDX11_CreateDevice(
			g_pAGSContext,
			&creationParams,
			&extensionParams,
			&returnedParams
		) == AGS_SUCCESS)
		{
			//*m_pSwapChain = returnedParams.pSwapChain;
			*ppDevice = returnedParams.pDevice;
			*pFeatureLevel = returnedParams.FeatureLevel;
			*ppImmediateContext = returnedParams.pImmediateContext;

			g_extensionsSupported = returnedParams.extensionsSupported;

			return S_OK;
		}
	}
#endif

	typedef HRESULT (WINAPI * FP_D3D11CreateDevice)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, CONST D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
	FP_D3D11CreateDevice pD3D11CD = (FP_D3D11CreateDevice)GetProcAddress(LoadLibraryA("d3d11.dll"), "D3D11CreateDevice");

	return pD3D11CD(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
}
#endif
