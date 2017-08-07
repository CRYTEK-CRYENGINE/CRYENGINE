// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#if defined(SUPPORT_DEVICE_INFO)

#include <CryCore/CryCustomTypes.h>

enum
{
#if (CRY_RENDERER_DIRECT3D >= 120)
	DefaultBufferCount = 4
#else
	DefaultBufferCount = 2
#endif
};

static void FillSwapChainDesc(DXGI_SWAP_CHAIN_DESC& desc, UINT width, UINT height, HWND hWnd, BOOL windowed)
{
	const bool bWaitable = (CRenderer::CV_r_D3D12WaitableSwapChain && windowed);

	desc.BufferDesc.Width = width;
	desc.BufferDesc.Height = height;
	desc.BufferDesc.RefreshRate.Numerator = 0;
	desc.BufferDesc.RefreshRate.Denominator = 0;
#if defined(USE_SDL2_VIDEO)
	desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
#else
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
#endif
	desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.BufferUsage = DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = (CRenderer::CV_r_minimizeLatency > 0 || bWaitable) ? MAX_FRAMES_IN_FLIGHT : DefaultBufferCount;
	desc.OutputWindow = hWnd;
	desc.Windowed = windowed ? 1 : 0;
	desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

#if (CRY_RENDERER_DIRECT3D >= 120)
	if (bWaitable)
	{
		// Set this flag to create a waitable object you can use to ensure rendering does not begin while a
		// frame is still being presented. When this flag is used, the swapchain's latency must be set with
		// the IDXGISwapChain2::SetMaximumFrameLatency API instead of IDXGIDevice1::SetMaximumFrameLatency.
		desc.BufferCount = MAX_FRAMES_IN_FLIGHT + 1;
		desc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
	}
#endif
}

DeviceInfo::DeviceInfo()
	: m_pFactory(0)
	, m_pAdapter(0)
	, m_pOutput(0)
	, m_pDevice(0)
	, m_pContext(0)
	, m_pSwapChain(0)
	, m_driverType(D3D_DRIVER_TYPE_NULL)
	, m_creationFlags(0)
	, m_featureLevel(D3D_FEATURE_LEVEL_9_1)
	, m_autoDepthStencilFmt(DXGI_FORMAT_R24G8_TYPELESS)
	, m_outputIndex(0)
	, m_syncInterval(0)
	, m_presentFlags(0)
	, m_activated(true)
	, m_activatedMT(true)
#if defined(SUPPORT_DEVICE_INFO_MSG_PROCESSING)
	, m_msgQueueLock()
	, m_msgQueue()
#endif
{
	memset(&m_adapterDesc, 0, sizeof(m_adapterDesc));
	memset(&m_swapChainDesc, 0, sizeof(m_swapChainDesc));
	memset(&m_refreshRate, 0, sizeof(m_refreshRate));
	memset(&m_desktopRefreshRate, 0, sizeof(m_desktopRefreshRate));

#if !CRY_RENDERER_OPENGL && !CRY_RENDERER_VULKAN && !CRY_RENDERER_GNM
#if CRY_RENDERER_DIRECT3D >= 120
	ZeroStruct(m_D3D120aOptions);
	ZeroStruct(m_D3D120bOptions);
	#if CRY_RENDERER_DIRECT3D >= 121
	ZeroStruct(m_D3D120cOptions);
	#endif
#elif CRY_RENDERER_DIRECT3D >= 110
	ZeroStruct(m_D3D110aOptions);
	#if CRY_RENDERER_DIRECT3D >= 112
	ZeroStruct(m_D3D112aOptions);
	#if CRY_RENDERER_DIRECT3D >= 113
	ZeroStruct(m_D3D113aOptions);
	#if CRY_RENDERER_DIRECT3D >= 114
	ZeroStruct(m_D3D114aOptions);
	ZeroStruct(m_D3D114bOptions);
	#endif
	#endif
	#endif
#endif
#endif
}

void DeviceInfo::Release()
{
	memset(&m_adapterDesc, 0, sizeof(m_adapterDesc));
	memset(&m_swapChainDesc, 0, sizeof(m_swapChainDesc));
	memset(&m_refreshRate, 0, sizeof(m_refreshRate));
	memset(&m_desktopRefreshRate, 0, sizeof(m_desktopRefreshRate));

	if (m_pSwapChain)
		m_pSwapChain->SetFullscreenState(FALSE, 0);
	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pContext);
	SAFE_RELEASE(m_pDevice);
	SAFE_RELEASE(m_pOutput);
	SAFE_RELEASE(m_pAdapter);
	SAFE_RELEASE(m_pFactory);
}

