// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if defined(SUPPORT_DEVICE_INFO)

	#if defined(SUPPORT_DEVICE_INFO_MSG_PROCESSING)
namespace DeviceInfoInternal
{
struct MsgQueueItem
{
	MsgQueueItem(ESystemEvent _event, UINT_PTR _wParam, UINT_PTR _lParam) : event(_event), wParam(_wParam), lParam(_lParam) {}
	ESystemEvent event;
	UINT_PTR     wParam;
	UINT_PTR     lParam;
};

typedef std::vector<MsgQueueItem> MsgQueue;
}
	#endif

typedef HRESULT (CALLBACK * OnCreateDeviceCallback)(D3DDevice*);
typedef HWND (*             CreateWindowCallback)();

struct DeviceInfo
{
	DeviceInfo();
	bool IsOk() const { return m_pFactory != nullptr && m_pAdapter != nullptr && m_pDevice != nullptr && m_pContext != nullptr; }
	void Release();

	bool CreateDevice(int zbpp, OnCreateDeviceCallback pCreateDeviceCallback, CreateWindowCallback pCreateWindowCallback);
	
	#if defined(SUPPORT_DEVICE_INFO_MSG_PROCESSING)
	void OnActivate(UINT_PTR wParam, UINT_PTR lParam);
	void PushSystemEvent(ESystemEvent event, UINT_PTR wParam, UINT_PTR lParam);
	void ProcessSystemEventQueue();
	#endif

	// Properties
	DXGIFactory*                        Factory() const                   { return m_pFactory; }
	DXGIAdapter*                        Adapter() const                   { return m_pAdapter; }
	D3DDevice*                          Device() const                    { return m_pDevice; }
	D3DDeviceContext*                   Context() const                   { return m_pContext; }

	const DXGI_ADAPTER_DESC1&           AdapterDesc()                     { return m_adapterDesc; }

	D3D_DRIVER_TYPE                     DriverType() const                { return m_driverType; }
	unsigned int                        CreationFlags() const             { return m_creationFlags; }
	D3D_FEATURE_LEVEL                   FeatureLevel() const              { return m_featureLevel; }
	D3DFormat                           AutoDepthStencilFmt() const       { return m_autoDepthStencilFmt; }

protected:
	#if defined(SUPPORT_DEVICE_INFO_MSG_PROCESSING)
	void ProcessSystemEvent(ESystemEvent event, UINT_PTR wParam, UINT_PTR lParam);
	#endif

protected:
	DXGIFactory*                        m_pFactory;
	DXGIAdapter*                        m_pAdapter;
	D3DDevice*                          m_pDevice;
	D3DDeviceContext*                   m_pContext;

	DXGI_ADAPTER_DESC1                  m_adapterDesc;

	D3D_DRIVER_TYPE                     m_driverType;
	unsigned int                        m_creationFlags;
	D3D_FEATURE_LEVEL                   m_featureLevel;
	D3DFormat                           m_autoDepthStencilFmt;

#if !CRY_RENDERER_OPENGL && !CRY_RENDERER_VULKAN && !CRY_RENDERER_GNM
	#if CRY_RENDERER_DIRECT3D >= 120
		D3D12_FEATURE_DATA_D3D12_OPTIONS    m_D3D120aOptions;
		D3D12_FEATURE_DATA_D3D12_OPTIONS1   m_D3D120bOptions;
		#if NTDDI_WIN10_RS2 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS2)
		D3D12_FEATURE_DATA_D3D12_OPTIONS2   m_D3D120cOptions;
		#if NTDDI_WIN10_RS3 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS3)
		D3D12_FEATURE_DATA_D3D12_OPTIONS3   m_D3D120dOptions;
		#endif
		#endif
	#elif CRY_RENDERER_DIRECT3D >= 110
		D3D11_FEATURE_DATA_D3D11_OPTIONS    m_D3D110aOptions;
		#if CRY_RENDERER_DIRECT3D >= 112
		D3D11_FEATURE_DATA_D3D11_OPTIONS1   m_D3D112aOptions;
		#if CRY_RENDERER_DIRECT3D >= 113
		D3D11_FEATURE_DATA_D3D11_OPTIONS2   m_D3D113aOptions;
		#if CRY_RENDERER_DIRECT3D >= 114
		D3D11_FEATURE_DATA_D3D11_OPTIONS3   m_D3D114aOptions;
		D3D11_FEATURE_DATA_D3D11_OPTIONS4   m_D3D114bOptions;
		#endif
		#endif
		#endif
	#endif
#endif

	bool                                m_activated;
	bool                                m_activatedMT;

	#if defined(SUPPORT_DEVICE_INFO_MSG_PROCESSING)
	CryCriticalSection           m_msgQueueLock;
	DeviceInfoInternal::MsgQueue m_msgQueue;
	#endif
};

#endif // #if defined(SUPPORT_DEVICE_INFO)