static void SetupPreferredMonitorDimensions(HMONITOR hMonitor)
{
#if CRY_PLATFORM_WINDOWS
	MONITORINFO monitorInfo;
	monitorInfo.cbSize = sizeof(monitorInfo);
	if (GetMonitorInfo(hMonitor, &monitorInfo))
	{
		gcpRendD3D->m_prefMonX = monitorInfo.rcMonitor.left;
		gcpRendD3D->m_prefMonY = monitorInfo.rcMonitor.top;
		gcpRendD3D->m_prefMonWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
		gcpRendD3D->m_prefMonHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
	}
#endif
}

static void SetupMonitorAndGetAdapterDesc(DXGIOutput* pOutput, DXGIAdapter* pAdapter, DXGI_ADAPTER_DESC1& adapterDesc)
{
	DXGI_OUTPUT_DESC outputDesc;
	if (SUCCEEDED(pOutput->GetDesc(&outputDesc)))
		SetupPreferredMonitorDimensions(outputDesc.Monitor);
	pAdapter->GetDesc1(&adapterDesc);
}

static int GetDXGIAdapterOverride()
{
#if CRY_PLATFORM_WINDOWS
	ICVar* pCVar = gEnv->pConsole ? gEnv->pConsole->GetCVar("r_overrideDXGIAdapter") : 0;
	return pCVar ? pCVar->GetIVal() : -1;
#else
	return -1;
#endif
}

static void ProcessWindowMessages(HWND hWnd)
{
#if CRY_PLATFORM_WINDOWS
	iSystem->PumpWindowMessage(true, hWnd);
#endif
}

static int GetMultithreaded()
{
	ICVar* pCVar = gEnv->pConsole ? gEnv->pConsole->GetCVar("r_multithreaded") : 0;
	return pCVar ? pCVar->GetIVal() : -1;
}

#if !defined(_RELEASE)
bool GetForcedFeatureLevel(D3D_FEATURE_LEVEL* pForcedFeatureLevel)
{
	struct FeatureLevel
	{
		const char*       m_szName;
		D3D_FEATURE_LEVEL m_eValue;
	};
	static FeatureLevel s_aLevels[] =
	{
		{ "10.0", D3D_FEATURE_LEVEL_10_0 },
		{ "10.1", D3D_FEATURE_LEVEL_10_1 },
		{ "11.0", D3D_FEATURE_LEVEL_11_0 },
		{ "11.1", D3D_FEATURE_LEVEL_11_1 },
		{ "12.0", D3D_FEATURE_LEVEL_12_0 },
		{ "12.1", D3D_FEATURE_LEVEL_12_1 }
	};

	const char* szForcedName(gcpRendD3D->CV_d3d11_forcedFeatureLevel->GetString());
	if (szForcedName == NULL || strlen(szForcedName) == 0)
		return false;

	FeatureLevel* pLevel;
	for (pLevel = s_aLevels; pLevel < s_aLevels + CRY_ARRAY_COUNT(s_aLevels); ++pLevel)
	{
		if (strcmp(pLevel->m_szName, szForcedName) == 0)
		{
			*pForcedFeatureLevel = pLevel->m_eValue;
			return true;
		}
	}

	CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, "Invalid value for d3d11_forcedFeatureLevel %s - using available feature level", szForcedName);
	return false;
}
#endif // !defined(_RELEASE)

bool DeviceInfo::CreateDevice(bool windowed, int width, int height, int backbufferWidth, int backbufferHeight, int zbpp, OnCreateDeviceCallback pCreateDeviceCallback, CreateWindowCallback pCreateWindowCallback)
{
#if CRY_RENDERER_VULKAN
	m_autoDepthStencilFmt = zbpp == 32 ? DXGI_FORMAT_R32G8X24_TYPELESS : zbpp == 24 ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_R16G8X8_TYPELESS;
#else
	m_autoDepthStencilFmt = zbpp == 32 ? DXGI_FORMAT_R32G8X24_TYPELESS : DXGI_FORMAT_R24G8_TYPELESS;
#endif

#if CRY_RENDERER_OPENGL || CRY_RENDERER_OPENGLES

	HWND hWnd = pCreateWindowCallback ? pCreateWindowCallback() : 0;
	if (!hWnd)
	{
		Release();
		return false;
	}

	const int r_overrideDXGIAdapter = GetDXGIAdapterOverride();
	const int r_multithreaded = GetMultithreaded();
	unsigned int nAdapterOrdinal = r_overrideDXGIAdapter >= 0 ? r_overrideDXGIAdapter : 0;
	FillSwapChainDesc(m_swapChainDesc, backbufferWidth, backbufferHeight, hWnd, windowed);

	if (!SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&m_pFactory)) || !m_pFactory)
	{
		Release();
		return false;
	}

	while (m_pFactory->EnumAdapters1(nAdapterOrdinal, &m_pAdapter) != DXGI_ERROR_NOT_FOUND)
	{
		if (m_pAdapter)
		{
			m_driverType = D3D_DRIVER_TYPE_HARDWARE;
			m_creationFlags = 0;

			D3D_FEATURE_LEVEL* aFeatureLevels(NULL);
			unsigned int uNumFeatureLevels(0);
#if !defined(_RELEASE)
			D3D_FEATURE_LEVEL eForcedFeatureLevel(CRY_RENDERER_DIRECT3D_FL);
			if (GetForcedFeatureLevel(&eForcedFeatureLevel))
			{
				aFeatureLevels = &eForcedFeatureLevel;
				uNumFeatureLevels = 1;
			}
#endif //!defined(_RELEASE)

			const D3D_DRIVER_TYPE driverType = m_driverType == D3D_DRIVER_TYPE_HARDWARE ? D3D_DRIVER_TYPE_UNKNOWN : m_driverType;
			HRESULT hr = D3D11CreateDeviceAndSwapChain(m_pAdapter, driverType, 0, m_creationFlags, aFeatureLevels, uNumFeatureLevels, D3D11_SDK_VERSION, &m_swapChainDesc, &m_pSwapChain, &m_pDevice, &m_featureLevel, &m_pContext);
			if (SUCCEEDED(hr) && m_pDevice && m_pSwapChain)
			{
#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
				const uint32 outputIdx = gRenDev->CV_r_overrideDXGIOutput > 0 ? gRenDev->CV_r_overrideDXGIOutput : 0;
				if (outputIdx)
				{
					if (SUCCEEDED(m_pAdapter->EnumOutputs(outputIdx, &m_pOutput)) && m_pOutput)
					{
						SetupMonitorAndGetAdapterDesc(m_pOutput, m_pAdapter, m_adapterDesc);
						break;
					}

					SAFE_RELEASE(m_pOutput);
					CryLogAlways("Failed to resolve DXGI display for override index %d. Falling back to primary display.", outputIdx);
				}
#endif
				if (SUCCEEDED(m_pAdapter->EnumOutputs(0, &m_pOutput)) && m_pOutput)
				{
					SetupMonitorAndGetAdapterDesc(m_pOutput, m_pAdapter, m_adapterDesc);
					break;
				}
				else if (r_overrideDXGIAdapter >= 0)
					CryLogAlways("No display connected to DXGI adapter override %d. Adapter cannot be used for rendering.", r_overrideDXGIAdapter);
			}

			SAFE_RELEASE(m_pOutput);
			SAFE_RELEASE(m_pContext);
			SAFE_RELEASE(m_pDevice);
			SAFE_RELEASE(m_pSwapChain);
			SAFE_RELEASE(m_pAdapter);
		}
	}

	if (!m_pDevice || !m_pSwapChain)
	{
		Release();
		return false;
	}

	{
		m_pFactory->MakeWindowAssociation(m_swapChainDesc.OutputWindow, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);

		if (pCreateDeviceCallback)
			pCreateDeviceCallback(m_pDevice);
	}

	#if !DXGL_FULL_EMULATION
		#if OGL_SINGLE_CONTEXT
	DXGLBindDeviceContext(m_pContext);
		#else
	if (r_multithreaded)
		DXGLReserveContext(m_pDevice);
	DXGLBindDeviceContext(m_pContext, !r_multithreaded);
		#endif
	#endif //!DXGL_FULL_EMULATION

	return IsOk();
#elif (CRY_RENDERER_VULKAN >= 10)
	HWND hWnd = pCreateWindowCallback ? pCreateWindowCallback() : 0;
	if (!hWnd)
	{
		Release();
		return false;
	}

	const int r_overrideDXGIAdapter = GetDXGIAdapterOverride();
	const int r_multithreaded = GetMultithreaded();
	unsigned int nAdapterOrdinal = r_overrideDXGIAdapter >= 0 ? r_overrideDXGIAdapter : 0;

	if (!SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&m_pFactory)) || !m_pFactory)
	{
		Release();
		return false;
	}

	while (m_pFactory->EnumAdapters1(nAdapterOrdinal, &m_pAdapter) != DXGI_ERROR_NOT_FOUND)
	{
		if (m_pAdapter)
		{
			m_driverType = D3D_DRIVER_TYPE_HARDWARE;
			m_creationFlags = 0;

			D3D_FEATURE_LEVEL* aFeatureLevels(NULL);
			unsigned int uNumFeatureLevels(0);
#if !defined(_RELEASE)
			D3D_FEATURE_LEVEL eForcedFeatureLevel(CRY_RENDERER_DIRECT3D_FL);
			if (GetForcedFeatureLevel(&eForcedFeatureLevel))
			{
				aFeatureLevels = &eForcedFeatureLevel;
				uNumFeatureLevels = 1;
			}
#endif //!defined(_RELEASE)

			const D3D_DRIVER_TYPE driverType = m_driverType == D3D_DRIVER_TYPE_HARDWARE ? D3D_DRIVER_TYPE_UNKNOWN : m_driverType;
			HRESULT hr = VKCreateDevice(m_pAdapter, driverType, 0, m_creationFlags, aFeatureLevels, uNumFeatureLevels, D3D11_SDK_VERSION, hWnd, &m_pDevice, &m_featureLevel, &m_pContext);
			if (SUCCEEDED(hr) && m_pDevice)
			{
#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
				const uint32 outputIdx = gRenDev->CV_r_overrideDXGIOutput > 0 ? gRenDev->CV_r_overrideDXGIOutput : 0;
				if (outputIdx)
				{
					if (SUCCEEDED(m_pAdapter->EnumOutputs(outputIdx, &m_pOutput)) && m_pOutput)
					{
						SetupMonitorAndGetAdapterDesc(m_pOutput, m_pAdapter, m_adapterDesc);
						break;
					}

					CryLogAlways("Failed to resolve DXGI display for override index %d. Falling back to primary display.", outputIdx);
				}
#endif
				if (SUCCEEDED(m_pAdapter->EnumOutputs(0, &m_pOutput)) && m_pOutput)
				{
					SetupMonitorAndGetAdapterDesc(m_pOutput, m_pAdapter, m_adapterDesc);
					break;
				}
				else if (r_overrideDXGIAdapter >= 0)
					CryLogAlways("No display connected to DXGI adapter override %d. Adapter cannot be used for rendering.", r_overrideDXGIAdapter);
			}
		}

		if (r_overrideDXGIAdapter >= 0)
			break;

		++nAdapterOrdinal;
	}

	if (!m_pDevice || !m_pAdapter || !m_pOutput)
	{
		Release();
		return false;
	}

	{
		FillSwapChainDesc(m_swapChainDesc, backbufferWidth, backbufferHeight, hWnd, windowed);
		
		if (!windowed)
		{
			DXGI_MODE_DESC match;
			if (SUCCEEDED(m_pOutput->FindClosestMatchingMode(&m_swapChainDesc.BufferDesc, &match, m_pDevice)))
			{
				m_swapChainDesc.BufferDesc = match;
			}
		}

		m_refreshRate = !windowed ? m_swapChainDesc.BufferDesc.RefreshRate : m_desktopRefreshRate;

		HRESULT hr = m_pFactory->CreateSwapChain(m_pDevice, &m_swapChainDesc, &m_pSwapChain);

		if (FAILED(hr) || !m_pSwapChain)
		{
			Release();
			return false;
		}
	}

	{
		m_pFactory->MakeWindowAssociation(m_swapChainDesc.OutputWindow, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);

		if (pCreateDeviceCallback)
			pCreateDeviceCallback(m_pDevice);
	}

	return IsOk();
#elif CRY_PLATFORM_WINDOWS
	typedef HRESULT (WINAPI * FP_CreateDXGIFactory1)(REFIID, void**);

	FP_CreateDXGIFactory1 pCDXGIF =
	#if (CRY_RENDERER_DIRECT3D >= 120)
	  (FP_CreateDXGIFactory1) DX12CreateDXGIFactory1;
	#else
	  (FP_CreateDXGIFactory1) GetProcAddress(LoadLibraryA("dxgi.dll"), "CreateDXGIFactory1");
	#endif

	IDXGIAdapter1* pAdapter = nullptr;
	IDXGIOutput* pOutput = nullptr;
	ID3D11Device* pDevice = nullptr;
	ID3D11DeviceContext* pContext = nullptr;

	if (pCDXGIF && SUCCEEDED(pCDXGIF(__uuidof(DXGIFactory), (void**) &m_pFactory)) && m_pFactory)
	{
		typedef HRESULT (WINAPI * FP_D3D11CreateDevice)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, CONST D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

		FP_D3D11CreateDevice pD3D11CD =
	#if (CRY_RENDERER_DIRECT3D >= 120)
			(FP_D3D11CreateDevice)DX12CreateDevice;
	#else
		  (FP_D3D11CreateDevice)GetProcAddress(LoadLibraryA("d3d11.dll"), "D3D11CreateDevice");
	#endif

		if (pD3D11CD)
		{
			const int r_overrideDXGIAdapter = GetDXGIAdapterOverride();
			unsigned int nAdapterOrdinal = r_overrideDXGIAdapter >= 0 ? r_overrideDXGIAdapter : 0;

			while (m_pFactory->EnumAdapters1(nAdapterOrdinal, &pAdapter) != DXGI_ERROR_NOT_FOUND)
			{
				if (pAdapter)
				{
					// Promote interfaces to the required level
					pAdapter->QueryInterface(__uuidof(DXGIAdapter), (void**)&m_pAdapter);

	#if defined(DX11_ALLOW_D3D_DEBUG_RUNTIME)
					const unsigned int debugRTFlag = gcpRendD3D->CV_r_EnableDebugLayer ? D3D11_CREATE_DEVICE_DEBUG : 0;
	#else
					const unsigned int debugRTFlag = 0;
	#endif
					m_driverType = D3D_DRIVER_TYPE_HARDWARE;
					m_creationFlags = debugRTFlag;
	#if defined(CRY_PLATFORM_WINDOWS)
					if (gcpRendD3D->CV_d3d11_preventDriverThreading)
						m_creationFlags |= D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;
	#endif

					D3D_FEATURE_LEVEL arrFeatureLevels[] = {
						D3D_FEATURE_LEVEL_11_1,
						D3D_FEATURE_LEVEL_11_0 };

					D3D_FEATURE_LEVEL* pFeatureLevels(arrFeatureLevels);
					unsigned int uNumFeatureLevels(sizeof(arrFeatureLevels) / sizeof(D3D_FEATURE_LEVEL));
	#if !defined(_RELEASE)
					D3D_FEATURE_LEVEL eForcedFeatureLevel(CRY_RENDERER_DIRECT3D_FL);

					if (GetForcedFeatureLevel(&eForcedFeatureLevel))
					{
						pFeatureLevels = &eForcedFeatureLevel;
						uNumFeatureLevels = 1;
					}
	#endif  //!defined(_RELEASE)

					const D3D_DRIVER_TYPE driverType = m_driverType == D3D_DRIVER_TYPE_HARDWARE ? D3D_DRIVER_TYPE_UNKNOWN : m_driverType;
					HRESULT hr = pD3D11CD(pAdapter, driverType, 0, m_creationFlags, pFeatureLevels, uNumFeatureLevels, D3D11_SDK_VERSION, &pDevice, &m_featureLevel, &pContext);
					if (SUCCEEDED(hr) && pDevice && pContext)
					{
#if !CRY_RENDERER_OPENGL && !CRY_RENDERER_VULKAN && !CRY_RENDERER_GNM
	#if CRY_RENDERER_DIRECT3D >= 120
						pDevice->CheckFeatureSupport((D3D11_FEATURE)D3D12_FEATURE_D3D12_OPTIONS , &m_D3D120aOptions, sizeof(m_D3D120aOptions));
						pDevice->CheckFeatureSupport((D3D11_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &m_D3D120bOptions, sizeof(m_D3D120bOptions));
		#if CRY_RENDERER_DIRECT3D >= 121
						pDevice->CheckFeatureSupport((D3D11_FEATURE)D3D12_FEATURE_D3D12_OPTIONS2, &m_D3D120cOptions, sizeof(m_D3D120cOptions));
		#endif
	#elif CRY_RENDERER_DIRECT3D >= 110
						pDevice->CheckFeatureSupport((D3D11_FEATURE)D3D11_FEATURE_D3D11_OPTIONS , &m_D3D110aOptions, sizeof(m_D3D110aOptions));
		#if CRY_RENDERER_DIRECT3D >= 112
						pDevice->CheckFeatureSupport((D3D11_FEATURE)D3D11_FEATURE_D3D11_OPTIONS1, &m_D3D112aOptions, sizeof(m_D3D112aOptions));
		#if CRY_RENDERER_DIRECT3D >= 113
						pDevice->CheckFeatureSupport((D3D11_FEATURE)D3D11_FEATURE_D3D11_OPTIONS2, &m_D3D113aOptions, sizeof(m_D3D113aOptions));
		#if CRY_RENDERER_DIRECT3D >= 114
						pDevice->CheckFeatureSupport((D3D11_FEATURE)D3D11_FEATURE_D3D11_OPTIONS3, &m_D3D114aOptions, sizeof(m_D3D114aOptions));
						pDevice->CheckFeatureSupport((D3D11_FEATURE)D3D11_FEATURE_D3D11_OPTIONS4, &m_D3D114bOptions, sizeof(m_D3D114bOptions));
		#endif
		#endif
		#endif
	#endif
#endif

						// Promote interfaces to the required level
						pDevice->QueryInterface(__uuidof(D3DDevice), (void**)&m_pDevice);
						pContext->QueryInterface(__uuidof(D3DDeviceContext), (void**)&m_pContext);

						{
							DXGIDevice* pDXGIDevice = 0;
							if (SUCCEEDED(pDevice->QueryInterface(__uuidof(DXGIDevice), (void**) &pDXGIDevice)) && pDXGIDevice)
								pDXGIDevice->SetMaximumFrameLatency(MAX_FRAME_LATENCY);
							SAFE_RELEASE(pDXGIDevice);
						}

	#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
						m_outputIndex = gRenDev->CV_r_overrideDXGIOutput > 0 ? gRenDev->CV_r_overrideDXGIOutput : 0;
						if (m_outputIndex)
						{
							if (SUCCEEDED(pAdapter->EnumOutputs(m_outputIndex, &pOutput)) && pOutput)
							{
								// Promote interfaces to the required level
								pOutput->QueryInterface(__uuidof(DXGIOutput), (void**)&m_pOutput);
								SetupMonitorAndGetAdapterDesc(m_pOutput, m_pAdapter, m_adapterDesc);
								break;
							}

							SAFE_RELEASE(pOutput);
							CryLogAlways("Failed to resolve DXGI display for override index %d. Falling back to primary display.", m_outputIndex);
							m_outputIndex = 0;
						}
	#endif
						if (SUCCEEDED(pAdapter->EnumOutputs(0, &pOutput)) && pOutput)
						{
							// Promote interfaces to the required level
							pOutput->QueryInterface(__uuidof(DXGIOutput), (void**)&m_pOutput);
							SetupMonitorAndGetAdapterDesc(m_pOutput, m_pAdapter, m_adapterDesc);
							break;
						}
						else if (r_overrideDXGIAdapter >= 0)
							CryLogAlways("No display connected to DXGI adapter override %d. Adapter cannot be used for rendering.", r_overrideDXGIAdapter);
					}

					// Decrement QueryInterface() increment
					SAFE_RELEASE(m_pOutput);
					SAFE_RELEASE(m_pContext);
					SAFE_RELEASE(m_pDevice);
					SAFE_RELEASE(m_pAdapter);

					// Decrement Create() increment
					SAFE_RELEASE(pOutput);
					SAFE_RELEASE(pContext);
					SAFE_RELEASE(pDevice);
					SAFE_RELEASE(pAdapter);
				}

				if (r_overrideDXGIAdapter >= 0)
					break;

				++nAdapterOrdinal;
			}
		}
	}

	if (!m_pFactory || !m_pAdapter || !m_pDevice || !m_pContext || !m_pOutput)
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "DeviceInfo::CreateDevice() failed");
		Release();
		return false;
	}

	// Decrement Create() increment
	SAFE_RELEASE(pOutput);
	SAFE_RELEASE(pContext);
	SAFE_RELEASE(pDevice);
	SAFE_RELEASE(pAdapter);

	#if CRY_PLATFORM_WINDOWS
	{
		DXGI_MODE_DESC desc;
		memset(&desc, 0, sizeof(desc));
		desc.Width = gcpRendD3D->m_prefMonWidth;
		desc.Height = gcpRendD3D->m_prefMonHeight;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

		DXGI_MODE_DESC match;

		// disable floating point exceptions due to driver bug with floating point exceptions
		SCOPED_DISABLE_FLOAT_EXCEPTIONS();
		if (SUCCEEDED(m_pOutput->FindClosestMatchingMode(&desc, &match, m_pDevice)))
		{
			m_desktopRefreshRate = match.RefreshRate;
		}
	}
	#endif

	HWND hWnd = pCreateWindowCallback ? pCreateWindowCallback() : 0;
	if (!hWnd)
	{
		Release();
		return false;
	}

	ProcessWindowMessages(hWnd);

	{
		FillSwapChainDesc(m_swapChainDesc, backbufferWidth, backbufferHeight, hWnd, windowed);

		if (!windowed)
		{
			DXGI_MODE_DESC match;
			if (SUCCEEDED(m_pOutput->FindClosestMatchingMode(&m_swapChainDesc.BufferDesc, &match, m_pDevice)))
			{
				m_swapChainDesc.BufferDesc = match;
			}
		}

		m_refreshRate = !windowed ? m_swapChainDesc.BufferDesc.RefreshRate : m_desktopRefreshRate;

		IDXGISwapChain* pSwapChain;
		HRESULT hr = m_pFactory->CreateSwapChain(m_pDevice, &m_swapChainDesc, &pSwapChain);
		if (FAILED(hr) || !pSwapChain)
		{
			Release();
			return false;
		}

		// Promote interfaces to the required level
		hr = pSwapChain->QueryInterface(__uuidof(DXGISwapChain), (void**)&m_pSwapChain);
		if (FAILED(hr) || !m_pSwapChain)
		{
			Release();
			return false;
		}

		// Decrement Create() increment
		SAFE_RELEASE(pSwapChain);

#if (CRY_RENDERER_DIRECT3D >= 120)
		if (m_swapChainDesc.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
		{
			m_pSwapChain->SetMaximumFrameLatency(MAX_FRAME_LATENCY);
		}
#endif
	}

	{
		m_pFactory->MakeWindowAssociation(m_swapChainDesc.OutputWindow, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);

		if (pCreateDeviceCallback)
			pCreateDeviceCallback(m_pDevice);
	}

	ProcessWindowMessages(hWnd);

	return IsOk();

#else
	#pragma message("DeviceInfo::CreateDevice not implemented on this platform")
	return false;
#endif
}

void DeviceInfo::SnapSettings()
{
	if (m_swapChainDesc.Windowed)
	{
		m_swapChainDesc.BufferDesc.RefreshRate.Denominator = 0;
		m_swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
		m_swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		m_swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		m_refreshRate = m_desktopRefreshRate;
	}
	else
	{
		DXGI_MODE_DESC desc;
		memset(&desc, 0, sizeof(desc));
		desc.Width = m_swapChainDesc.BufferDesc.Width;
		desc.Height = m_swapChainDesc.BufferDesc.Height;
		desc.Format = m_swapChainDesc.BufferDesc.Format;

		DXGI_MODE_DESC match;
		if (SUCCEEDED(m_pOutput->FindClosestMatchingMode(&desc, &match, m_pDevice)))
		{
			m_swapChainDesc.BufferDesc = match;

			m_refreshRate = match.RefreshRate;
		}
	}
}

void DeviceInfo::ResizeDXGIBuffers()
{
	HRESULT hr = m_pSwapChain->ResizeBuffers(0, m_swapChainDesc.BufferDesc.Width, m_swapChainDesc.BufferDesc.Height, m_swapChainDesc.BufferDesc.Format, m_swapChainDesc.Flags);
	CRY_ASSERT(hr == S_OK);
}

#if defined(SUPPORT_DEVICE_INFO_MSG_PROCESSING)
void DeviceInfo::OnActivate(UINT_PTR wParam, UINT_PTR lParam)
{
	const bool activate = LOWORD(wParam) != 0;
	if (m_activatedMT != activate)
	{
		if (gcpRendD3D->IsFullscreen())
		{
			HWND hWnd = (HWND) gcpRendD3D->GetHWND();
			ShowWindow(hWnd, activate ? SW_RESTORE : SW_MINIMIZE);
		}

		m_activatedMT = activate;
	}

	PushSystemEvent(ESYSTEM_EVENT_ACTIVATE, wParam, lParam);
}

void DeviceInfo::PushSystemEvent(ESystemEvent event, UINT_PTR wParam, UINT_PTR lParam)
{
	#if !defined(_RELEASE) && !defined(STRIP_RENDER_THREAD)
	if (gcpRendD3D->m_pRT && !gcpRendD3D->m_pRT->IsMainThread()) __debugbreak();
	#endif
	CryAutoCriticalSection lock(m_msgQueueLock);
	m_msgQueue.push_back(DeviceInfoInternal::MsgQueueItem(event, wParam, lParam));
}

void DeviceInfo::ProcessSystemEventQueue()
{
	#if !defined(_RELEASE)
	if (gcpRendD3D->m_pRT && !gcpRendD3D->m_pRT->IsRenderThread()) __debugbreak();
	#endif

	m_msgQueueLock.Lock();
	DeviceInfoInternal::MsgQueue localQueue;
	localQueue.swap(m_msgQueue);
	m_msgQueueLock.Unlock();

	for (size_t i = 0; i < localQueue.size(); ++i)
	{
		const DeviceInfoInternal::MsgQueueItem& item = localQueue[i];
		ProcessSystemEvent(item.event, item.wParam, item.lParam);
	}
}

void DeviceInfo::ProcessSystemEvent(ESystemEvent event, UINT_PTR wParam, UINT_PTR lParam)
{
	#if !defined(_RELEASE)
	if (gcpRendD3D->m_pRT && !gcpRendD3D->m_pRT->IsRenderThread()) __debugbreak();
	#endif

	switch (event)
	{
	case ESYSTEM_EVENT_ACTIVATE:
		{
	#if CRY_PLATFORM_WINDOWS
			// disable floating point exceptions due to driver bug with floating point exceptions
			SCOPED_DISABLE_FLOAT_EXCEPTIONS();
	#endif
			const bool activate = LOWORD(wParam) != 0;
			if (m_activated != activate)
			{
				HWND hWnd = (HWND) gcpRendD3D->GetHWND();

				const bool isFullscreen = gcpRendD3D->IsFullscreen();
				if (isFullscreen)
				{
					GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->ClearState(true);

					gcpRendD3D->GetS3DRend().ReleaseBuffers();
					gcpRendD3D->ReleaseBackBuffers(gcpRendD3D->GetBaseDisplayContext());

					//	m_pSwapChain->ResizeTarget(&SwapChainDesc().BufferDesc);
					m_pSwapChain->SetFullscreenState(activate, 0);
					ResizeDXGIBuffers();

					gcpRendD3D->OnD3D11PostCreateDevice(m_pDevice);

					gcpRendD3D->GetS3DRend().OnResolutionChanged();

					if (activate)
					{
						gEnv->pHardwareMouse->GetSystemEventListener()->OnSystemEvent(event, wParam, lParam);
					}
				}

				m_activated = activate;
			}
		}
		break;

	default:
		assert(0);
		break;
	}
}
#endif // #if defined(SUPPORT_DEVICE_INFO_MSG_PROCESSING)

#if CRY_PLATFORM_WINDOWS
void DeviceInfo::EnforceFullscreenPreemption()
{
	if (gRenDev->CV_r_FullscreenPreemption && gcpRendD3D->IsFullscreen())
	{
		HRESULT hr = m_pSwapChain->Present(0, DXGI_PRESENT_TEST);
		if (hr == DXGI_STATUS_OCCLUDED)
		{
			HWND hWnd = (HWND) gcpRendD3D->GetHWND();
			if (m_activated)
				BringWindowToTop(hWnd);
		}
	}
}
#endif // #if CRY_PLATFORM_WINDOWS

#endif // #if defined(SUPPORT_DEVICE_INFO)
